#!/usr/bin/env bash
# Build the web page from the ROM on this PC and publish it on GitHub Pages, https://amazingalek.github.io/rush-2049-recomp/.
# CI has no ROM, so it can't generate the recompiled code the .wasm is made of; the page is published from here, like a
# release. It is the build's Rush2049Recompiled.html as index.html, with its .js, .wasm, .data (the menus' assets and
# the controller mappings, no ROM) and coi_serviceworker.js (Pages can't send the cross-origin headers threads need).
# They are the only commit on the orphan branch gh-pages, force-pushed each time, so the repo holds one copy of the
# page rather than every one published. Pages serves that branch's root (set once in the repo's settings).
#
# It always builds origin/main, in a worktree of its own beside the main checkout (<main checkout>-publish), kept
# between runs with its build-web/: a fresh checkout has to build the page from nothing, ~16 minutes here, most of it
# the shader chain (dxc and naga, which ccache can't cache), and a kept one rebuilds only what changed: measured
# 2026-10-12 on a busy host, 15 s with nothing changed, 2 minutes for a game source (mostly the link at -O3), 7 for an
# rt64 bump. The first run makes that worktree, copying the submodules from the checkout it is run from (half an hour
# here, once) and its roms/. A run moves it to origin/main, resets the submodules only if their commits
# changed, and reapplies the runtime patches, so the files nothing changed keep their times.
# Usage: tools/publish_page.sh [--dry-run]
#   Run from any checkout of this repo with the setup from the README's build steps and CLAUDE.md (submodules, roms/),
#   with emsdk's emcmake on PATH, naga on PATH or in $NAGA, and push access to origin. --dry-run builds and stages the
#   page in <worktree>/out/page but pushes nothing: serve that with the README's headers and try it first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
dry_run=false
[ "${1:-}" = "--dry-run" ] && dry_run=true
command -v emcmake > /dev/null || { echo "emcmake isn't on PATH: put emsdk's upstream/emscripten there"; exit 1; }

main="$(git worktree list --porcelain | sed -n '1s/^worktree //p')"
pub="$main-publish"
mkdir "$pub.lock" 2> /dev/null ||
    { echo "$pub.lock exists: another publish is running, or one was stopped (then delete it)"; exit 1; }
trap 'rmdir "$pub.lock"' EXIT

git fetch -q origin main
reset_submodules() {  # patches off the named submodules, then every one (nested ones once their parents have moved) on its commit
    for sub in "$@"; do
        git -C "$sub" submodule foreach --recursive -q 'git reset -q --hard && git clean -fdq'
        git -C "$sub" reset -q --hard && git -C "$sub" clean -fdq
    done
    for _ in 1 2; do
        git submodule sync -q --recursive
        git -c core.longpaths=true submodule update -q --init --recursive
    done
}
if [ ! -e "$pub/.git" ]; then
    git worktree add -q --detach "$pub" origin/main
    (cd "$pub" && bash tools/copy_submodules.sh "$root")
else
    git -C "$pub" checkout -q --detach origin/main
    # Only the submodules main moved (a + in the status) are reset: a reset rewrites the files the patches touched,
    # and a submodule nested in one moves only with it.
    moved=($(git -C "$pub" submodule status | sed -n 's/^[-+][0-9a-f]* \([^ ]*\).*/\1/p'))
    if [ ${#moved[@]} -gt 0 ]; then (cd "$pub" && reset_submodules "${moved[@]}"); fi
fi
mkdir -p "$pub/roms"
cp -u "$root"/roms/*.z64 "$pub/roms/"
cd "$pub"
# An applied patch that main has since edited makes the script stop before it touches anything: start them over.
bash tools/apply_runtime_patches.sh > /dev/null ||
    { reset_submodules $(git config -f .gitmodules --get-regexp '\.path$' | cut -d' ' -f2) &&
        bash tools/apply_runtime_patches.sh > /dev/null; }

# The recompiled code is regenerated from the committed symbols, so it matches this commit and not a dev build's.
# With clang-cl from env.sh, in a subshell: its INCLUDE and LIB are for the desktop build.
(source tools/env.sh && bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl)

build=build-web
if [ ! -f "$build/build.ninja" ]; then  # after that, ninja reruns CMake itself when a CMakeLists.txt changes
    emcmake cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release ${NAGA:+-DRT64_NAGA_EXECUTABLE="$NAGA"} \
        > "$build.log" 2>&1 || { tail -20 "$build.log"; rm -f "$build/build.ninja"; exit 1; }
fi
ninja -C "$build" Rush2049Recompiled > "$build/build.log" 2>&1 ||
    { grep -E " error|FAILED|undefined" "$build/build.log" | head -20; exit 1; }

stage=out/page
rm -rf "$stage"
mkdir -p "$stage"
cp "$build"/Rush2049Recompiled.{js,wasm,data} "$build"/coi_serviceworker.js "$stage"/
cp "$build"/Rush2049Recompiled.html "$stage"/index.html
touch "$stage"/.nojekyll  # served as is, not run through Jekyll
if $dry_run; then
    echo "staged in $pub/$stage, nothing pushed"
    exit 0
fi

git -C "$stage" init -q -b gh-pages
git -C "$stage" config core.autocrlf false  # the files exactly as built
git -C "$stage" add -A
git -C "$stage" commit -q -m "The web page, built from $(git rev-parse --short HEAD)"
git -C "$stage" push -q -f "$(git remote get-url origin)" gh-pages
echo "pushed gh-pages; Pages serves it in a minute or two"
