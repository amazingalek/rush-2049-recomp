#!/usr/bin/env bash
# Applies rush2049/runtime-patches/<Submodule>-*.patch to the matching submodule under rush2049/lib, in order,
# skipping patches that are already applied. Run after cloning or updating submodules.
# A later patch can change lines an earlier one added, and then the earlier one no longer reverses on its own. So the
# newest patch of a submodule that reverses cleanly marks it and every patch before it as applied.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
patches=("$root"/rush2049/runtime-patches/*.patch)
declare -A applied_through
for patch in "${patches[@]}"; do
    name="$(basename "$patch")"
    sub="${name%%-[0-9]*}"
    if git -C "$root/rush2049/lib/$sub" apply --reverse --check "$patch" 2>/dev/null; then
        applied_through[$sub]="$name"
    fi
done
for patch in "${patches[@]}"; do
    name="$(basename "$patch")"
    sub="${name%%-[0-9]*}"
    last="${applied_through[$sub]:-}"
    if [[ -n "$last" && ! "$name" > "$last" ]]; then
        echo "already applied: $name"
    else
        git -C "$root/rush2049/lib/$sub" apply "$patch"
        echo "applied: $name"
    fi
done
