// Prints a symbolized stack trace to stderr when the process crashes (Windows, and Linux with less detail).
//
// Recompiled functions are named after their N64 address (func_800A11E4, ovl1_func_8038A634, ...), or by
// syms/rush2049/names.toml for the few we have worked out (draw_tex_rect, ...), so a trace
// of host frames points straight at the game code that crashed.

#include "crash_handler.h"
#include "debug_probe.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>
#include <map>
#include <string>

#include "ultramodern/ultra64.h"

#pragma comment(lib, "Dbghelp.lib")

namespace {

std::mutex crash_mutex;

// Native thread id -> game thread, recorded by game_thread_name (ultramodern calls it on each new game thread).
std::mutex game_threads_mutex;
std::map<DWORD, const OSThread*> game_threads;

const char* state_name(uint16_t state) {
    switch (state) {
        case 0: return "stopped";
        case 1: return "queued";
        case 2: return "running";
        case 3: return "blocked";
        default: return "?";
    }
}

bool sym_initialized = false;

void init_symbols() {
    if (!sym_initialized) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        sym_initialized = true;
    }
}

// only_game: print just recompiled game frames and the libultra replacements they call, on one line.
void print_trace(CONTEXT* context, HANDLE thread = GetCurrentThread(), bool only_game = false) {
    HANDLE process = GetCurrentProcess();
    init_symbols();

    CONTEXT ctx = *context;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);

    for (int depth = 0; depth < 64; depth++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }
        DWORD64 pc = frame.AddrPC.Offset;
        if (pc == 0) {
            break;
        }

        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        const char* name = SymFromAddr(process, pc, &displacement, symbol) ? symbol->Name : "?";

        if (only_game) {
            if (strncmp(name, "func_", 5) == 0 || strstr(name, "_func_") != nullptr || strstr(name, "_recomp") != nullptr || strstr(name, "musyx") != nullptr ||
                (name[0] == '_' && name[1] == '_' && name[2] == 'o' && name[3] == 's')) {
                fprintf(stderr, " %s", name);
            }
            continue;
        }

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, pc, &line_displacement, &line)) {
            fprintf(stderr, "  #%02d %016llX %s+0x%llX (%s:%lu)\n", depth, (unsigned long long)pc, name,
                    (unsigned long long)displacement, line.FileName, line.LineNumber);
        }
        else {
            fprintf(stderr, "  #%02d %016llX %s+0x%llX\n", depth, (unsigned long long)pc, name,
                    (unsigned long long)displacement);
        }
    }
}

LONG WINAPI crash_filter(EXCEPTION_POINTERS* info) {
    std::lock_guard lock{ crash_mutex };
    const EXCEPTION_RECORD* rec = info->ExceptionRecord;
    fprintf(stderr, "\n[crash] exception 0x%08lX at %p", rec->ExceptionCode, rec->ExceptionAddress);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        fprintf(stderr, " (%s 0x%016llX)", rec->ExceptionInformation[0] == 1 ? "write" : "read",
                (unsigned long long)rec->ExceptionInformation[1]);
    }
    fprintf(stderr, " on thread %lu\n", GetCurrentThreadId());
    print_trace(info->ContextRecord);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

void dump_all_stacks() {
    std::lock_guard lock{ crash_mutex };
    DWORD self = GetCurrentThreadId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    fprintf(stderr, "[stacks]\n");
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == self) {
            continue;
        }
        HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (t == nullptr) {
            continue;
        }
        if (SuspendThread(t) != (DWORD)-1) {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(t, &ctx)) {
                fprintf(stderr, "  %5lu:", te.th32ThreadID);
                {
                    std::lock_guard threads_lock{ game_threads_mutex };
                    auto it = game_threads.find(te.th32ThreadID);
                    if (it != game_threads.end()) {
                        const OSThread* gt = it->second;
                        fprintf(stderr, " [id %d pri %d %s%s]", gt->id, gt->priority, state_name(gt->state),
                                gt->queue == (PTR(PTR(OSThread)))-1 ? " in run queue" : "");
                    }
                }
                print_trace(&ctx, t, true);
                fprintf(stderr, "\n");
            }
            ResumeThread(t);
        }
        CloseHandle(t);
    }
    CloseHandle(snap);
    rush2049::print_probes();
    fflush(stderr);
}

} // namespace

std::string rush2049::game_thread_name(const OSThread* t) {
    {
        std::lock_guard lock{ game_threads_mutex };
        game_threads[GetCurrentThreadId()] = t;
    }
    return "Game Thread " + std::to_string(t->id);
}

void rush2049::install_crash_handler() {
    SetUnhandledExceptionFilter(crash_filter);
}

void rush2049::start_stack_sampler(int interval_seconds, bool stacks) {
    std::thread([interval_seconds, stacks]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
            if (stacks) {
                dump_all_stacks();
            }
            else {
                rush2049::print_probes();
                fflush(stderr);
            }
        }
    }).detach();
}

void rush2049::start_byte_watch(uint32_t vram) {
    std::thread([vram]() {
        uint8_t* rdram;
        while ((rdram = rush2049::probe_get_rdram()) == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        volatile uint8_t* byte = rdram + ((vram & 0x7FFFFFFF) ^ 3);
        uint8_t last = *byte;
        int reports = 0;
        while (reports < 6) {
            uint8_t now = *byte;
            if (now != last) {
                fprintf(stderr, "[watch] %08X: %02X -> %02X\n", vram, last, now);
                dump_all_stacks();
                last = now;
                reports++;
            }
        }
    }).detach();
}

// --hwwatch <vram>: a hardware data breakpoint (x86 debug register 0) on one byte of RDRAM, set on every thread,
// so reads are caught too. Each new host instruction that touches the byte is printed once with its game stack.
namespace {

uintptr_t hw_watch_addr = 0;
std::mutex hw_watch_mutex;
std::map<DWORD64, int> hw_watch_hits;
thread_local bool in_hw_watch_filter = false;

LONG WINAPI hw_watch_filter(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || (info->ContextRecord->Dr6 & 1) == 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    info->ContextRecord->Dr6 = 0;
    // The breakpoint stays armed while this runs, so reading the byte below traps again on this thread. Without
    // this the nested call relocks hw_watch_mutex, which throws out of the handler and kills the process silently.
    if (in_hw_watch_filter) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    in_hw_watch_filter = true;
    std::lock_guard lock{ hw_watch_mutex };
    DWORD64 rip = info->ContextRecord->Rip;
    if (hw_watch_hits[rip]++ == 0 && hw_watch_hits.size() < 64) {
        std::lock_guard crash_lock{ crash_mutex };
        uint8_t value = *reinterpret_cast<volatile uint8_t*>(hw_watch_addr);
        fprintf(stderr, "[hwwatch] value %02X, thread %lu:", value, GetCurrentThreadId());
        print_trace(info->ContextRecord, GetCurrentThread(), true);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    in_hw_watch_filter = false;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void arm_hw_watch() {
    DWORD self = GetCurrentThreadId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == self) {
            continue;
        }
        HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
        if (t == nullptr) {
            continue;
        }
        if (SuspendThread(t) != (DWORD)-1) {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(t, &ctx) && ctx.Dr0 != hw_watch_addr) {
                ctx.Dr0 = hw_watch_addr;
                ctx.Dr7 = (ctx.Dr7 & ~0xF0003ull) | 1 | (3ull << 16);  // local enable, read/write, 1 byte
                SetThreadContext(t, &ctx);
            }
            ResumeThread(t);
        }
        CloseHandle(t);
    }
    CloseHandle(snap);
}

} // namespace

void rush2049::start_hw_watch(uint32_t vram) {
    AddVectoredExceptionHandler(1, hw_watch_filter);
    std::thread([vram]() {
        uint8_t* rdram;
        while ((rdram = rush2049::probe_get_rdram()) == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        hw_watch_addr = reinterpret_cast<uintptr_t>(rdram + ((vram & 0x7FFFFFFF) ^ 3));
        while (true) {
            arm_hw_watch();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();
}

#elif defined(__linux__)

// Linux: the crash trace only. The executable links with -rdynamic, so backtrace_symbols names the recompiled
// functions; the --stacks, --watch and --hwwatch dev flags are Windows only and do nothing here.

#include <csignal>
#include <cstring>
#include <cstdio>
#include <execinfo.h>
#include <unistd.h>

namespace {

void crash_signal(int sig, siginfo_t* info, void*) {
    fprintf(stderr, "\n[crash] signal %d (%s) at address %p\n", sig, strsignal(sig), info->si_addr);
    void* frames[64];
    int depth = backtrace(frames, 64);
    backtrace_symbols_fd(frames, depth, STDERR_FILENO);
    rush2049::print_probes();
    fflush(stderr);
    // SA_RESETHAND put the default action back, so re-raising ends the process the usual way (core dump included).
    raise(sig);
}

} // namespace

void rush2049::install_crash_handler() {
    // Load libgcc's unwinder now: backtrace() would otherwise dlopen it inside the handler.
    void* warmup[1];
    backtrace(warmup, 1);
    struct sigaction sa{};
    sa.sa_sigaction = crash_signal;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT }) {
        sigaction(sig, &sa, nullptr);
    }
}

void rush2049::start_hw_watch(uint32_t) {}
std::string rush2049::game_thread_name(const OSThread* t) { return "Game Thread " + std::to_string(t->id); }
void rush2049::start_stack_sampler(int, bool) {}
void rush2049::start_byte_watch(uint32_t) {}

#else

void rush2049::start_hw_watch(uint32_t) {}
void rush2049::install_crash_handler() {}
std::string rush2049::game_thread_name(const OSThread* t) { return "Game Thread " + std::to_string(t->id); }
void rush2049::start_stack_sampler(int, bool) {}
void rush2049::start_byte_watch(uint32_t) {}

#endif
