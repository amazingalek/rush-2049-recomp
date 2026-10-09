// Prints a symbolized stack trace to stderr when the process crashes (Windows only).
//
// Recompiled functions are named after their N64 address (func_800A11E4, ovl1_func_8038A634, ...), so a trace
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

#else

void rush2049::install_crash_handler() {}
std::string rush2049::game_thread_name(const OSThread* t) { return "Game Thread " + std::to_string(t->id); }
void rush2049::start_stack_sampler(int, bool) {}

#endif
