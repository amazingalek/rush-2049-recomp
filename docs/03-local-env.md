# Local environment (checked 2026-10-09)

Machine: Windows 10 Enterprise LTSC 2019 (build 17763), NVIDIA GeForce RTX 2070, driver 560.94.

## Present

| Tool | Version / location |
|---|---|
| git | 2.55.0.windows.3 |
| Visual Studio 2022 Enterprise | MSVC toolset 14.44.35207 (also VS 2017 and 2019 installed) |
| Python 3 | 3.11 via the `py` launcher (`py -3`); bare `python` on PATH is **2.7.17** |

## Missing

| Tool | Why a recomp needs it |
|---|---|
| CMake | Recomp projects build with CMake |
| Ninja | The generator the recomp projects' build instructions use |
| clang / clang-cl (LLVM) | The recomp projects build the runtime with clang-cl on Windows, and build the C `patches/` for MIPS with clang + a MIPS target |
| MIPS linker (`ld.lld` from LLVM, or binutils `mips-linux-gnu-ld`) | Links the patches into an ELF that N64Recomp recompiles |
| make | The patches folder in the existing recomps builds with a Makefile |
| WSL / Docker | Not required, but decomp toolchains (splat, IDO recomp, asm-differ) are Linux-first |

The VS 2022 install has no "C++ Clang tools for Windows" or "C++ CMake tools" component (no
bundled `VC/Tools/Llvm`, no bundled CMake). Two ways to fix it:

1. Add the components *C++ Clang Compiler for Windows* and *C++ CMake tools for Windows* in
   the Visual Studio Installer (brings clang-cl, CMake and Ninja together), or
2. install LLVM, CMake and Ninja standalone (e.g. `winget install LLVM.LLVM Kitware.CMake Ninja-build.Ninja`).

Exact versions to match get pinned in [01-toolchain.md](01-toolchain.md) once the toolchain
research is in.

## Notes

- Windows 10 LTSC 2019 is old (17763). RT64 targets D3D12 and Vulkan; D3D12 works on 17763, but
  check RT64's minimum Windows build before relying on it.
- The RTX 2070 is fine for RT64. helga's RTX 5090 is for the gpu-queue and is not needed here.
