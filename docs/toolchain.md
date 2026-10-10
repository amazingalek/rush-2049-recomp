# How the recomp fits together

What N64Recomp, N64ModernRuntime, RecompFrontend and RT64 each do, and where this repo plugs into them. Read from
the projects' source code (2026-10-09); where a README and the code disagree, this follows the code. What is
specific to Rush 2049 (its memory map, the hooks, the runtime fixes) is in [rush2049.md](rush2049.md).

## The stack

| Layer | Repo (submodule under `rush2049/lib/`) | License |
|---|---|---|
| Recompiler (MIPS → C), RSPRecomp, mod tools, LiveRecomp | [N64Recomp/N64Recomp](https://github.com/N64Recomp/N64Recomp), nested in the runtime | MIT |
| Runtime: ultramodern + librecomp | [N64Recomp/N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) | GPL-3.0 |
| Launcher, menus, input (RmlUi + SDL2) | [N64Recomp/RecompFrontend](https://github.com/N64Recomp/RecompFrontend) | none listed |
| Renderer | [rt64/rt64](https://github.com/rt64/rt64) | MIT |

Because the runtime is GPL-3.0, the recomp is GPL-3.0 too. Our changes to these submodules are patch files in
`rush2049/runtime-patches/`.

## N64Recomp: the recompiler

Each MIPS function becomes a C function `void f(uint8_t* rdram, recomp_context* ctx)`, one instruction at a time:
`addiu` → `ctx->r4 = ADD32(ctx->r4, 0x20)`, `jal` → a direct call, tail-call `j`/`b` → a call. Delay slots are
handled by duplicating instructions, and a `jr` through a jump table becomes a `switch`.

**Inputs.** We use symbol-file mode: `rush2049/us.toml` points N64Recomp at the extracted ROM and at
`syms/rush2049/rush2049.us.syms.toml`, which `tools/rush2049/gen_syms.py` writes (sections, then
`functions = [{name, vram, size}]` and relocations per section). The output goes to `rush2049/RecompiledFuncs/`,
which is generated and not in git.

**Config keys we use** (all in [src/config.cpp](https://github.com/N64Recomp/N64Recomp/blob/main/src/config.cpp)):

| Key | What it does |
|---|---|
| `stubs` | Function becomes an empty stub |
| `ignored` | Function is not emitted, so the runtime or our code supplies it |
| `[[patches.instruction]] {func, vram, value}` | Replaces one instruction word |
| `[[patches.hook]] {func, before_vram?, text}` | Injects literal C text at a point in a function. The README says hooks are unimplemented; the code applies them, and `us.toml` relies on them |
| `manual_funcs`, `function_sizes` | Fix function boundaries the symbol file got wrong |

**libultra is matched by name.** [src/symbol_lists.cpp](https://github.com/N64Recomp/N64Recomp/blob/main/src/symbol_lists.cpp)
routes calls to `<name>_recomp` in the runtime for reimplemented functions, drops about 400 libultra internals and
renames libc and math. An unnamed libultra function is recompiled as-is, pokes hardware registers and crashes; a
game function given a libultra name silently disappears. See "Function naming" in [rush2049.md](rush2049.md).

**Hard cases.**
- *Jump tables*: found by tracing `lui`/`addiu`/`addu`/`lw` and assumed to sit in the function's own section.
  "Failed to determine size of jump table" means a boundary or symbol is wrong.
- *Function pointers*: `jalr` becomes `LOOKUP_FUNC(ctx->r25)(rdram, ctx)`, a lookup over the loaded sections; a
  miss aborts with "Failed to find function at 0x…".
- *Overlays*: relocatable sections get `RELOC_HI16`/`RELOC_LO16` and are relocated when loaded.
- *Not supported*: self-modifying code, TLB-mapped code, `tlbp` and `cache`. Stub or ignore the function and
  reimplement it ("What N64Recomp can't translate" in [rush2049.md](rush2049.md)).

**RSPRecomp** translates non-graphics RSP microcode. Here that is the MusyX audio ucode (`rush2049/musyx.toml` →
`rush2049/rsp/musyx.cpp`); its config needs `extra_indirect_branch_targets` because RSP code jumps through
computed addresses. The default audio path is our HLE in `rush2049/src/hle/`, and `--musyx` picks the others.

**Mods (.nrm).** An `.nrm` is a zip with a JSON manifest, an optional MIPS binary, symbols, texture packs and a
thumbnail. `RecompModTool` builds one from `mod.toml`, and LiveRecomp (a JIT on sljit) recompiles mods at game
start. The API is `RECOMP_PATCH`, `RECOMP_HOOK` / `RECOMP_HOOK_RETURN`, `RECOMP_IMPORT` / `RECOMP_EXPORT` and
`RECOMP_DECLARE_EVENT` / `RECOMP_CALLBACK` ([Wiseguy's modding doc](https://hackmd.io/@Wiseguy/HyhUXwiV0)). What
this game gives mods is under "Mods" in [rush2049.md](rush2049.md).

## N64ModernRuntime

**ultramodern** reimplements libultra's OS layer on the host: one host thread per `OSThread` with priority
scheduling, message queues, timers, VI timing, RSP task dispatch, audio and controllers. **librecomp** provides the
`_recomp` versions of the libultra calls (threads, messages, timers, PI/VI/AI, controllers and paks, saves,
`osSpTask*`).

The game supplies, in `rush2049/src/main.cpp`:
- a `recomp::GameEntry`: the ROM's XXH3-64 hash, game and mod IDs, save type, entrypoint, and
  `decompression_routine`, which builds the recomp ROM with the deflate-compressed code inflated;
- a `recomp::Configuration` with the RSP callback that maps a task to its microcode function, and the audio,
  input, renderer, event and thread callbacks;
- `register_overlays` and `register_patches` (`src/register_overlays.cpp`, `src/register_patches.cpp`).

**Overlay loading is the game's job.** The runtime maps only the boot code at startup, and a plain PI DMA does not
register code. Rush 2049 inflates its main code and overlays itself, so hooks on that routine register each
segment as it lands (`src/code_loading.cpp`).

**Memory.** `MEM_W` maps a game address to `rdram + (addr - 0x80000000)`. KSEG1 and hardware-register accesses land
in guard memory, so a stray register poke faults at an address that is easy to decode (subtract the RDRAM base).

## RT64

RT64 renders the display lists at a high level on D3D12, Vulkan or Metal with ubershaders, and identifies the
graphics microcode by hashing its data segment. Rush 2049's F3DEX2 2.08 NoN `.fifo` is in its table.

For widescreen and high frame rates it has two mechanisms: automatic matching of draw calls between frames, and
the extended GBI ([rt64_extended_gbi.h](https://github.com/rt64/rt64/blob/main/include/rt64_extended_gbi.h)),
commands a game can emit to tag transforms for interpolation (`gEXMatrixGroup*`) and to anchor rects, viewports
and scissors to the screen edges (`gEXSetRectAlign`, `gEXSetViewportAlign`, `gEXSetScissorAlign`). This game
leaves interpolation to the matching and emits extended commands for the HUD and menus (`src/widescreen.cpp`) and
the hi-res font (`src/hires_font.cpp`); see "Runtime notes" in [rush2049.md](rush2049.md).
