# The N64Recomp toolchain

Researched 2026-10-09 from the repos' source code, not just their READMEs. The N64Recomp README
is stale on several points (symbol-file mode, hooks, RSP overlays); where README and code
disagree, this doc follows the code.

## The stack

| Layer | Repo | License | Last push |
|---|---|---|---|
| Recompiler (MIPS → C), RSPRecomp, mod tools, LiveRecomp | [N64Recomp/N64Recomp](https://github.com/N64Recomp/N64Recomp) | MIT | 2026-05-27 |
| Runtime: ultramodern + librecomp | [N64Recomp/N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) | GPL-3.0 | 2026-09-29 |
| Launcher, menus, input (RmlUi + SDL2) | [N64Recomp/RecompFrontend](https://github.com/N64Recomp/RecompFrontend) | none listed | 2026-09-29 |
| Renderer | [rt64/rt64](https://github.com/rt64/rt64) | MIT | 2026-10-08 |
| Reference game ports | [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), [BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp) | GPL-3.0 | 2026-09 |

Because the runtime is GPL-3.0, our port is effectively GPL-3.0.

## 1. N64Recomp: the recompiler

Each MIPS function becomes a C function `void f(uint8_t* rdram, recomp_context* ctx)`, one
instruction at a time: `addiu` → `ctx->r4 = ADD32(ctx->r4, 0x20)`, `jal` → a direct call,
tail-call `j`/`b` → a call. Delay slots are handled by duplicating instructions, and a `jr`
through a jump table becomes a `switch`. Decoding uses rabbitizer. It is tested against IDO,
gcc 2.7.2 and clang-mips output; modern gcc output can confuse it.

### Inputs

- **ELF mode** (`elf_path`). A function counts only if its symbol is `FUNC`, has a non-zero
  size and sits in a real section, not `ABS` (unless `use_absolute_symbols = true`).
  See [#53](https://github.com/N64Recomp/N64Recomp/issues/53) and [#58](https://github.com/N64Recomp/N64Recomp/issues/58).
- **Symbol-file mode** (`symbols_file_path` + `rom_file_path`). A TOML of `[[section]]`
  entries (`name`, `rom`, `vram`, `size`, optional `got_address`), each listing
  `functions = [{name, vram, size}]` and `relocs = [{type, vram, target_vram}]`.
  Example: [Zelda64RecompSyms](https://github.com/Zelda64Recomp/Zelda64RecompSyms).
- **Making symbol files.** `N64Recomp <cfg> --dump-context` turns an ELF into `dump.toml`
  (functions and relocs) plus `data_dump.toml`. This is how the `*RecompSyms` repos are made
  from a decomp ELF.

### Config keys (from [src/config.cpp](https://github.com/N64Recomp/N64Recomp/blob/main/src/config.cpp))

`[input]`:

| Group | Keys |
|---|---|
| Paths | `entrypoint`, `elf_path`, `symbols_file_path`, `rom_file_path`, `output_func_path` (required), `relocatable_sections_path` |
| Codegen | `uses_mips3_float_mode`, `bss_section_suffix` (default `.bss`; Banjo uses `_bss`), `single_file_output`, `use_absolute_symbols`, `functions_per_output_file` (default 50), `recomp_include`, `trace_mode` |
| Function boundaries | `manual_funcs = [{name, section, vram, size}]`, `function_sizes = [{name, size}]` |
| Debug info | `use_mdebug`, `mdebug_file_mappings` |
| Patches and mods | `output_binary_path`, `unpaired_lo16_warnings`, `func_reference_syms_file`, `data_reference_syms_files`, `allow_exports`, `strict_patch_mode` |

`[patches]`:

| Key | What it does |
|---|---|
| `stubs` | Function becomes an empty stub |
| `ignored` | Function is not emitted |
| `renamed` | Function is emitted under another name |
| `[[patches.instruction]] {func, vram, value}` | Replaces one instruction word |
| `[[patches.hook]] {func, before_vram?, text}` | Injects literal C text. The README says hooks are unimplemented, but the code parses and applies them; untested by us. |

### libultra is matched by name

[src/symbol_lists.cpp](https://github.com/N64Recomp/N64Recomp/blob/main/src/symbol_lists.cpp)
holds three hardcoded lists:

- `reimplemented_funcs`: calls are routed to `<name>_recomp` in the runtime.
- `ignored_funcs`: about 400 libultra internals are dropped.
- `renamed_funcs`: libc and math (`memcpy`, `sinf`) are renamed so they don't clash with the host.

**The matching is by name.** An unnamed libultra function (`func_XXXXXXXX`) gets recompiled
as-is, pokes hardware registers, and crashes. Naming every libultra function is mandatory.

### Hard cases

- **Jump tables.** [src/analysis.cpp](https://github.com/N64Recomp/N64Recomp/blob/main/src/analysis.cpp)
  traces registers through `lui`/`addiu`/`addu`/`lw` (including `$gp`/GOT tables) and assumes
  the table is in the same section as the function. Typical errors: "Failed to determine size
  of jump table" ([#56](https://github.com/N64Recomp/N64Recomp/issues/56)) and "unable to find
  jump table for jr $s2" ([#108](https://github.com/N64Recomp/N64Recomp/issues/108)). Fix with
  better symbols, an instruction patch, or by ignoring the function and reimplementing it.
- **Function pointers.** `jalr` becomes `LOOKUP_FUNC(ctx->r25)(rdram, ctx)`, a hash-map
  lookup over the loaded sections. A miss aborts with "Failed to find function at 0x…". Jump
  targets inside a function that have no symbol are emitted as `static_<section>_<addr>`.
- **Overlays.** Both static-address and relocatable overlays work. In relocatable sections,
  instructions with relocations get `RELOC_HI16`/`RELOC_LO16` macros, and the runtime
  relocates them on load.
- **Not supported:** self-modifying code, TLB-mapped code, and the `tlbp`/`cache`
  instructions. The fix is to stub or ignore the function that contains them. Banjo stubs
  about 60 duplicated `boot_*` libultra functions plus `osMapTLBRdb`
  ([banjo.us.rev0.toml](https://github.com/BanjoRecomp/BanjoRecomp/blob/main/banjo.us.rev0.toml)).

### Output

- `funcs.h`
- `funcs_N.c` (about 50 functions per file)
- `lookup.cpp`
- `recomp_overlays.inl`: section table, relocations, export and event tables
- `recomp_entrypoint`: the renamed entry function

### RSPRecomp

[RSPRecomp](https://github.com/N64Recomp/N64Recomp/tree/main/RSPRecomp) has its own TOML:

| Key | Notes |
|---|---|
| `text_offset`, `text_size`, `text_address`, `rom_file_path` | Where the RSP code is |
| `output_file_path`, `output_function_name` | What it produces |
| `extra_indirect_branch_targets` | **Needed**: RSP code uses computed jumps |
| `unsupported_instructions` | Instructions to reject |
| `overlay_slots` | IMEM overlay swapping |

RSPRecomp handles **audio and other non-graphics RSP microcode**, such as Zelda's `aspMain` and
`njpgdspMain` and Banjo's `n_aspMain`. Graphics display lists go to RT64, which emulates them
at a high level.

### Mods (.nrm)

- **Format.** An `.nrm` is a zip with a JSON manifest plus an optional MIPS binary, symbol
  file, texture packs and thumbnail.
- **Build.** `RecompModTool` builds one from `mod.toml`.
- **Loading.** At game start, LiveRecomp (a JIT built on sljit) recompiles the mods.
- **API.** `RECOMP_PATCH`, `RECOMP_HOOK` / `RECOMP_HOOK_RETURN`, `RECOMP_IMPORT` /
  `RECOMP_EXPORT`, `RECOMP_DECLARE_EVENT` / `RECOMP_CALLBACK`.
- **Docs.** [Wiseguy's modding doc](https://hackmd.io/@Wiseguy/HyhUXwiV0),
  [MM mod template](https://github.com/Zelda64Recomp/MMRecompModTemplate).

Mod support is something to plan for in our port, not to build first.

## 2. N64ModernRuntime

([README](https://github.com/N64Recomp/N64ModernRuntime/blob/main/README.md))

**ultramodern** reimplements libultra's OS layer on the host: threads (one host thread per
`OSThread`, with priority scheduling), message queues, timers, VI timing, RSP task dispatch,
audio and controllers. Platform I/O comes in through callbacks the game supplies.

**librecomp** reimplements these `_recomp` entry points:

| Area | Functions |
|---|---|
| Threads | `osCreateThread`, `osStartThread`, `osStopThread`, `osDestroyThread`, `osYieldThread`, `osSetThreadPri`, `osGetThreadPri` |
| Messages, events, timers | `osCreateMesgQueue`, `osSendMesg`, `osRecvMesg`, `osJamMesg`, `osSetEventMesg`, `osViSetEvent`, `osGetCount`/`osSetCount`, `osGetTime`/`osSetTime`, `osSetTimer`, `osStopTimer` |
| Cache, interrupts, jumps | `osInvalDCache`, `osWritebackDCache`, `osSetIntMask`, `setjmp`/`longjmp` |
| PI | `osPiStartDma`, `osEPiStartDma`, `osEPiReadIo`, `osCartRomInit` |
| VI | `osViSwapBuffer`, `osViSetMode`, `osViBlack`, `osViSetXScale`, `osViSetYScale` |
| AI | `osAiSetFrequency`, `osAiSetNextBuffer`, `osAiGetLength`, `osAiGetStatus` |
| Controllers and Rumble Pak | `osContInit`, `osContStartReadData`, `osContGetReadData`, `osContQuery`, `osMotor*` |
| Saves | EEPROM, SRAM, Flash |
| RSP | `osSpTask*` |
| Stubs | VRU (`osVoice*`), `osUnmapTLBAll` |

### What the game project provides

From [game.hpp](https://github.com/N64Recomp/N64ModernRuntime/blob/main/librecomp/include/librecomp/game.hpp):

- **A `recomp::GameEntry`** with these fields:
  - `rom_hash` (XXH3-64)
  - `game_id` and `mod_game_id`
  - `save_type` (None, Eep4k, Eep16k, Sram, Flashram, AllowAll)
  - `entrypoint`
  - optional `decompression_routine` and `has_compressed_code`
  - optional `thread_create_callback` and `on_init_callback`
- **A `recomp::Configuration`.** It must include `rsp_callbacks.get_rsp_microcode(const OSTask*)`,
  which maps a task to its RSPRecomp'd function, and the renderer callbacks. The audio, input,
  gfx, events and threads callbacks are also set here.
- **Registration calls:** `register_overlays(section_table, overlay_sections_by_index)` and
  `register_patches(...)`.

### Overlay loading is the game's job

At startup the runtime maps only `load_overlays(0x1000, entry, 1MB)`
([recomp.cpp](https://github.com/N64Recomp/N64ModernRuntime/blob/main/librecomp/src/recomp.cpp)).
A plain PI DMA does **not** register code. Each game patches its own overlay loader to call
`recomp_load_overlays(rom, ram, size)`:

- Zelda patches `Main_Init` and `Overlay_Load` ([required_patches.c](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/dev/patches/required_patches.c)).
- Banjo patches its rarezip overlay loader (`patches/overlay_loading.c`).

ROM addresses here are those of the **decompressed** ROM layout.

### Memory model

`MEM_W` maps a game address to `rdram + (addr - 0x80000000)`. KSEG1 and hardware-register
accesses land in `PAGE_NOACCESS` guard memory, so a register poke faults at an address that
is easy to decode. Zelda and Banjo patch `lui 0xA000` to `0x8000` (uncached to cached).

## 3. RT64

([README](https://github.com/rt64/rt64/blob/main/README.md),
[src/gbi/rt64_gbi.cpp](https://github.com/rt64/rt64/blob/main/src/gbi/rt64_gbi.cpp))

RT64 runs on D3D12, Vulkan and Metal, with ubershaders and the RSP emulated at high level in
compute shaders. It **identifies the microcode by hashing the ucode data segment** against a
built-in table.

**Supported:**
- RDP and S2DEX 1.03–1.07, S2DEX2, L3DEX2.
- F3D: SDK 2.0D–2.0H, SM64, Blast Corps, WG 3D Hockey.
- Game-specific F3D variants: F3DGOLDEN (GoldenEye), F3DPD (Perfect Dark), F3DWAVE (Wave Race).
- F3DEX 0.95–1.23, including LX, LP, NoN and Rej.
- F3DEX2 `.fifo`/`.xbus` 2.03–2.08, including NoN, Rej and a Cruis'n variant.
- F3DZEX2 (Zelda).

Many entries are marked "Needs confirmation".

**Not supported** (these map to `Unknown`):
- F3DFLX2, F3DAM2, L3DEX 1.x, ZSortp 0.33.
- Anything missing from the hash table, which includes custom ucodes from Factor 5, Boss Game
  Studios and Rare.

> **Triage step 1 for any candidate: find the ucode ID string in the ROM and check it against
> RT64's table.**

### Widescreen and high frame rate

RT64 has two mechanisms:

1. **Automatic draw-call matching between frames.** This is experimental.
2. **The extended GBI** ([rt64_extended_gbi.h](https://github.com/rt64/rt64/blob/main/include/rt64_extended_gbi.h)),
   turned on with a NOOP-encoded `gEXEnable`. Its commands:
   - **Interpolation:** `gEXMatrixGroup`, `gEXMatrixGroupSimple`, `gEXMatrixGroupDecomposed`
     and `gEXPopMatrixGroup`, which give each transform a stable ID to interpolate between frames.
   - **HUD anchoring:** `gEXSetRectAlign`, `gEXSetViewportAlign`, `gEXSetScissorAlign`.
   - **Other:** `gEXSetRefreshRate`, `gEXVertexZTest`, `gEXSetProjMatrixFloat`,
     `gEXSetViewMatrixFloat`, `gEXForceUpscale2D`, `gEXSetRenderToRAM`.

Zelda carries about 10 `*_transform_tagging.c` patch files (actors, terrain, sky, particles,
effects, UI). **That tagging is the real per-game cost of high frame rate**, and it comes
after the game boots and renders correctly.

## 4. Anatomy of a game repo (Zelda64Recomp `dev`, BanjoRecomp `main`)

```
<game>.toml               main N64Recomp config (symbol-file mode + decompressed ROM)
patches.toml              config that recompiles patches/patches.elf
aspMain.*.toml            RSPRecomp config for the audio ucode
overlays.*.txt            relocatable section list (Zelda)
recompcontrollerdb.txt
lib/
  N64ModernRuntime/       submodule (nests N64Recomp)
  rt64/                   submodule
  <game>-decomp/          submodule, used for headers only
  RecompFrontend/         (Banjo) or RmlUi + lunasvg (Zelda dev)
<Game>RecompSyms/         submodule: symbol TOMLs made with --dump-context
patches/
  *.c                     RECOMP_PATCH replacements (39 in Zelda, 44 in Banjo)
  patches.ld, syms.ld, dummy_headers/, Makefile
src/
  main/main.cpp           GameEntry, ucode map, RT64 context
  main/register_overlays.cpp   includes recomp_overlays.inl
  main/register_patches.cpp
  game/rom_decompression.cpp   Yaz0 (MM) / rarezip+miniz (BK)
  game/config.cpp, recomp_api.cpp (exports to mods), debug.cpp
```

**Patches build.** `patches/` is compiled with
`clang -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls …` and linked with
`ld.lld --emit-relocs` into `patches.elf`. N64Recomp turns that into
`RecompiledPatches/patches.c` and `patches.bin`. Patches link ahead of `RecompiledFuncs`, so
they replace the originals ([patches/Makefile](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/dev/patches/Makefile)).
**This needs a clang built with the MIPS target**; see [03-local-env.md](03-local-env.md).

**ROM handling.**
- At runtime the user picks a normal ROM. librecomp fixes byte order, checks the XXH3 hash,
  and keeps a copy.
- The build needs the **decompressed** ROM, identified by SHA-1. BK's is
  `1fe1632098865f639e22c11b9a81ee8f29c75d7a`.
- Compressed code is decompressed in memory at runtime.

**Windows build** ([BanjoRecomp BUILDING.md](https://github.com/BanjoRecomp/BanjoRecomp/blob/main/BUILDING.md)):

1. Install VS 2022 with "Desktop development with C++", "C++ Clang Compiler for Windows" and
   "C++ CMake tools", plus `make`.
2. Build N64Recomp and RSPRecomp (CMake 3.20+, C++20).
3. Run `N64Recomp game.toml` and `RSPRecomp aspMain.toml`.
4. Build the game with `cmake -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl`.

SDL2 2.30.3 is fetched automatically. `RecompiledFuncs` is compiled with
`-fno-strict-aliasing -falign-functions=16`. The runtime was developed with Clang 15; older
versions may not work.

### Starting point for a new game

There is **no official new-game template**; Wiseguy's promised minimal example hasn't shipped
([#105](https://github.com/N64Recomp/N64Recomp/issues/105)). The options:

- **Fork BanjoRecomp.** Recommended. It is official-adjacent, uses RecompFrontend, and has a
  good `boot_*` stub list to learn from.
- **[SrBananaMan/N64RecompGameTemplate](https://github.com/SrBananaMan/N64RecompGameTemplate).**
  A community Banjo-style skeleton from June 2026 with 4 stars. Its `configure_game.py` renames
  the placeholders, and `User Task:` markers show what's left to do.

## 5. Bring-up workflow

1. **Triage the ROM:**
   - Graphics ucode ID against RT64's table.
   - Audio ucode.
   - Save type.
   - Overlay and compression scheme.
   - TLB use.
   - libultra version.
2. **Get symbols.**
   - Use the decomp ELF if one exists.
   - Otherwise run splat (assembly only) to an ELF, then `--dump-context`. Turn on splat's
     `asm_emit_size_directive`.
   - Make sure functions are `FUNC`, sized, and not `ABS`.
   - Name every libultra function, for example by matching against a named SDK build.
   - Example of silent failures, from the Rayman 2 port
     ([PHASE02](https://github.com/danielgomesvieira2000/rayman-2-the-great-escape-recomp/blob/main/docs/PHASE02-FINDINGS.md)):
     zero sizes meant only 1 of 4,465 functions was emitted, and `ABS` symbols from
     `undefined_funcs_auto` shadowed 116 real cross-segment calls.
3. **Run N64Recomp repeatedly.** Fix jump tables, unhandled instructions and missing `jal`
   targets with `ignored`, `stubs`, `manual_funcs`, `function_sizes` and instruction patches.
4. **RSPRecomp the audio ucode.** It will need `extra_indirect_branch_targets`.
5. **Write the runtime glue:** GameEntry, ucode map, overlay registration, and patches that
   call `recomp_load_overlays`.
   - Bring RecompFrontend up early, since it owns the RT64 context.
   - Missing fonts or `assets/recomp.rcss` throw on the render thread, and the crash shows up
     as `0xC0000409` ([PHASE03](https://github.com/danielgomesvieira2000/rayman-2-the-great-escape-recomp/blob/main/docs/PHASE03-FINDINGS.md)).
6. **Debug boot.**
   - Subtract the RDRAM base from the faulting address to get the game address and the
     hardware register it touched.
   - Walk up to the nearest public libultra function, then name it so it gets reimplemented.
7. **Fix the basics.** Rendering, audio, saves, input.
8. **Then add enhancements.** Widescreen and high-frame-rate tagging through gEX patches.

### Pitfalls

- Unnamed or duplicated libultra copies (Banjo's `boot_*`).
- Handwritten assembly.
- `cache`, TLB and `mfc0`/`mtc0` instructions.
- Direct hardware-register access: patch KSEG1 to KSEG0.
- RCP timeout timers. Zelda NOPs those `osSetTimer` calls.
- Compressed code: Yaz0, MIO0, rarezip and similar. Recompile from a decompressed image and
  supply a `decompression_routine`.
- Game-specific DMA or overlay managers.
- libultra struct-layout differences. Rayman 2 hit an `OSThread` state-flag mismatch.
- Controller Pak saves. Beetle Adventure Racing and Rayman 2 rely on a runtime fork's
  `controller-pak` branch.
- Custom graphics ucodes, which RT64 won't render.

**Effort.** Official ports built on a full decomp took months to years. Without a decomp,
recovering symbols is the main cost.

## 6. Legal norms

- **No ROMs or game assets in the repo or in releases.** The user provides their own ROM and
  the runtime checks its hash, so only the exact expected dump is accepted.
- **Release binaries still contain translated game code.** That is the accepted gray area.
  The GoldenEye port ([cblock85/GoldenEye64Recomp](https://github.com/cblock85/GoldenEye64Recomp))
  also offers builds with no game code that recompile from the user's ROM at launch.

## Unverified

- Whether an official new-game template is in progress.
- How `[[patches.hook]]` behaves in practice.
- RecompFrontend's license, and whether Zelda `dev` will move to it.
- How much high-frame-rate support works through automatic matching versus needing
  extended-GBI tagging.
- How the community Rogue Squadron port handles Factor 5's ucode.
- How TLB-heavy games like GoldenEye are handled in community ports.
- The community port write-ups (Rayman 2, GGA, n64recomp-companion) may be AI-assisted. Treat
  them as hints, not authority.
- No minimum clang or MSVC version is pinned beyond "Clang 15" and "VS 2022".
