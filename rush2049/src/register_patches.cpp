// Registers the patches library (patches/, recompiled into RecompiledPatches/ by N64Recomp) with the runtime: its
// code and data, the functions it exports to mods, and its events.

#include "rush2049_game.h"
#include "../RecompiledPatches/recomp_overlays.inl"

#include "librecomp/overlays.hpp"

extern "C" {
    extern const char rush2049_patches_bin[];
    extern const size_t rush2049_patches_bin_size;
}

void rush2049::register_patches() {
    recomp::overlays::register_patches(rush2049_patches_bin, rush2049_patches_bin_size, section_table, ARRLEN(section_table));
    recomp::overlays::register_base_exports(export_table);
    recomp::overlays::register_base_events(event_names);
    recomp::overlays::register_manual_patch_symbols(manual_patch_symbols);
}
