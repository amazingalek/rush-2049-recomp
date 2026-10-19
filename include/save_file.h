#ifndef __RUSH2049_SAVE_FILE_H__
#define __RUSH2049_SAVE_FILE_H__

#include <cstddef>
#include <filesystem>
#include <string>

namespace rush2049 {
    // The file a path names: a symlink, or a chain of them, is followed to its end (a relative target is relative to
    // the link's folder). Anything else comes back as it is.
    std::filesystem::path follow_links(std::filesystem::path path);

    // Writes `size` bytes to `path` so a crash never leaves half a file: to a .tmp beside the file, then renamed over
    // it. A symlinked path is followed first, so the file it points at is replaced and the link stays (a player who
    // links a save into a synced folder keeps the link). Returns an empty string, or what went wrong.
    std::string replace_file(const std::filesystem::path& path, const void* data, size_t size);
}

#endif
