// Writing a save file in one piece, through a symlink. Pure code with no game dependencies: tests/save_file_test.cpp
// checks it with real links.
//
// The write goes to <file>.tmp and is renamed over <file>. A rename replaces whatever directory entry it names, and
// for a symlink that is the link itself, so the path is resolved first and the .tmp is written beside the link's
// target: the rename stays on one filesystem (a synced folder can be on another drive) and the link is left alone.

#include "save_file.h"

#include <fstream>
#include <system_error>

namespace rush2049 {
    std::filesystem::path follow_links(std::filesystem::path path) {
        std::error_code ec;
        // 40 is Linux's own limit on a chain of links; it also ends a loop of them.
        for (int i = 0; i < 40 && std::filesystem::is_symlink(path, ec); i++) {
            std::filesystem::path target = std::filesystem::read_symlink(path, ec);
            if (ec) {
                break;
            }
            path = target.is_absolute() ? target : path.parent_path() / target;
        }
        return path;
    }

    std::string replace_file(const std::filesystem::path& path, const void* data, size_t size) {
        std::filesystem::path target = follow_links(path);
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
        std::filesystem::path temp = target;
        temp += ".tmp";
        {
            std::ofstream file{ temp, std::ios::binary };
            file.write(static_cast<const char*>(data), (std::streamsize)size);
            if (!file.good()) {
                return "could not write " + temp.string();
            }
        }
        std::filesystem::rename(temp, target, ec);
        if (ec) {
            return "could not replace " + target.string() + ": " + ec.message();
        }
        return {};
    }
}
