#pragma once

#include "common/config/config_error.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace xenon::config {

    namespace fs = std::filesystem;

    // The [build] table of xenon.toml. Omitted values stay empty until the
    // resolver applies CLI overrides and defaults.
    struct BuildConfig {
        std::optional<std::string> environment;
        std::optional<std::string> target;
        std::optional<std::string> optimisation;   // validated: "debug" or "release"
        std::optional<fs::path> output;            // as written; relative to the project directory
    };

    // What the user wrote in xenon.toml, nothing more.
    struct ProjectConfig {
        std::string name;
        std::vector<fs::path> source_files;        // as written; relative to the project directory
        BuildConfig build;
        std::map<std::string, bool> capabilities;  // [capabilities] overrides
    };

    inline fs::path project_config_path(const fs::path& project_dir) {
        return project_dir / "xenon.toml";
    }

    // Reads and validates a xenon.toml. Throws ConfigError (with file, line and
    // column where known) on a missing file, bad TOML, a wrong type, a missing
    // required key or an unknown key.
    ProjectConfig load_project_config(const fs::path& config_file);

} // namespace xenon::config
