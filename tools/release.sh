#!/usr/bin/env bash
# Build a release from a local ROM, for Windows here and for Linux on $BUILD_HOST, and publish both as a GitHub
# release. CI has no ROM (none is ever uploaded), so it can't link the game; releases are cut on a machine with the
# ROM instead.
# Usage: BUILD_HOST=<host> tools/release.sh [--dry-run]
#   The tag is v<version_string> from src/main.cpp, so bump that first, and write the release's notes beside it in
#   releases/v<version>.md: what a player gets, not a list of merged PRs (most are web support and other games'
#   pages, which the zip doesn't have). Without that file it refuses. Needs a clean tree at origin/main, the
#   setup from the README's build steps and CLAUDE.md (submodules, runtime patches, extract.py's roms/, the
#   .venv, the toolchain) and gh signed in. --dry-run builds and packs but publishes nothing and skips the checks on
#   the tree and the tag.
# The Linux build runs through tools/remote_build.sh (a copy named release-linux on $BUILD_HOST, which gets roms/ too)
# in tools/docker/Dockerfile.linux, Ubuntu 22.04, so it needs glibc 2.35 or newer. It carries the libSDL2 it was built
# with beside the binary (RUNPATH $ORIGIN), as the Windows zip carries SDL2.dll, and is a .tar.gz so the binary stays
# executable.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
dry_run=false
[ "${1:-}" = "--dry-run" ] && dry_run=true

version=$(sed -n 's/^const std::string version_string = "\(.*\)";$/\1/p' src/main.cpp)
[ -n "$version" ] || { echo "no version_string in src/main.cpp"; exit 1; }
tag="v$version"
notes="releases/$tag.md"
[ -s "$notes" ] || { echo "no $notes: write the release notes for players first"; exit 1; }
name="Rush2049Recompiled-$tag-windows-x64"
linux="Rush2049Recompiled-$tag-linux-x64"
[ -n "${BUILD_HOST:-}" ] || { echo "set BUILD_HOST to the Linux host to build the Linux release on"; exit 1; }

if ! $dry_run; then
    # Submodule worktrees are dirty with the runtime patches applied; their pinned commits still count.
    [ -z "$(git status --porcelain --ignore-submodules=dirty)" ] || { echo "the tree has uncommitted changes"; exit 1; }
    git fetch -q origin main
    [ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || { echo "HEAD is not origin/main"; exit 1; }
    ! gh release view "$tag" > /dev/null 2>&1 || { echo "release $tag exists: bump version_string first"; exit 1; }
fi

source tools/env.sh
# The recompiled code is regenerated from the committed symbols, so it matches this commit and not a dev build's.
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl

# Its own build directory, so the dev build in build keeps its RelWithDebInfo configuration.
build=build-release
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
    -DPATCHES_C_COMPILER="$MIPS_CC" -DPATCHES_LD="$MIPS_LD" > "$build.log" 2>&1 || { tail -20 "$build.log"; exit 1; }
ninja -C "$build" Rush2049Recompiled > "$build/build.log" 2>&1 ||
    { grep -E " error|FAILED|undefined" "$build/build.log" | head -20; exit 1; }

# The game finds assets/ and recompcontrollerdb.txt in its working directory, so everything sits beside the exe.
stage="out/$name"
rm -rf "$stage" "out/$name.zip"
mkdir -p "$stage"
cp "$build"/{Rush2049Recompiled.exe,SDL2.dll,dxil.dll,dxcompiler.dll} recompcontrollerdb.txt COPYING "$stage"/
cp -r assets "$stage"/
(cd out && ../.venv/Scripts/python -I -m zipfile -c "$name.zip" "$name")
echo "out/$name.zip ($(du -h "out/$name.zip" | cut -f1))"

# Linux: HEAD (with any uncommitted changes, for a dry run) built from scratch in the copy's build-release.
rb() { bash tools/remote_build.sh -n release-linux -d tools/docker/Dockerfile.linux "$@"; }
rb sync > /dev/null
rb run "set -e
    bash tools/recompile.sh -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    rm -rf build-release out
    cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \\
        -DPATCHES_C_COMPILER=clang -DPATCHES_LD=ld.lld -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > build-release.log 2>&1 ||
        { tail -20 build-release.log; exit 1; }
    ninja -C build-release Rush2049Recompiled > build-release/build.log 2>&1 ||
        { grep -E ' error|FAILED|undefined' build-release/build.log | head -20; exit 1; }
    mkdir -p out/$linux
    cp build-release/Rush2049Recompiled recompcontrollerdb.txt COPYING out/$linux/
    cp -L /usr/local/lib/libSDL2-2.0.so.0 out/$linux/
    patchelf --set-rpath '\$ORIGIN' out/$linux/Rush2049Recompiled
    cp -r assets out/$linux/
    tar -C out -czf out/$linux.tar.gz $linux"
rb get out out
echo "out/$linux.tar.gz ($(du -h "out/$linux.tar.gz" | cut -f1))"

$dry_run && { echo "dry run: $tag not published"; exit 0; }
gh release create "$tag" "out/$name.zip" "out/$linux.tar.gz" --target "$(git rev-parse HEAD)" --title "Rush 2049 Recompiled $tag" \
    --notes-file "$notes"
