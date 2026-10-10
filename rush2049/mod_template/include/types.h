#ifndef __RUSH2049_TYPES_H__
#define __RUSH2049_TYPES_H__

// libultra's basic types, for MIPS code with no decomp headers to lean on (patches and mods).
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
