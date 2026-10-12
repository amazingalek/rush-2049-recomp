# The headless driver (main.cpp): the game, the runtime and the MusyX audio, without the menus or input, and with RT64
# only in the Emscripten build (HEADLESS_RT64). Builds natively and with Emscripten (emcmake cmake -S headless
# -B <dir> -DHEADLESS_ROM=<rom.z64>, which an Emscripten build preloads as /rom.z64); CI doesn't build it. Needs the
# same generated RecompiledFuncs/ and rsp/musyx.cpp as the game.

cmake_minimum_required(VERSION 3.20)

project(Rush2049Headless LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

set(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# As the game's CMakeLists.txt: the runtime's RSP vector unit needs SSE4.1 on x86-64.
if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    add_compile_options(-march=nehalem)
endif()

if (EMSCRIPTEN)
    # Threads are pthreads (SharedArrayBuffer), main() runs on one of them so the browser's thread can wait, and the
    # runtime ends a game thread by throwing. -msimd128 and XXH_HAS_INCLUDE as the game's CMakeLists.txt.
    # The rest is the driver's own (below): RT64 builds a tool of its own that runs at build time.
    add_compile_options(-pthread -fwasm-exceptions -msimd128)
    add_compile_options("-DXXH_HAS_INCLUDE(x)=0")
    add_link_options(-pthread -fwasm-exceptions)
    add_compile_options(-sUSE_SDL=2)
endif()

add_subdirectory(${GAME_DIR}/lib/N64ModernRuntime ${CMAKE_BINARY_DIR}/N64ModernRuntime)
# fmt 11.1 (N64Recomp's) leans on <cstdlib> arriving through other headers, which Emscripten's libc++ doesn't do.
if (EMSCRIPTEN)
    target_compile_options(fmt PRIVATE -include cstdlib)
endif()

add_library(RecompiledFuncs STATIC)
target_compile_options(RecompiledFuncs PRIVATE -fno-strict-aliasing -Wno-unused-variable)
# A call to an undeclared function links to a trap in WebAssembly, so it is an error there (N64Recomp's funcs.h declares
# every function the recompiled code calls).
if (EMSCRIPTEN)
    target_compile_options(RecompiledFuncs PRIVATE -Werror=implicit-function-declaration)
else()
    target_compile_options(RecompiledFuncs PRIVATE -Wno-implicit-function-declaration)
endif()
target_include_directories(RecompiledFuncs PRIVATE
    ${GAME_DIR}/include
    ${GAME_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${GAME_DIR}/lib/N64ModernRuntime/librecomp/include
    ${GAME_DIR}/lib/N64ModernRuntime/N64Recomp/include
)
file(GLOB FUNC_C_SOURCES ${GAME_DIR}/RecompiledFuncs/*.c)
file(GLOB FUNC_CXX_SOURCES ${GAME_DIR}/RecompiledFuncs/*.cpp)
target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES})

add_executable(Rush2049Headless
    main.cpp
    stubs.cpp
    ${GAME_DIR}/src/frame_rate.cpp
    ${GAME_DIR}/src/register_overlays.cpp
    ${GAME_DIR}/src/code_loading.cpp
    ${GAME_DIR}/src/libultra_extra.cpp
    ${GAME_DIR}/src/spin_yield.cpp
    ${GAME_DIR}/src/crash_handler.cpp
    ${GAME_DIR}/src/controller_pak.cpp
    ${GAME_DIR}/src/debug_probe.cpp
    ${GAME_DIR}/src/dl_dump.cpp
    ${GAME_DIR}/src/widescreen.cpp
    ${GAME_DIR}/src/rewind.cpp
    ${GAME_DIR}/src/music_thread.cpp
    ${GAME_DIR}/src/audio_resampler.cpp
    ${GAME_DIR}/src/audio_limiter.cpp
    ${GAME_DIR}/src/musyx_hle.cpp
    ${GAME_DIR}/src/gsm_decoder.cpp
    ${GAME_DIR}/src/hle/musyx.c
    ${GAME_DIR}/src/hle/audio.c
    ${GAME_DIR}/src/hle/memory.c
    ${GAME_DIR}/rsp/musyx.cpp
)
target_include_directories(Rush2049Headless PRIVATE
    shim
    ${GAME_DIR}/include
    ${GAME_DIR}/lib/N64ModernRuntime/N64Recomp/include
)
target_compile_options(Rush2049Headless PRIVATE -fno-strict-aliasing)
target_link_libraries(Rush2049Headless PRIVATE RecompiledFuncs librecomp ultramodern)
if (EMSCRIPTEN)
    target_link_options(Rush2049Headless PRIVATE -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=24 -sEXIT_RUNTIME
        -sINITIAL_MEMORY=1024MB -sSTACK_SIZE=1MB -sDEFAULT_PTHREAD_STACK_SIZE=1MB -sUSE_SDL=2
        -sENVIRONMENT=web,worker,node "SHELL:--preload-file ${HEADLESS_ROM}@/rom.z64")
endif()

# With Emscripten, RT64 draws the game on the page's canvas through WebGPU (lib/N64Recomp-browser/renderer): the build is then
# a page, Rush2049Headless.html, for a browser, and no longer runs in node. HEADLESS_RT64=OFF keeps the renderer null.
if (EMSCRIPTEN)
    option(HEADLESS_RT64 "Draw with RT64 on the page's canvas" ON)
endif()
if (HEADLESS_RT64)
    set(RT64_STATIC TRUE)
    add_subdirectory(${GAME_DIR}/lib/rt64 ${CMAKE_BINARY_DIR}/rt64)
    target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)
    add_subdirectory(${GAME_DIR}/lib/N64Recomp-browser/renderer ${CMAKE_BINARY_DIR}/web_renderer)
    target_link_libraries(Rush2049Headless PRIVATE n64recomp_browser_renderer)
    target_compile_definitions(Rush2049Headless PRIVATE HEADLESS_RT64)
    set_target_properties(Rush2049Headless PROPERTIES SUFFIX ".html")
    target_link_options(Rush2049Headless PRIVATE "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/page_arguments.js")
endif()

if (WIN32)
    include(FetchContent)
    FetchContent_Declare(sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-devel-2.30.3-VC.zip)
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(Rush2049Headless PRIVATE ${sdl2_SOURCE_DIR}/include)
    target_link_directories(Rush2049Headless PRIVATE ${sdl2_SOURCE_DIR}/lib/x64)
    target_link_libraries(Rush2049Headless PRIVATE SDL2 Winmm.lib)
    target_link_options(Rush2049Headless PRIVATE /OPT:NOICF)
    add_compile_definitions(NOMINMAX)
    add_custom_command(TARGET Rush2049Headless POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll" $<TARGET_FILE_DIR:Rush2049Headless>)
elseif (NOT EMSCRIPTEN)
    find_package(SDL2 REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(Rush2049Headless PRIVATE SDL2::SDL2 Threads::Threads)
endif()
