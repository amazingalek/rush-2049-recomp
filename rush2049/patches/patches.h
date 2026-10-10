#ifndef __PATCHES_H__
#define __PATCHES_H__

// Section attributes N64Recomp reads in patches.elf (same meaning as in a mod, see ../mod_template/include/modding.h).
#define RECOMP_EXPORT __attribute__((retain, section(".recomp_export")))
#define RECOMP_PATCH __attribute__((retain, section(".recomp_patch")))
#define RECOMP_FORCE_PATCH __attribute__((retain, section(".recomp_force_patch")))
#define RECOMP_DECLARE_EVENT(func) \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
    __attribute__((retain, noinline, weak, used, section(".recomp_event"))) void func {} \
    _Pragma("GCC diagnostic pop")

#include <stdarg.h>
#include "ultra64.h"
#include "patch_helpers.h"

#endif
