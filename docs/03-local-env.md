# Local environment

Machine: Windows 10 Enterprise LTSC 2019 (build 17763), NVIDIA GeForce RTX 2070 (driver 560.94).
The session that set this up had **no admin rights**, so everything is installed per-user
under `%LOCALAPPDATA%\Programs\n64-toolchain\`.

## Use it

```sh
. tools/env.sh        # Git Bash
. tools/env.ps1       # PowerShell
```

Both put the tools on `PATH` and set `INCLUDE`/`LIB` for the portable MSVC and Windows SDK.
They also export `MIPS_CC`, `MIPS_LD` and `MIPS_OBJDUMP` for the MIPS patches build.

The `llvm`, `cmake`, `ninja` and `make` bin dirs are also on the **user PATH**. `INCLUDE` and
`LIB` are not, so source an env script before building host code.

## What's installed (2026-10-09)

| Tool | Version | Where (under `n64-toolchain\`) | Verified |
|---|---|---|---|
| LLVM (clang-cl, lld, llvm-*) | 22.1.8, llvm.org x86_64-pc-windows-msvc | `llvm\` | sha256 matches the GitHub release digest |
| CMake | 3.31.12 | `cmake\` | sha256 matches the GitHub release digest |
| Ninja | 1.13.2 | `ninja\` | sha256 matches the GitHub release digest |
| GNU make | 4.4.1 (ezwinports, without guile) | `make\` | md5 matches SourceForge |
| MSVC toolset + Windows SDK | MSVC 14.44.35207, SDK 10.0.26100.0 | `portable-msvc\msvc\` | per-payload sha256 from Microsoft's manifest |
| MSYS2 (clang64) clang + llvm-tools | clang 23.1.3 | `msys64\clang64\bin\` | installer sha256 matches; packages verified by pacman |

Already on the machine: git 2.55.0, Python 3.11 (`py -3`). Bare `python` on `PATH` is 2.7.17,
because the machine PATH comes before the user PATH, so always use `py -3`.

## Why it's set up this way

- **CMake 3.31, not 4.x.** CMake 4 rejects `cmake_minimum_required(VERSION <3.5)`, and old
  submodules still declare that. If a project needs 4.x, swap it, and use
  `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` for the old submodules.
- **LLVM 22.1.8, not 23.1.3.** 23.1.3 was three days old when this was installed, and 22.1.8
  is the last release of the 22.x series. The runtime only asks for Clang 15 or newer.
- **Portable MSVC.** VS 2022 Enterprise is installed but has no C++ workload: no MSVC libs, no
  vcvars, no Windows 10 SDK. VS 2017 and 2019 have none either. Adding the workload needs
  admin, so [mmozeiko's portable-msvc.py](https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977)
  (reviewed before running) fetched the Build Tools and the SDK from Microsoft's VS 2022 channel
  manifest. Running it accepted the VS Build Tools license. The script also deletes `vctip.exe`
  (telemetry).
- **Two clangs.** The llvm.org Windows builds (both the tarball and the `LLVM-*-win64.exe`
  installer) are built **without the MIPS target**: `clang -print-targets` lists no mips. The
  patches build (`clang -target mips …`) therefore uses MSYS2's clang, which is built with all
  targets. Linking uses llvm.org's `ld.lld`, which handles MIPS ELF fine without the backend.
  MSYS2's `ld.lld.exe` rejects GNU-style flags such as `--emit-relocs`, so it isn't used.
  The patch Makefiles default to `CC ?= clang` and `LD ?= ld.lld`, so run
  `make CC="$MIPS_CC" LD="$MIPS_LD"`.
- If admin becomes available, the official route is VS 2022 + "Desktop development with C++" +
  "C++ Clang Compiler for Windows" + "C++ CMake tools". Microsoft's bundled clang should include
  MIPS, but that is unverified.

## Smoke tests that passed

1. **Host build.** CMake + Ninja + clang-cl 22.1.8 builds and runs a C++20 executable against
   the portable MSVC and SDK. CMake reports "Clang 22.1.8 with MSVC-like command-line".
   `env.ps1` + `clang-cl` links against `user32.lib` and runs.
2. **MIPS build.** With MSYS2 clang, the Zelda patches' flags work (`-target mips -mips2
   -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg …`), and llvm.org
   `ld.lld --emit-relocs` produces an `ELF32 big-endian MIPS R3000, o32, mips2` binary.
   `$MIPS_OBJDUMP` disassembles it correctly.

## Still to check

- An end-to-end build of a real recomp, e.g. BanjoRecomp or Zelda64Recomp, needs a ROM. Without
  one, building N64Recomp and RSPRecomp from source is the next proof.
- RT64's minimum Windows build: D3D12 is available on 17763, but RT64's needs are unconfirmed.
  Vulkan is the fallback.
