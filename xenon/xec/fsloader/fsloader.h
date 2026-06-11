#pragma once

#include "common/dataclasses.h"
#include "common/diagnostics.h"

#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

namespace xenon {

    using config::ConfigFile;
    
    class FsLoader {
    public:
        struct LoadedProject {
            ConfigFile config;
            SourceFile entry;  // the entry point source file
        };

        // Discover project root and load config + entry file.
        // Walks up from cwd until xenon.toml is found.
        // Returns nullptr on failure (errors logged to g_diagnostics).
        //
        // Pass a ConfigFile with desired CLI overrides; non-empty fields override
        // values from xenon.toml.
        static std::unique_ptr<LoadedProject> load_project(const ConfigFile& overrides = {});

        // Load a single .ar file directly (for single-file debug mode).
        // Errors are logged to g_diagnostics.
        static std::unique_ptr<SourceFile> load_file(const fs::path& path);

        // Parse xenon.toml at the given path, applying CLI overrides.
        // Returns true if successful, false otherwise.
        // Errors are reported via g_diagnostics.
        static bool parse_config(const fs::path& toml_path, const ConfigFile& overrides, ConfigFile& out_config);

        // Sexech upward from `start` for a file named `filename`.
        // Returns the directory containing it, or nullopt if not found.
        static std::optional<fs::path> find_project_root(
            const fs::path& start,
            const std::string& filename = "xenon.toml");

        // Resolve an import specifier (e.g., "foo::bar" or "./relative").
        // - Imports starting with "./" are relative to from_file's directory
        // - Other imports are relative to project_root
        // Returns the absolute path to the .ar file. Returns empty string on resolution failure.
        static std::string resolve_import_path(const std::string& import_spec, const std::string& from_file, const fs::path& project_root);
        
        // Read a file's contents into a string. Returns nullopt on failure.
        static std::optional<std::string> read_file(const fs::path& path);

        // Minimal TOML key-value parser — handles [section] and key = "value".
        // Returns flat map of "section.key" -> "value" on success.
        // Errors reported via g_diagnostics.
        static std::optional<std::unordered_map<std::string, std::string>>
            parse_toml_flat(const std::string& content, const fs::path& toml_path);

        static int init_project();
    };
} // namespace xenon