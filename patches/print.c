#include "patches.h"
#include "misc_funcs.h"

// Mods import this as RECOMP_IMPORT("*", int recomp_printf(const char* fmt, ...)).
RECOMP_EXPORT int recomp_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = recomp_vprintf(fmt, args);
    va_end(args);
    return ret;
}
