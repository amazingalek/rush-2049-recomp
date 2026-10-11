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
// The spinning thread goes to the back of the run queue and the thread at its head runs until it blocks or makes an
// OS call while the spinner, now the higher priority, is ready again. The game's idle thread (thread 1) is always
// ready, so with nobody else to run it gets the CPU and waits in pause_self for the next RCP, VI or timer event,
// which is what anything the loop waits for comes from; yield_self_1ms remains for a run queue that is empty.

#include "ultramodern/ultramodern.hpp"

extern "C" void yield_self_1ms(RDRAM_ARG1);

extern "C" void rush2049_spin_yield(uint8_t* rdram) {
    if (ultramodern::thread_queue_empty(PASS_RDRAM ultramodern::running_queue)) {
        yield_self_1ms(PASS_RDRAM1);
        return;
    }
    // ultramodern's swap_to_thread, to the head of the queue whatever its priority.
    OSThread* next = TO_PTR(OSThread, ultramodern::thread_queue_pop(PASS_RDRAM ultramodern::running_queue));
    ultramodern::thread_queue_insert(PASS_RDRAM ultramodern::running_queue, ultramodern::this_thread());
    TO_PTR(OSThread, ultramodern::this_thread())->state = OSThreadState::QUEUED;
    ultramodern::resume_thread_and_wait(PASS_RDRAM next);
}
