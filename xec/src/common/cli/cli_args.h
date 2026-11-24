#pragma once

#include <string>
#include <optional>
#include <filesystem>

#include "common/config/optimisation.h"

namespace fs = std::filesystem;

namespace xenon::cli {

    struct CLIArgs {
        const std::string command; // e.g., "build", "run", "test"
        const fs::path project_dir = fs::current_path(); // Working directory for the project
        const std::optional<std::string> target; // e.g., "linux-x86_64", "macos-arm64"
        const std::optional<config::OptimisationLevel> optimisation; // "release", "debug"
        const std::optional<fs::path> output_path; // e.g., "build/output"
    };

} // namespace xenon::config