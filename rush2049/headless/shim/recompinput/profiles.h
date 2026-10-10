// Headless stand-in for RecompFrontend's recompinput/profiles.h (headless/stubs.cpp): only what rewind.cpp uses.
#pragma once
namespace recompinput {
    enum class GameInput { REWIND };
    namespace profiles {
        bool get_game_input_held(int player_index, GameInput input);
    }
}
