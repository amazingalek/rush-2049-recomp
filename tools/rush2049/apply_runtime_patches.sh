#!/usr/bin/env bash
# Applies the runtime patches, <Submodule>-*.patch, to the matching submodule under rush2049/lib: first
# web/runtime-patches/ (the web target, game-agnostic, so each applies to the clean upstream submodule), then
# rush2049/runtime-patches/ (this game's) on top, each folder in order of name. Patches already applied are skipped.
# A submodule nested one level down (N64ModernRuntime/N64Recomp), or in a submodule's src/contrib
# (rt64/src/contrib/plume), is found by its name too; a patch for one inside a submodule that isn't checked out is
# skipped. Run after cloning or updating submodules.
# A later patch can change lines an earlier one added, and then the earlier one no longer reverses on its own. So the
# last patch of a submodule, in that order, that reverses cleanly marks it and every patch before it as applied.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
patches=("$root"/web/runtime-patches/*.patch "$root"/rush2049/runtime-patches/*.patch)
declare -A applied_through
sub_dir() {
    local dir
    for dir in "$root/rush2049/lib/$1" "$root"/rush2049/lib/*/"$1" "$root"/rush2049/lib/*/src/contrib/"$1"; do
        if [[ -d "$dir" ]]; then
            echo "$dir"
            return
        fi
    done
}
for i in "${!patches[@]}"; do
    name="$(basename "${patches[$i]}")"
    sub="${name%%-[0-9]*}"
    dir="$(sub_dir "$sub")"
    if [[ -n "$dir" ]] && git -C "$dir" apply --reverse --check "${patches[$i]}" 2>/dev/null; then
        applied_through[$sub]=$i
    fi
done
for i in "${!patches[@]}"; do
    patch="${patches[$i]}"
    name="$(basename "$patch")"
    sub="${name%%-[0-9]*}"
    last="${applied_through[$sub]:--1}"
    dir="$(sub_dir "$sub")"
    if [[ -z "$dir" ]]; then
        echo "not checked out, skipped: $name"
    elif (( i <= last )); then
        echo "already applied: $name"
    else
        git -C "$dir" apply "$patch"
        echo "applied: $name"
    fi
done
