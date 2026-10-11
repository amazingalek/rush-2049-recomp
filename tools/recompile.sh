#!/usr/bin/env bash
# Generates the recompiled code from roms/rush2049.us.recomp.z64 (tools/extract.py) and the symbols in syms/:
# RecompiledFuncs/ with N64Recomp (us.toml) and rsp/musyx.cpp with RSPRecomp (musyx.toml). Both are wiped and
# written again, and the game's CMake configure needs them. The recompilers are the N64Recomp the runtime pins
# (lib/N64ModernRuntime/N64Recomp), built into build/n64recomp the first time; arguments go to that configure.
#
# Usage: tools/recompile.sh [cmake arguments, e.g. -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl]
set -euo pipefail
cd "$(dirname "$0")/.."
bin=build/n64recomp
mkdir -p build
if [[ ! -f "$bin/build.ninja" ]]; then
    cmake -S lib/N64ModernRuntime/N64Recomp -B "$bin" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@" > "$bin.log" 2>&1 ||
        { tail -20 "$bin.log"; exit 1; }
fi
ninja -C "$bin" N64RecompCLI RSPRecomp >> "$bin.log" 2>&1 || { tail -20 "$bin.log"; exit 1; }
rm -rf RecompiledFuncs && mkdir RecompiledFuncs
"$bin/N64Recomp" us.toml > recomp.log 2>&1 || { tail -5 recomp.log; exit 1; }
mkdir -p rsp && rm -f rsp/musyx.cpp
"$bin/RSPRecomp" musyx.toml > /dev/null
