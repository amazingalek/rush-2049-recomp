#!/usr/bin/env bash
# Build the web page from the ROM and publish it in its folder of the hub, https://n64hub.com/rush-2049/.
# CI has no ROM, so it can't generate the recompiled code the .wasm is made of; the page is published from a machine
# with the ROM, like a release. It is the build's Rush2049Recompiled.html as index.html, with its .js, .wasm, .data (the menus' assets and
# the controller mappings, no ROM) and coi_serviceworker.js (Pages can't send the cross-origin headers threads need).
# They go into the folder rush-2049/ of the hub (n64hub/n64hub.github.io's gh-pages), the games' page at its top, with
# N64Recomp-browser's tools/publish_page.sh --into, as every game's page is (its tools/publish_pages.sh does the same
# with this game's name). This repo's own Pages, at https://n64hub.com/rush-2049-recomp/, only sends a player there.
#
# It always builds origin/main, on $BUILD_HOST (tools/remote_build.sh), in a copy of its own named publish, kept between
# runs with its build-web/, so a republish rebuilds only what changed. The page comes back into out/page and is pushed
# from this checkout, with push access to the hub.
# Usage: BUILD_HOST=<host> tools/publish_page.sh [--dry-run]
#   Run from a checkout with roms/ (the ROM and the recomp ROM extract.py makes) and push access to the hub. --dry-run
#   builds and stages the page in out/page but pushes nothing: serve that with the README's headers and try it first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
dry_run=false
[ "${1:-}" = "--dry-run" ] && dry_run=true

git fetch -q origin main
bash tools/remote_build.sh -n publish sync origin/main
bash tools/remote_build.sh -n publish build Rush2049Recompiled

stage=out/page
built=out/page-build
rm -rf "$stage" "$built"
mkdir -p "$stage"
bash tools/remote_build.sh -n publish get build-web "$built"
cp "$built"/Rush2049Recompiled.{js,wasm,data} "$built"/coi_serviceworker.js "$stage"/
cp "$built"/Rush2049Recompiled.html "$stage"/index.html
touch "$stage"/.nojekyll  # served as is, not run through Jekyll
if $dry_run; then
    rm -rf "$built"
    echo "staged in $root/$stage, nothing pushed"
    exit 0
fi

bash lib/N64Recomp-browser/tools/publish_page.sh "$built" Rush2049Recompiled n64hub/n64hub.github.io \
    "main $(git rev-parse --short origin/main)" --into rush-2049
rm -rf "$built"
