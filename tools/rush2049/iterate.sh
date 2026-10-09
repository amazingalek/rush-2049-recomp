#!/usr/bin/env bash
# Bring-up loop: regenerate symbols, rerun N64Recomp, rebuild, and run with --boot for $1 seconds (default 30).
# Output goes to rush2049/run.log. Usage: tools/rush2049/iterate.sh [seconds] [--no-recomp]
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
secs="${1:-30}"
cd "$root"
source tools/env.sh
if [ "${2:-}" != "--no-recomp" ]; then
    .venv/Scripts/python -I tools/rush2049/gen_syms.py > /dev/null || exit 1
    (cd rush2049 && rm -rf RecompiledFuncs && mkdir RecompiledFuncs &&
        "$LOCALAPPDATA/Programs/n64-toolchain/n64recomp/N64Recomp.exe" us.toml > recomp.log 2>&1) ||
        { tail -5 rush2049/recomp.log; exit 1; }
fi
ninja -C rush2049/build Rush2049Recompiled > rush2049/build/build.log 2>&1 ||
    { grep -E " error|FAILED|undefined" rush2049/build/build.log | head -20; exit 1; }
cd rush2049
timeout "$secs" ./build/Rush2049Recompiled.exe --boot --stacks > run.log 2>&1
echo "exit $?" >> run.log
