# MIPS build of the patches library (patches.elf). CMakeLists.txt runs this, then N64Recomp on
# ../patches.toml, which turns patches.elf into RecompiledPatches/patches.c and patches.bin for the executable.
# On Windows the llvm.org clang has no MIPS backend: pass CC and LD (tools/env.sh sets MIPS_CC and MIPS_LD).
TARGET = patches.elf

# make's built-in CC (cc) and LD (ld) would win over ?=, so replace only those defaults.
ifeq ($(origin CC),default)
CC := clang
endif
ifeq ($(origin LD),default)
LD := ld.lld
endif

CFLAGS   := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
			-fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset -funsigned-char \
			-ffreestanding -Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter \
			-Wno-unknown-pragmas -Wno-unused-variable -Wno-missing-braces -Wno-unsupported-floating-point-opt
CPPFLAGS := -D_LANGUAGE_C -DMIPS -I ../mod_template/include
LDFLAGS  := -nostdlib -T patches.ld -T syms.ld -Map patches.map --unresolved-symbols=ignore-all --emit-relocs -e 0

C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:.c=.o)
C_DEPS := $(C_SRCS:.c=.d)

$(TARGET): $(C_OBJS) patches.ld syms.ld
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(C_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MP -MF $(@:.o=.d) -c -o $@

clean:
	rm -rf $(C_OBJS) $(TARGET) $(C_DEPS) patches.map

-include $(C_DEPS)

.PHONY: clean
