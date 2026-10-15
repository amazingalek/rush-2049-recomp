#!/usr/bin/env bash
# Build a Windows release from a local ROM and publish it as a GitHub release. CI has no ROM (none is ever uploaded),
# so it can't link the exe; releases are cut on a machine with the ROM instead.
# Usage: tools/release.sh [--dry-run]
#   The tag is v<version_string> from src/main.cpp, so bump that first, and write the release's notes beside it in
#   releases/v<version>.md: what a player gets, not a list of merged PRs (most are web support and other games'
#   pages, which the zip doesn't have). Without that file it refuses. Needs a clean tree at origin/main, the
#   setup from the README's build steps and CLAUDE.md (submodules, runtime patches, extract.py's roms/, the
#   .venv, the toolchain) and gh signed in. --dry-run builds and zips but publishes nothing and skips the checks on
#   the tree and the tag.
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
# Release's own flags plus debug info, which doesn't change the code: the .pdb ships beside the exe, so a crash's
# trace in the player's rush2049.log names the game function it crashed in (src/crash_handler.cpp). The flags are
# written -X, not /X, which Git Bash would turn into a path.
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -Ob2 -DNDEBUG -Z7" -DCMAKE_CXX_FLAGS_RELEASE="-O2 -Ob2 -DNDEBUG -Z7" \
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-INCREMENTAL:NO -DEBUG -OPT:REF -OPT:ICF" \
    -DPATCHES_C_COMPILER="$MIPS_CC" -DPATCHES_LD="$MIPS_LD" > "$build.log" 2>&1 || { tail -20 "$build.log"; exit 1; }
ninja -C "$build" Rush2049Recompiled > "$build/build.log" 2>&1 ||
    { grep -E " error|FAILED|undefined" "$build/build.log" | head -20; exit 1; }

# The game finds assets/ and recompcontrollerdb.txt in its working directory, so everything sits beside the exe.
stage="out/$name"
rm -rf "$stage" "out/$name.zip"
mkdir -p "$stage"
cp "$build"/{Rush2049Recompiled.exe,Rush2049Recompiled.pdb,SDL2.dll,dxil.dll,dxcompiler.dll} recompcontrollerdb.txt COPYING "$stage"/
cp -r assets "$stage"/
(cd out && ../.venv/Scripts/python -I -m zipfile -c "$name.zip" "$name")
echo "out/$name.zip ($(du -h "out/$name.zip" | cut -f1))"

$dry_run && { echo "dry run: $tag not published"; exit 0; }
gh release create "$tag" "out/$name.zip" --target "$(git rev-parse HEAD)" --title "Rush 2049 Recompiled $tag" \
    --notes-file "$notes"
