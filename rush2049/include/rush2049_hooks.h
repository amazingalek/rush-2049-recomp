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

#ifdef __cplusplus
}
#endif

#endif
