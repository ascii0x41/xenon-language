#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace xenon::config {

    namespace fs = std::filesystem;

    // The parsed command line. Turning argv into this struct is the driver's job.
    // Relative paths here are relative to the current working directory.
    struct CLIArgs {
        std::string command;                       // e.g. "build"
        fs::path project_dir = ".";                // directory containing xenon.toml
        std::string help_command;

        std::optional<std::string> target;
        std::optional<std::string> optimisation;   // "debug" or "release"
        std::optional<fs::path> output;

        bool show_color = true;
        bool show_ast = false;
    };

} // namespace xenon::config
