#ifndef __MISC_FUNCS_H__
#define __MISC_FUNCS_H__

#include "patch_helpers.h"

// printf to the console; `args` is the MIPS va_list (src/mod_exports.cpp).
DECLARE_FUNC(int, recomp_vprintf, const char* fmt, va_list args);

#endif
