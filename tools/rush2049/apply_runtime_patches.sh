#!/usr/bin/env bash
# Applies rush2049/runtime-patches/<Submodule>-*.patch to the matching submodule under rush2049/lib, skipping
# patches that are already applied. Run after cloning or updating submodules.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
for patch in "$root"/rush2049/runtime-patches/*.patch; do
    name="$(basename "$patch")"
    sub="${name%%-[0-9]*}"
    dir="$root/rush2049/lib/$sub"
    if git -C "$dir" apply --reverse --check "$patch" 2>/dev/null; then
        echo "already applied: $name"
    else
        git -C "$dir" apply "$patch"
        echo "applied: $name"
    fi
done
