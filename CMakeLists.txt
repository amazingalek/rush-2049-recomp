# San Francisco Rush 2049: Recompiled.
# Based on BanjoRecomp's CMakeLists.txt (GPL-3.0). Builds on Windows (clang-cl) and Linux (clang); only Windows has been run.
# With Emscripten (emcmake cmake), it builds the page for a browser, Rush2049Recompiled.html (README, "In a browser"),
# through lib/recomp-web/frontend: RT64 and the menus on WebGPU, mods off.

cmake_minimum_required(VERSION 3.20)

project(Rush2049Recompiled LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()

# Opt out of the constexpr mutex constructor on Windows to avoid vcredist issues.
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()

# Speed for everything built here, RT64 and the runtime included, not just the game's own code. RelWithDebInfo (the
# build the README uses) inlines only functions marked inline by default: inline as Release does, and keep the symbols for
# crash traces. And build every x86-64 library for Nehalem (SSE4.2, 2008), which the executable has always been built
# for, so that rounding functions such as ceilf become single instructions instead of library calls.
foreach (lang C CXX)
    string(REPLACE "/Ob1" "/Ob2" CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO}")
endforeach()
if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    add_compile_options(-march=nehalem)
endif()

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

if (EMSCRIPTEN)
    # Threads are pthreads (SharedArrayBuffer), and the runtime ends a game thread by throwing. -msimd128 is the web's
    # -march=nehalem: wasm SIMD, which every browser with WebGPU has, for hlslpp (its wasm backend) and the
    # auto-vectorizer; a race display list took ~5% less time with it (CLAUDE.md).
    add_compile_options(-pthread -fwasm-exceptions -msimd128)
    # With it xxHash would take its NEON path through Emscripten's arm_neon.h (SIMDe), which it includes inside its
    # extern "C", where the C++ headers that reaches don't compile. XXH_HAS_INCLUDE is used for nothing else, so XXH3
    # stays scalar, as it was before -msimd128.
    add_compile_options("-DXXH_HAS_INCLUDE(x)=0")
    add_link_options(-pthread -fwasm-exceptions)
    # With ccache on PATH, emcc runs its clang through it (EM_COMPILER_WRAPPER, so the key is clang's own command and
    # binary), and paths inside this checkout are made relative (CCACHE_BASEDIR), so a fresh build dir at the same place
    # in another checkout, or after a move, reuses the objects instead of compiling them again (README).
    find_program(CCACHE_PROGRAM ccache)
    if (CCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER
            ${CMAKE_COMMAND} -E env EM_COMPILER_WRAPPER=${CCACHE_PROGRAM} CCACHE_BASEDIR=${CMAKE_SOURCE_DIR})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_C_COMPILER_LAUNCHER})
    endif()
    # Mods are off on the web: the runtime can't recompile a hooked function there (no live recompiler).
    set(RUSH2049_PATCHES OFF)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(
    HLSL_CPU
    __PRFCHWINTRIN_H
)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/rt64 ${CMAKE_BINARY_DIR}/rt64)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)

if (EMSCRIPTEN)
    add_subdirectory(${CMAKE_SOURCE_DIR}/lib/recomp-web/frontend ${CMAKE_BINARY_DIR}/web_frontend)
    # fmt 11.1 (N64Recomp's) leans on <cstdlib> arriving through other headers, which Emscripten's libc++ doesn't do.
    target_compile_options(fmt PRIVATE -include cstdlib)
endif()

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# RecompiledFuncs: N64Recomp's output (run it on us.toml first).
add_library(RecompiledFuncs STATIC)

target_compile_options(RecompiledFuncs PRIVATE
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
    -falign-functions=16
)
# A call to an undeclared function links to a trap in WebAssembly, so it is an error there (N64Recomp's funcs.h declares
# every function the recompiled code calls).
if (EMSCRIPTEN)
    target_compile_options(RecompiledFuncs PRIVATE -Werror=implicit-function-declaration)
endif()

target_include_directories(RecompiledFuncs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)

file(GLOB FUNC_C_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
file(GLOB FUNC_CXX_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.cpp)
target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES})

# PatchesLib: the patches library (patches/), compiled for MIPS by patches/Makefile and recompiled by N64Recomp
# (patches.toml; the runtime's own N64Recomp, target N64RecompCLI). Its RECOMP_EXPORT functions are what mods
# import from "*". Needs a clang with a MIPS backend and ld.lld (PATCHES_C_COMPILER, PATCHES_LD; on Windows
# tools/env.sh's MIPS_CC and MIPS_LD). RUSH2049_PATCHES=OFF is for CI's Windows job only, which has no MIPS clang
# and stops before linking: without the library the executable doesn't link.
option(RUSH2049_PATCHES "Build the patches library (mods need it)" ON)
set(PATCHES_C_COMPILER clang CACHE STRING "clang with a MIPS backend, for patches/")
set(PATCHES_LD ld.lld CACHE STRING "ld.lld, for patches/")

if (RUSH2049_PATCHES)
    add_library(PatchesLib STATIC)

    target_compile_options(PatchesLib PRIVATE
        -fno-strict-aliasing
        -Wno-unused-variable
        -Wno-implicit-function-declaration
        -falign-functions=16
    )

    target_include_directories(PatchesLib PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    )

    target_sources(PatchesLib PRIVATE
        ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
        ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c
    )

    # make is incremental, so this runs every build and relinks only when a patch source changed.
    add_custom_target(PatchesBin
        COMMAND ${CMAKE_COMMAND} -E env CC=${PATCHES_C_COMPILER} LD=${PATCHES_LD} make
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/patches
        BYPRODUCTS ${CMAKE_SOURCE_DIR}/patches/patches.elf
    )

    add_custom_command(OUTPUT
            ${CMAKE_SOURCE_DIR}/patches/patches.bin
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/recomp_overlays.inl
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/funcs.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/RecompiledPatches
        COMMAND N64RecompCLI patches.toml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.elf ${CMAKE_SOURCE_DIR}/patches.toml
    )

    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/patches/patches.bin rush2049_patches_bin
            ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.h
        DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.bin
    )
endif()

add_executable(Rush2049Recompiled)

target_sources(Rush2049Recompiled PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/config.cpp
    ${CMAKE_SOURCE_DIR}/src/frame_rate.cpp
    ${CMAKE_SOURCE_DIR}/src/register_overlays.cpp
    ${CMAKE_SOURCE_DIR}/src/mod_exports.cpp
    ${CMAKE_SOURCE_DIR}/src/code_loading.cpp
    ${CMAKE_SOURCE_DIR}/src/libultra_extra.cpp
    ${CMAKE_SOURCE_DIR}/src/spin_yield.cpp
    ${CMAKE_SOURCE_DIR}/src/crash_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/rom_finder.cpp
    ${CMAKE_SOURCE_DIR}/src/controller_pak.cpp
    ${CMAKE_SOURCE_DIR}/src/debug_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/dl_dump.cpp
    ${CMAKE_SOURCE_DIR}/src/widescreen.cpp
    ${CMAKE_SOURCE_DIR}/src/hires_font.cpp
    ${CMAKE_SOURCE_DIR}/src/rewind.cpp
    ${CMAKE_SOURCE_DIR}/src/music_thread.cpp
    ${CMAKE_SOURCE_DIR}/src/audio_resampler.cpp
    ${CMAKE_SOURCE_DIR}/src/audio_limiter.cpp
    ${CMAKE_SOURCE_DIR}/src/musyx_hle.cpp
    ${CMAKE_SOURCE_DIR}/src/gsm_decoder.cpp
    ${CMAKE_SOURCE_DIR}/src/hle/musyx.c
    ${CMAKE_SOURCE_DIR}/src/hle/audio.c
    ${CMAKE_SOURCE_DIR}/src/hle/memory.c
    ${CMAKE_SOURCE_DIR}/rsp/musyx.cpp
)

target_include_directories(Rush2049Recompiled PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty/sse2neon
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/hlslpp/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/inc
    ${CMAKE_SOURCE_DIR}/lib/rt64/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/rhi
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/render
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompinput/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/nativefiledialog-extended/src/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(Rush2049Recompiled PRIVATE -fno-strict-aliasing -fms-extensions)

if (MSVC)
    # Identical code folding would merge recompiled functions, which breaks function lookup by address.
    target_link_options(Rush2049Recompiled PRIVATE /OPT:NOICF)
endif()

if (WIN32)
    include(FetchContent)
    set(SDL2_VERSION "2.30.3")
    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(Rush2049Recompiled PRIVATE ${sdl2_SOURCE_DIR}/include)
    target_link_directories(Rush2049Recompiled PRIVATE ${sdl2_SOURCE_DIR}/lib/x64)

    add_custom_command(TARGET Rush2049Recompiled POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:Rush2049Recompiled>)

    # Console subsystem for now: the log is the main debugging tool during bring-up.
    target_link_libraries(Rush2049Recompiled PRIVATE SDL2 Winmm.lib)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(SDL2 REQUIRED)
    add_compile_definitions("PLUME_SDL_VULKAN_ENABLED")
    add_compile_definitions("RT64_SDL_WINDOW_VULKAN")
    target_include_directories(Rush2049Recompiled PRIVATE ${SDL2_INCLUDE_DIRS})
    find_package(Freetype REQUIRED)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    target_link_libraries(Rush2049Recompiled PRIVATE ${FREETYPE_LIBRARIES} SDL2::SDL2 "-latomic -static-libstdc++" ${CMAKE_DL_LIBS} Threads::Threads)
    # Export the recompiled functions' names, so the crash handler's backtrace can print them.
    target_link_options(Rush2049Recompiled PRIVATE -rdynamic)
endif()

# DXC settings RT64 expects from the parent project.
set(DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set(DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set(DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set(DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")

if (WIN32)
    add_compile_definitions(NOMINMAX)
endif()
if (CMAKE_HOST_WIN32)
    set(DXC "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc.exe")
else()
    set(DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc-linux")
endif()

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)
set(RECOMP_FRONTEND_RT64_PATH ${CMAKE_SOURCE_DIR}/lib/rt64)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RecompFrontend)

target_link_libraries(Rush2049Recompiled PRIVATE
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    libzstd_static
)

if (RUSH2049_PATCHES)
    target_sources(Rush2049Recompiled PRIVATE ${CMAKE_SOURCE_DIR}/src/register_patches.cpp)
    target_link_libraries(Rush2049Recompiled PRIVATE PatchesLib)
    target_compile_definitions(Rush2049Recompiled PRIVATE RUSH2049_PATCHES)
endif()

if (EMSCRIPTEN)
    # The page (lib/recomp-web/frontend), with the menus' assets and the controller mappings in its .data file. No ROM:
    # the player picks theirs in the launcher, and it stays in the browser.
    recomp_web_frontend_page(Rush2049Recompiled)
    target_link_options(Rush2049Recompiled PRIVATE
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt@/recompcontrollerdb.txt")
    # For a build to try something: the page is linked at -O1, where Binaryen's wasm-opt doesn't optimize the whole
    # module, which takes most of a link at -O3 (1.5-3 minutes on one core). The code is still compiled at -O3.
    option(RUSH2049_WEB_FAST_LINK "Link the page without wasm-opt's optimizations (test builds)" OFF)
    if (RUSH2049_WEB_FAST_LINK)
        target_link_options(Rush2049Recompiled PRIVATE -O1)
    endif()
else()
    target_link_libraries(Rush2049Recompiled PRIVATE nfd)
endif()

set_property(TARGET Rush2049Recompiled PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
