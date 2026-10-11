// Forced into every RecompiledFuncs file by headless/CMakeLists.txt: the functions the recompiled code calls that
// N64Recomp leaves out of funcs.h, the ones us.toml ignores (src/gsm_decoder.cpp replaces them) and runtime ones
// missing from its libultra list. C lets a call go without a declaration and x86-64 doesn't mind the `int f()` it
// guesses, but WebAssembly checks signatures when it links and turns such a call into a trap.
#ifndef RUSH2049_RECOMPILED_EXTERNS_H
#define RUSH2049_RECOMPILED_EXTERNS_H

#include "recomp.h"

void func_800277D0(uint8_t* rdram, recomp_context* ctx);
void func_800278C4(uint8_t* rdram, recomp_context* ctx);
void func_80027B44(uint8_t* rdram, recomp_context* ctx);
void __ll_lshift_recomp(uint8_t* rdram, recomp_context* ctx);
void osYieldThread_recomp(uint8_t* rdram, recomp_context* ctx);
void osPfsRepairId_recomp(uint8_t* rdram, recomp_context* ctx);

#endif
