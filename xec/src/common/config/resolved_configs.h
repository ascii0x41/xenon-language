#pragma once

#include "common/config/optimisation.h"
#include "common/config/environment.h"
#include "common/config/module_manifest.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace xenon::config {

    namespace fs = std::filesystem;

    // The single, complete answer to "what are we building?". Produced once by
    // Phase 1 and passed by const reference to every later phase. Nothing in it
    // is optional or relative: every path is absolute.
    struct ResolvedConfigs {
        std::string project_name;
        fs::path project_directory;

        std::vector<fs::path> source_files;        // absolute, existing, no duplicates

        Environment environment;
        Target target;                             // a copy of environment.targets[<selected>]

        Optimisation optimisation = Optimisation::debug;
        fs::path output;                           // directory for all build products

        // environment defaults, then target overrides, then project overrides.
        // Metadata only in v1: nothing reads it.
        std::map<std::string, bool> capabilities;

        fs::path stdlib_directory;
        std::map<std::string, ModuleManifest> stdlibs;   // everything discovered, imported or not
    };

} // namespace xenon::config
