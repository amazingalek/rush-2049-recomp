#ifndef RUSH2049_CRASH_HANDLER_H
#define RUSH2049_CRASH_HANDLER_H

namespace rush2049 {
    // Prints a symbolized stack trace to stderr on an unhandled exception.
    void install_crash_handler();
    // Development: every interval, print the game frames on every thread's stack to stderr.
    void start_stack_sampler(int interval_seconds);
}

#endif
