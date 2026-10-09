#ifndef RUSH2049_CRASH_HANDLER_H
#define RUSH2049_CRASH_HANDLER_H

#include <string>

#include "ultramodern/ultra64.h"

namespace rush2049 {
    // Prints a symbolized stack trace to stderr on an unhandled exception.
    void install_crash_handler();
    // Development: every interval, print the bring-up probes and, if stacks, the game frames on every thread's
    // stack (suspends each thread briefly) to stderr.
    void start_stack_sampler(int interval_seconds, bool stacks);
    // Development: spin on one RDRAM byte and dump every thread's game frames when it changes (first 6 changes).
    void start_byte_watch(uint32_t vram);
    // ultramodern threads callback: names a game thread and records it for the stack sampler.
    std::string game_thread_name(const OSThread* t);
}

#endif
