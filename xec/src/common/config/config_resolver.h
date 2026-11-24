#pragma once

#include "common/config/cli_args.h"
#include "common/config/config_error.h"
#include "common/config/environment.h"
#include "common/config/project_config.h"
#include "common/config/resolved_configs.h"
#include "common/config/module_manifest.h"

#include <filesystem>
#include <map>
#include <string>

namespace xenon::config {

    namespace fs = std::filesystem;

    // Built-in defaults.
    inline constexpr const char* default_environment = "xenon";
    inline constexpr const char* default_output = "build/";
    inline constexpr Optimisation default_optimisation = Optimisation::debug;

    // The environment to load: CLI, then project config, then `xenon`. Needed before
    // environment.toml can be loaded, because it decides which file to read.
    std::string resolve_environment_name(const CLIArgs& cli, const ProjectConfig& config);

    // Merges everything that has already been loaded into one ResolvedConfigs.
    //   environment / optimisation / output : CLI, then project config, then default
    //   target                          : CLI, then project config; neither is an error
    //   capabilities                    : environment, then target, then project
    // Throws ConfigError for an unknown target, a bad optimisation value, and
    // source files that are missing, duplicated or absent.
    ResolvedConfigs resolve_configs(const CLIArgs& cli,
                                    const ProjectConfig& config,
                                    Environment environment,
                                    std::map<std::string, ModuleManifest> stdlibs,
                                    const fs::path& stdlib_directory);

    // Phase 1 end to end: read <project_dir>/xenon.toml, load the selected
    // environment from <installation_dir>/<environment>/environment.toml, discover
    // <installation_dir>/stdlib/*/module.toml and resolve everything.
    ResolvedConfigs load_and_resolve(const CLIArgs& cli, const fs::path& installation_dir);

} // namespace xenon::config
