# recomp_web_frontend: what a recomp's executable built on RecompFrontend needs to be a page, as link options. Compile
# everything, libraries too, with -pthread -fwasm-exceptions -msimd128 "-DXXH_HAS_INCLUDE(x)=0" (add_compile_options;
# the last keeps xxHash off a NEON path that doesn't compile in C++ there), and make the executable a page with
# recomp_web_frontend_page(<target>) below, which links it with this and copies coi_serviceworker.js beside it. It brings:
# - threads as pthreads, with main() on one of them (-sPROXY_TO_PTHREAD), so the page's own thread is free for the
#   WebGPU device (plume_device_thread.h), the file picker and the browser's events,
# - the page, shell.html: a canvas filling the window, which SDL's window is and RT64 draws on, and a status line,
# - persistent_home.js: HOME (/home/web_user, where RecompFrontend keeps settings, saves and the ROM it was given)
#   kept in the browser's IndexedDB,
# - sdl_audio.js: SDL2's audio opened from a thread, which Emscripten's port can't do by itself,
# - the page's WebGPU device (plume_webgpu_device.js).
# A page with threads must be cross-origin isolated: served with Cross-Origin-Opener-Policy: same-origin and
# Cross-Origin-Embedder-Policy: require-corp, or from a host that can't set headers (GitHub Pages) with
# coi_serviceworker.js beside it, which adds them.

add_library(recomp_web_frontend INTERFACE)
target_link_options(recomp_web_frontend INTERFACE
    -pthread -fwasm-exceptions -sUSE_SDL=2 -sUSE_FREETYPE=1
    -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=32 -sINITIAL_MEMORY=1536MB -sSTACK_SIZE=1MB -sDEFAULT_PTHREAD_STACK_SIZE=1MB
    -lidbfs.js
    "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$addRunDependency,$removeRunDependency"
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/persistent_home.js"
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/sdl_audio.js"
    "--pre-js=${rt64_SOURCE_DIR}/src/contrib/plume/plume_webgpu_device.js"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html")

# Makes <target> a page, <target>.html, with the service worker beside it.
function(recomp_web_frontend_page target)
    target_link_libraries(${target} PRIVATE recomp_web_frontend)
    set_target_properties(${target} PROPERTIES SUFFIX ".html")
    # The page's scripts go into it at link time, so a change to one relinks it.
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/persistent_home.js" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sdl_audio.js"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shell.html" "${rt64_SOURCE_DIR}/src/contrib/plume/plume_webgpu_device.js")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/coi_serviceworker.js"
            $<TARGET_FILE_DIR:${target}>)
endfunction()
