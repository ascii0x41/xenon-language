#pragma once

#include "common/config/config_error.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace xenon::config {

    namespace fs = std::filesystem;

    // One standard-library module, described by `stdlib/<name>/module.toml`.
    // This is discovery metadata only: there are no dependency fields.
    struct ModuleManifest {
        std::string name;
        fs::path directory;                        // absolute
        fs::path source;                           // absolute; the module's .xe (not checked for existence here)
        std::optional<fs::path> c_source;          // absolute; compiled when the module is imported
        std::vector<std::string> link_libs;        // system libraries for the linker, e.g. "m" for -lm
    };

    // Reads `<module_dir>/module.toml`. The `name` key must equal the directory
    // name. Does not read or parse the module's .xe file.
    ModuleManifest load_module_manifest(const fs::path& module_dir);

    // Finds every `<stdlib_dir>/<name>/module.toml`. A missing stdlib directory
    // gives an empty map (it only matters if something imports a stdlib module).
    std::map<std::string, ModuleManifest> discover_stdlib(const fs::path& stdlib_dir);

} // namespace xenon::config
