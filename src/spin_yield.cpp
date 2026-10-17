// The yield in the game's busy-wait loops (the hooks find_spins.py generates into us.toml, and the audio thread's).
//
// Each loop re-reads a word until another game thread changes it. ultramodern runs one game thread at a time, so
// the loop has to give the CPU away, and it has to give it to whichever thread is ready, not only to one of higher
// priority as ultramodern's yield_self_1ms (and osYieldThread) do. Measured with every display list held 2 s in
// send_dl: the frame loop (thread 7, priority 5) spins at 0x800FD500 on the graphics-task counter (0x8002EB70),
// which only the scheduler (thread 4, priority 12) lowers; the scheduler is blocked in osSendMesg on a full queue
// that thread 6 (priority 4) drains, and thread 6 was ready all along but never ran, so the boot stopped at the
// Midway screen for good. On the console the RCP never takes that long, so the loop is only ever a short wait.
//
// The spinning thread goes back into the run queue by its priority and hands the CPU to the first ready thread that
// isn't itself waiting here, which runs until it blocks or makes an OS call while the spinner, now the higher
// priority, is ready again. The game's idle thread (thread 1) is always ready and never spins, so with nobody else to
// run it gets the CPU and waits in pause_self for the next RCP, VI or timer event, which is what anything the loop
// waits for comes from; yield_self_1ms remains for a run queue with only spinners in it.
//
// Handing it to the head of the queue instead let two spinners keep it between them. At the GAME OVER menu of a
// race the frame loop (thread 7, priority 5) and the music thread (thread 10, priority 10) both wait in
// func_80096130 for the graphics-task counter; each one's yield found the other at the head of the queue, so the idle
// thread never ran again, the scheduler never got the RDP's message that lowers the counter, and the picture and the
// sound stopped for good with a core still busy. It took a slow RDP to put both in the loop at once (Linux under
// lavapipe froze there at every race's end; a Steam Deck sometimes).

#include <algorithm>
#include <vector>

#include "ultramodern/ultramodern.hpp"

extern "C" void yield_self_1ms(RDRAM_ARG1);

// The threads waiting in rush2049_spin_yield. Only the game thread that runs touches it.
static std::vector<PTR(OSThread)> spinners;

namespace {
struct Spinning {
    PTR(OSThread) self;
    explicit Spinning(PTR(OSThread) t) : self(t) { spinners.push_back(t); }
    // Also when the thread is destroyed while it waits (thread_terminated unwinds through here).
    ~Spinning() { spinners.erase(std::find(spinners.begin(), spinners.end(), self)); }
};
}

extern "C" void rush2049_spin_yield(uint8_t* rdram) {
    PTR(OSThread) next_ = ultramodern::thread_queue_peek(PASS_RDRAM ultramodern::running_queue);
    while (next_ != NULLPTR && std::find(spinners.begin(), spinners.end(), next_) != spinners.end()) {
        next_ = TO_PTR(OSThread, next_)->next;
    }
    if (next_ == NULLPTR) {
        yield_self_1ms(PASS_RDRAM1);
        return;
    }
    // ultramodern's swap_to_thread, to that thread whatever its priority.
    ultramodern::thread_queue_remove(PASS_RDRAM ultramodern::running_queue, next_);
    OSThread* next = TO_PTR(OSThread, next_);
    next->queue = NULLPTR;
    PTR(OSThread) self = ultramodern::this_thread();
    ultramodern::thread_queue_insert(PASS_RDRAM ultramodern::running_queue, self);
    TO_PTR(OSThread, self)->state = OSThreadState::QUEUED;
    Spinning spinning{ self };
    ultramodern::resume_thread_and_wait(PASS_RDRAM next);
}
