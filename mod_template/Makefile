# Builds build/mod.elf; then `RecompModTool mod.toml build` turns it into build/rush2049_hello_mod.nrm.
# clang needs a MIPS backend: on Windows the llvm.org build has none, so use MSYS2's clang64 clang
# (tools/env.sh: make CC="$MIPS_CC" LD="$MIPS_LD").
# make's built-in CC (cc) and LD (ld) would win over ?=, so replace only those defaults.
ifeq ($(origin CC),default)
CC := clang
endif
ifeq ($(origin LD),default)
LD := ld.lld
endif

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/mod.elf

CFLAGS   := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
            -fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset -ffreestanding \
            -ffunction-sections -Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter \
            -Wno-unknown-pragmas -Wno-unused-variable -Wno-missing-braces -Wno-unsupported-floating-point-opt \
            -Werror=section
CPPFLAGS := -D_LANGUAGE_C -DMIPS -I include
LDFLAGS  := -nostdlib -T mod.ld -Map $(BUILD_DIR)/mod.map --unresolved-symbols=ignore-all --emit-relocs -e 0 \
            --no-nmagic -gc-sections

C_SRCS := $(wildcard src/*.c)
C_OBJS := $(C_SRCS:src/%.c=$(BUILD_DIR)/%.o)

$(TARGET): $(C_OBJS) mod.ld
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MP -MF $(@:.o=.d) -c -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

-include $(C_OBJS:.o=.d)

.PHONY: clean
