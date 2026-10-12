#!/usr/bin/env bash
# Build the web page from the ROM and publish it on GitHub Pages, https://amazingalek.github.io/rush-2049-recomp/.
# CI has no ROM, so it can't generate the recompiled code the .wasm is made of; the page is published from here, like a
# release. It is the build's Rush2049Recompiled.html as index.html, with its .js, .wasm, .data (the menus' assets and
# the controller mappings, no ROM) and coi_serviceworker.js (Pages can't send the cross-origin headers threads need).
# They are the only commit on the orphan branch gh-pages, force-pushed each time, so the repo holds one copy of the
# page rather than every one published. Pages serves that branch's root (set once in the repo's settings).
#
# It always builds origin/main, on helga (tools/helga.sh), in a copy of its own named publish, kept between runs with
# its build-web/, so a republish rebuilds only what changed. The page comes back here into out/page and is pushed from
# here, with this PC's push access.
# Usage: tools/publish_page.sh [--dry-run]
#   Run from a checkout with roms/ (the ROM and the recomp ROM extract.py makes) and push access to origin. --dry-run
#   builds and stages the page in out/page but pushes nothing: serve that with the README's headers and try it first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
dry_run=false
[ "${1:-}" = "--dry-run" ] && dry_run=true

git fetch -q origin main
bash tools/helga.sh -n publish sync origin/main
bash tools/helga.sh -n publish build Rush2049Recompiled

stage=out/page
built=out/page-build
rm -rf "$stage" "$built"
mkdir -p "$stage"
bash tools/helga.sh -n publish get build-web "$built"
cp "$built"/Rush2049Recompiled.{js,wasm,data} "$built"/coi_serviceworker.js "$stage"/
cp "$built"/Rush2049Recompiled.html "$stage"/index.html
rm -rf "$built"
touch "$stage"/.nojekyll  # served as is, not run through Jekyll
if $dry_run; then
    echo "staged in $root/$stage, nothing pushed"
    exit 0
fi

git -C "$stage" init -q -b gh-pages
git -C "$stage" config core.autocrlf false  # the files exactly as built
git -C "$stage" add -A
git -C "$stage" commit -q -m "The web page, built from $(git rev-parse --short origin/main)"
git -C "$stage" push -q -f "$(git remote get-url origin)" gh-pages
echo "pushed gh-pages; Pages serves it in a minute or two"
