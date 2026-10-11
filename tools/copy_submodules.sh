#!/usr/bin/env bash
# Gives this worktree submodules of its own, copied from another checkout of this repo instead of cloned.
#
#   bash tools/copy_submodules.sh <other worktree>   copy every submodule from there, then reset and update them
#   bash tools/copy_submodules.sh                    only make the submodules already here this worktree's own
#
# Why a script: a submodule's working tree has a .git *file*, "gitdir: <path>", pointing at its git dir in the
# superproject's (in a worktree, <repo>/.git/worktrees/<name>/modules/<path>). Copied by hand, that link still points
# into the other worktree, so the two share one git dir, and the next submodule update sets its core.worktree to the
# copy: git status, checkout, clean or apply run in the other worktree's submodule then act on this one's files, and
# removing the other worktree deletes the git dir this one depends on.
#
# So for every .git file under a submodule path (nested ones too, shallowest first), the git dir it points at, if it
# is not this worktree's, is copied to the same place under this worktree's git dir (a nested one comes with its
# parent's copy), the .git file is pointed at the copy and the copy's core.worktree at this working tree, both as
# relative paths, the way git writes them, with core.longpaths on (a worktree's git dirs are deep enough for Windows'
# path limit). A copied link is resolved from the tree it was copied from: one from the main checkout
# (../../../.git/modules/...) would lead, from here, into this worktree's .git file. If the other git dir's
# core.worktree had been moved here, it is pointed back at its own worktree's tree. Run with no argument, that is the
# repair for a worktree that already shares.
#
# With a source, its working trees come with their patches applied and any other edits, on whatever commits it had,
# so each is reset to its commit and cleaned (ignored files stay), then synced to the URLs and updated to the commits
# this tree pins. Run tools/apply_runtime_patches.sh after it.
set -euo pipefail
root="$(cygpath -u "$(git rev-parse --show-toplevel)" 2>/dev/null || git rev-parse --show-toplevel)"
cd "$root"
gitdir="$(cd "$(git rev-parse --absolute-git-dir)" && pwd)"
paths=($(git config -f .gitmodules --get-regexp '\.path$' | cut -d' ' -f2))
src=

if (( $# > 0 )); then
    src="$(cd "$1" && pwd)"
    # A checkout from before the game moved to the repo's root (2026-10-11) has its submodules under rush2049/.
    [[ -e "$src/${paths[0]}/.git" || ! -e "$src/rush2049/${paths[0]}/.git" ]] || src="$src/rush2049"
    for p in "${paths[@]}"; do
        [[ -e "$src/$p/.git" ]] || { echo "not checked out in $src: $p" >&2; exit 1; }
        if [[ -e "$p/.git" ]]; then
            echo "already checked out here: $p (run without an argument to make it this worktree's own)" >&2
            exit 1
        fi
    done
    for p in "${paths[@]}"; do
        echo "copying $p"
        mkdir -p "$p"
        cp -r "$src/$p/." "$p/"
    done
fi

# The worktree of the repository whose git dir is $1 (<repo>/.git, or <repo>/.git/worktrees/<name>).
owner_root() {
    if [[ -f "$1/gitdir" ]]; then
        dirname "$(cygpath -u "$(cat "$1/gitdir")" 2>/dev/null || cat "$1/gitdir")"
    else
        dirname "$1"
    fi
}

for p in "${paths[@]}"; do
    [[ -d "$p" ]] && find "$p" -name .git -type f
done | awk -F/ '{ print NF, $0 }' | sort -n | cut -d' ' -f2- | while read -r link; do
    tree="$(cd "$(dirname "$link")" && pwd)"
    target="$(sed -n 's/^gitdir: //p' "$link")"
    # Relative, from the tree it was copied from; with no source, a link that points nowhere needs a fresh clone.
    from="${src:+$src/$(dirname "$link")}"
    old="$(cd "${from:-$tree}" && cd "$target" 2>/dev/null && pwd)" || { echo "$link points nowhere: $target" >&2; exit 1; }
    case "$old/" in
    "$gitdir/modules/"*)
        new="$old"
        ;;
    *)
        name="${old#*/modules/}"
        new="$gitdir/modules/${name#rush2049/}"  # named by their old paths before the move
        if [[ ! -d "$new" ]]; then
            echo "copying git dir of ${link%/.git}"
            mkdir -p "$(dirname "$new")"
            cp -r "$old" "$new"
        fi
        # Its core.worktree may have been moved here by a submodule update; point it back at its own tree.
        wt="$(git config -f "$old/config" core.worktree || true)"
        if [[ -n "$wt" && "$(cd "$old" && cd "$wt" 2>/dev/null && pwd)" == "$tree" ]]; then
            theirs="$(owner_root "${old%%/modules/*}")/${link%/.git}"
            if [[ -d "$theirs" ]]; then
                git config -f "$old/config" core.worktree "$(realpath --relative-to="$old" "$theirs")"
                echo "pointed $old back at $theirs"
            fi
        fi
        echo "gitdir: $(realpath --relative-to="$tree" "$new")" > "$link"
        ;;
    esac
    git config -f "$new/config" core.worktree "$(realpath --relative-to="$new" "$tree")"
    git config -f "$new/config" core.longpaths true
done

if (( $# > 0 )); then
    git -c core.longpaths=true submodule foreach --recursive --quiet 'git reset --quiet --hard && git clean -fdq'
    # Synced first: a copied git dir fetches from the URL it was cloned from, which may not be the one .gitmodules names
    # now (a checkout from before the forks). Twice, since a nested submodule's URL is only known once its parent moved.
    git submodule sync --quiet && git -c core.longpaths=true submodule update --init
    git submodule sync --quiet --recursive && git -c core.longpaths=true submodule update --init --recursive
fi
