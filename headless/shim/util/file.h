// Headless stand-in for RecompFrontend's util/file.h (headless/stubs.cpp).
#pragma once
#include <filesystem>
namespace recompui::file {
    std::filesystem::path get_app_folder_path();
}
