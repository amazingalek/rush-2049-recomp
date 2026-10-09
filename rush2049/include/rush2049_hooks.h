#ifndef RUSH2049_HOOKS_H
#define RUSH2049_HOOKS_H

#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks inserted into recompiled code by us.toml ([[patches.hook]]). */
void rush2049_inflate_begin(uint8_t* rdram, recomp_context* ctx);
void rush2049_inflate_end(uint8_t* rdram, recomp_context* ctx);

/* ultramodern (scheduling.cpp): let queued game threads run, waking after at most 1 ms. For spin loops. */
void yield_self_1ms(uint8_t* rdram);

/* Debug tracing for bring-up: prints "<ms> <tag> <value>" to stderr. */
void rush2049_trace(const char* tag, uint32_t value);
void rush2049_probe_sched_msg(uint8_t* rdram, uint32_t msg);

#ifdef __cplusplus
}
#endif

#endif
