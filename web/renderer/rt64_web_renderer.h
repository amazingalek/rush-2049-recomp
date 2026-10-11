// RT64 as the runtime's renderer in a browser (rt64_web_renderer.cpp says how).

#pragma once

#include <cstdint>
#include <memory>

#include "ultramodern/renderer_context.hpp"

namespace recompweb {
    // For ultramodern::renderer::callbacks_t::create_render_context: RT64 drawing the game's display lists on the page's
    // canvas (id "canvas", as Emscripten's default page has). The window handle is not used.
    std::unique_ptr<ultramodern::renderer::RendererContext> create_rt64_renderer(uint8_t *rdram);
}
