// rush2049::replace_file checks, run by CI: a plain path is written, and a symlinked one (relative, absolute, a chain,
// or one whose file does not exist yet) writes the file the link names and leaves the link in place, with no .tmp left.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

#include "save_file.h"

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

std::string contents(const fs::path& path) {
    std::ifstream file{ path, std::ios::binary };
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

void write(const fs::path& path, const std::string& text) {
    std::ofstream file{ path, std::ios::binary };
    file << text;
}

bool replace(const fs::path& path, const std::string& text) {
    std::string error = rush2049::replace_file(path, text.data(), text.size());
    if (!error.empty()) {
        printf("     %s\n", error.c_str());
    }
    return error.empty();
}

} // namespace

int main() {
    fs::path root = fs::temp_directory_path() / ("rush2049_save_file_test_" + std::to_string(std::random_device{}()));
    fs::path saves = root / "saves";
    fs::path synced = root / "synced";
    fs::create_directories(saves);
    fs::create_directories(synced);

    check(replace(saves / "plain.mpk", "one") && contents(saves / "plain.mpk") == "one", "a new plain file is written");
    check(replace(saves / "plain.mpk", "two") && contents(saves / "plain.mpk") == "two", "a plain file is replaced");
    check(!fs::exists(saves / "plain.mpk.tmp"), "no .tmp is left beside a plain file");

    write(synced / "relative.mpk", "old");
    fs::create_symlink("../synced/relative.mpk", saves / "relative.mpk");
    check(replace(saves / "relative.mpk", "new"), "a relative link is written through");
    check(fs::is_symlink(saves / "relative.mpk"), "the relative link is still a link");
    check(contents(synced / "relative.mpk") == "new", "the relative link's file holds the new data");
    check(!fs::exists(saves / "relative.mpk.tmp") && !fs::exists(synced / "relative.mpk.tmp"), "no .tmp is left");

    write(synced / "absolute.mpk", "old");
    fs::create_symlink(fs::absolute(synced / "absolute.mpk"), saves / "absolute.mpk");
    check(replace(saves / "absolute.mpk", "new") && fs::is_symlink(saves / "absolute.mpk") &&
              contents(synced / "absolute.mpk") == "new", "an absolute link keeps its link and gets the data");

    write(synced / "chain.mpk", "old");
    fs::create_symlink("chain.mpk", synced / "chain_link.mpk");
    fs::create_symlink("../synced/chain_link.mpk", saves / "chain.mpk");
    check(replace(saves / "chain.mpk", "new") && fs::is_symlink(saves / "chain.mpk") &&
              fs::is_symlink(synced / "chain_link.mpk") && contents(synced / "chain.mpk") == "new",
          "a chain of links keeps every link and the last file gets the data");

    fs::create_symlink("../synced/missing.mpk", saves / "dangling.mpk");
    check(replace(saves / "dangling.mpk", "new") && fs::is_symlink(saves / "dangling.mpk") &&
              contents(synced / "missing.mpk") == "new", "a link to a file not there yet creates that file");

    fs::create_symlink("loop_b.mpk", saves / "loop_a.mpk");
    fs::create_symlink("loop_a.mpk", saves / "loop_b.mpk");
    rush2049::follow_links(saves / "loop_a.mpk");
    check(true, "a loop of links ends");

    std::error_code ec;
    fs::remove_all(root, ec);
    printf("%s\n", failures == 0 ? "all passed" : "FAILED");
    return failures == 0 ? 0 : 1;
}
