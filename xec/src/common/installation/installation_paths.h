#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace xenon::installation {

struct InstallationPaths {
    fs::path executable;
    fs::path root;
    fs::path profiles;
    fs::path stdlib;

    static InstallationPaths discover();

private:
    static fs::path find_executable();
    static bool is_valid_root(const fs::path& path);
};

inline const InstallationPaths& g_installation_paths() {
    static const InstallationPaths paths = InstallationPaths::discover();
    return paths;
}
}