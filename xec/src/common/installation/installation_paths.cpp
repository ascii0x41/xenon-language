#include "installation_paths.h"

#include <cstdlib>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#elif defined(__linux__)
    #include <unistd.h>
#endif

namespace xenon::installation {

namespace fs = std::filesystem;


// ------------------------------------------------------------
// Find the running executable
// ------------------------------------------------------------

fs::path InstallationPaths::find_executable() {

#ifdef _WIN32

    std::vector<wchar_t> buffer(1024);

    while (true) {
        DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );

        if (length == 0)
            throw std::runtime_error("Could not locate Xenon executable.");

        if (length < buffer.size() - 1)
            return fs::canonical(fs::path(buffer.data()));

        buffer.resize(buffer.size() * 2);
    }

#elif defined(__APPLE__)

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);

    std::vector<char> buffer(size);

    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        throw std::runtime_error("Could not locate Xenon executable.");

    return fs::canonical(fs::path(buffer.data()));

#elif defined(__linux__)

    std::vector<char> buffer(1024);

    while (true) {
        ssize_t length = readlink(
            "/proc/self/exe",
            buffer.data(),
            buffer.size()
        );

        if (length < 0)
            throw std::runtime_error("Could not locate Xenon executable.");

        if (static_cast<size_t>(length) < buffer.size()) {
            return fs::canonical(
                fs::path(std::string(buffer.data(), static_cast<size_t>(length)))
            );
        }

        buffer.resize(buffer.size() * 2);
    }

#else

    throw std::runtime_error(
        "Executable discovery is not supported on this platform."
    );

#endif
}


// ------------------------------------------------------------
// Validate an installation root
// ------------------------------------------------------------

bool InstallationPaths::is_valid_root(const fs::path& path) {

    return fs::is_directory(path / "environments")
        && fs::is_directory(path / "stdlib");
}


// ------------------------------------------------------------
// Discover installation directories
// ------------------------------------------------------------

InstallationPaths InstallationPaths::discover() {

    fs::path root;

    if (const char* home = std::getenv("XENON_HOME")) {
        root = fs::canonical(home);
    } else {
        root = DEFAULT_HOME;
    }

    if (!fs::is_directory(root / "environments") ||
        !fs::is_directory(root / "stdlib")) {
        throw std::runtime_error(
            "Invalid Xenon installation directory: " + root.string()
        );
    }

    return InstallationPaths{
        .executable = find_executable(),
        .root = root,
        .environments = root / "environments",
        .stdlib = root / "stdlib",
    };
}


}