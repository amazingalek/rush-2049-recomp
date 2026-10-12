#!/usr/bin/env bash
# Applies this game's runtime patches, runtime-patches/<Submodule>-*.patch, to the matching submodule under
# lib, in order of name. The submodules are amazingalek's forks, checked out on their main branches, so the
# patches go on top of the web support (CLAUDE.md, "The web target"). Patches already applied are skipped.
# A submodule nested one level down (N64ModernRuntime/N64Recomp), or in a submodule's src/contrib
# (rt64/src/contrib/plume), is found by its name too; a patch for one inside a submodule that isn't checked out is
# skipped. Run after cloning or updating submodules, and again after pulling new patches.
# Which patches are applied is found on a scratch copy of the files they touch: they are reversed there newest first,
# so a patch whose lines a later one changed reverses once that one is off, and each one that reverses was applied.
# A checkout can hold a later patch without an earlier one, if a patch was added or edited on main after the others
# were applied. Then the applied ones after the first missing one come off and everything from it goes
# on in order, so the tree is the same as a fresh checkout's. The whole set is tried on the copy first, and a patch
# that doesn't apply (the submodule has other changes, or an applied patch was since edited) stops the script before
# the submodule is touched.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
patches=("$root"/runtime-patches/*.patch)
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
export GIT_CEILING_DIRECTORIES="$scratch"  # git apply in a copy works on its files, never on a repo above it
sub_dir() {
    local dir
    for dir in "$root/lib/$1" "$root"/lib/*/"$1" "$root"/lib/*/src/contrib/"$1"; do
        if [[ -e "$dir/.git" ]]; then  # an uninitialised submodule is an empty folder
            echo "$dir"
            return
        fi
    done
}
subs=()
declare -A sub_patches
for patch in "${patches[@]}"; do
    name="$(basename "$patch")"
    sub="${name%%-[0-9]*}"
    [[ -v sub_patches[$sub] ]] || subs+=("$sub")
    sub_patches[$sub]+="$patch"$'\n'
done
for sub in "${subs[@]}"; do
    mapfile -t list < <(printf %s "${sub_patches[$sub]}")
    dir="$(sub_dir "$sub")"
    if [[ -z "$dir" ]]; then
        for patch in "${list[@]}"; do echo "not checked out, skipped: $(basename "$patch")"; done
        continue
    fi
    copy="$scratch/$sub"
    mkdir -p "$copy"
    sed -n 's#^\(---\|+++\) [ab]/##p' "${list[@]}" | sort -u | while read -r path; do
        if [[ -e "$dir/$path" ]]; then (cd "$dir" && cp --parents -p "$path" "$copy/"); fi
    done
    applied=()
    for ((k = ${#list[@]} - 1; k >= 0; k--)); do
        if (cd "$copy" && git apply --whitespace=nowarn --reverse "${list[$k]}" 2>/dev/null); then applied[$k]=1; fi
    done
    first=${#list[@]}
    for ((k = 0; k < ${#list[@]}; k++)); do
        if [[ -z "${applied[$k]:-}" ]]; then first=$k; break; fi
    done
    for ((k = 0; k < ${#list[@]}; k++)); do
        if ! out="$(cd "$copy" && git apply --whitespace=nowarn "${list[$k]}" 2>&1)"; then
            echo "$out" >&2
            echo "$(basename "${list[$k]}") doesn't apply to ${dir#"$root"/}: it has changes of its own, or a patch" \
                "applied there was edited since. Nothing was changed; a clean checkout of it takes every patch." >&2
            exit 1
        fi
    done
    for ((k = 0; k < first; k++)); do echo "already applied: $(basename "${list[$k]}")"; done
    for ((k = ${#list[@]} - 1; k > first; k--)); do
        if [[ -n "${applied[$k]:-}" ]]; then
            git -C "$dir" apply --whitespace=nowarn --reverse "${list[$k]}"
            echo "taken off, to go back on in order: $(basename "${list[$k]}")"
        fi
    done
    for ((k = first; k < ${#list[@]}; k++)); do
        git -C "$dir" apply --whitespace=nowarn "${list[$k]}"
        echo "applied: $(basename "${list[$k]}")"
    done
done
