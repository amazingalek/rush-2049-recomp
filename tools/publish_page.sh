#!/usr/bin/env bash
# Build the web page from the ROM on this PC and publish it on GitHub Pages, https://amazingalek.github.io/rush-2049-recomp/.
# CI has no ROM, so it can't generate the recompiled code the .wasm is made of; the page is published from here, like a
# release. It is the build's Rush2049Recompiled.html as index.html, with its .js, .wasm, .data (the menus' assets and
# the controller mappings, no ROM) and coi_serviceworker.js (Pages can't send the cross-origin headers threads need).
# They are the only commit on the orphan branch gh-pages, force-pushed each time, so the repo holds one copy of the
# page rather than every one published. Pages serves that branch's root (set once in the repo's settings).
# Usage: tools/publish_page.sh [--dry-run]
#   Needs a clean tree at origin/main, the setup from the README's build steps and CLAUDE.md, emsdk's emcmake on PATH,
#   naga on PATH or in $NAGA, and push access to origin. --dry-run builds and stages the page in out/page but pushes
#   nothing and skips the two checks: serve out/page with the README's headers and try it first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
dry_run=false
[ "${1:-}" = "--dry-run" ] && dry_run=true

if ! $dry_run; then
    # Submodule worktrees are dirty with the runtime patches applied; their pinned commits still count.
    [ -z "$(git status --porcelain --ignore-submodules=dirty)" ] || { echo "the tree has uncommitted changes"; exit 1; }
    git fetch -q origin main
    [ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || { echo "HEAD is not origin/main"; exit 1; }
fi
command -v emcmake > /dev/null || { echo "emcmake isn't on PATH: put emsdk's upstream/emscripten there"; exit 1; }

# The recompiled code is regenerated from the committed symbols, so it matches this commit and not a dev build's.
# With clang-cl from env.sh, in a subshell: its INCLUDE and LIB are for the desktop build.
(source tools/env.sh && bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl)

build=build-web
emcmake cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release ${NAGA:+-DRT64_NAGA_EXECUTABLE="$NAGA"} \
    > "$build.log" 2>&1 || { tail -20 "$build.log"; exit 1; }
ninja -C "$build" Rush2049Recompiled > "$build/build.log" 2>&1 ||
    { grep -E " error|FAILED|undefined" "$build/build.log" | head -20; exit 1; }

stage=out/page
rm -rf "$stage"
mkdir -p "$stage"
cp "$build"/Rush2049Recompiled.{js,wasm,data} "$build"/coi_serviceworker.js "$stage"/
cp "$build"/Rush2049Recompiled.html "$stage"/index.html
touch "$stage"/.nojekyll  # served as is, not run through Jekyll
if $dry_run; then
    echo "staged in $stage, nothing pushed"
    exit 0
fi

git -C "$stage" init -q -b gh-pages
git -C "$stage" config core.autocrlf false  # the files exactly as built
git -C "$stage" add -A
git -C "$stage" commit -q -m "The web page, built from $(git rev-parse --short HEAD)"
git -C "$stage" push -q -f "$(git remote get-url origin)" gh-pages
echo "pushed gh-pages; Pages serves it in a minute or two"
