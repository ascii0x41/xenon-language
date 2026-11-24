#include "common/config/config_resolver.h"

#include "common/config/config_error.h"

#include <algorithm>
#include <format>
#include <set>
#include <system_error>

namespace xenon::config {

    namespace {

        // Absolute, normalised, without a trailing separator.
        fs::path normalise_directory(const fs::path& directory) {
            fs::path path = fs::absolute(directory).lexically_normal();
            if (!path.has_filename() && path.has_relative_path()) {
                path = path.parent_path();
            }
            return path;
        }

        template <typename Map>
        std::string join_keys(const Map& map) {
            std::string out;
            for (const auto& entry : map) {
                if (!out.empty()) {
                    out += ", ";
                }
                out += entry.first;
            }
            return out.empty() ? "(none)" : out;
        }

        std::string installed_environments(const fs::path& installation_dir) {
            std::vector<std::string> names;
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(installation_dir, ec)) {
                if (entry.is_directory(ec) && fs::is_regular_file(entry.path() / "environment.toml", ec)) {
                    names.push_back(entry.path().filename().string());
                }
            }
            std::sort(names.begin(), names.end());
            std::string out;
            for (const std::string& name : names) {
                if (!out.empty()) {
                    out += ", ";
                }
                out += name;
            }
            return out.empty() ? "(none)" : out;
        }

    } // namespace

    ResolvedConfigs resolve_configs(const CLIArgs& cli,
                                    const ProjectConfig& config,
                                    Environment environment,
                                    std::map<std::string, ModuleManifest> stdlibs,
                                    const fs::path& stdlib_directory) {
        const fs::path project_dir = normalise_directory(cli.project_dir);
        const fs::path config_file = project_config_path(project_dir);

        ResolvedConfigs resolved;
        resolved.project_name = config.name;
        resolved.project_directory = project_dir;

        // target: explicitly required
        const std::optional<std::string>& target_name = cli.target ? cli.target : config.build.target;
        if (!target_name) {
            throw ConfigError(
                std::format("no target specified: set 'build.target' in xenon.toml or pass --target "
                            "(environment '{}' has: {})",
                            environment.name, join_keys(environment.targets)),
                config_file);
        }
        const auto target_it = environment.targets.find(*target_name);
        if (target_it == environment.targets.end()) {
            throw ConfigError(
                std::format("environment '{}' has no target '{}' (available: {})",
                            environment.name, *target_name, join_keys(environment.targets)),
                config_file);
        }
        resolved.target = target_it->second;

        // optimisation: CLI, then project config, then default
        if (cli.optimisation) {
            const auto parsed = parse_optimisation(*cli.optimisation);
            if (!parsed) {
                throw ConfigError(std::format("--optimisation must be \"debug\" or \"release\", found \"{}\"",
                                              *cli.optimisation));
            }
            resolved.optimisation = *parsed;
        } else if (config.build.optimisation) {
            // Already validated by load_project_config, but a ProjectConfig can be built by hand.
            const auto parsed = parse_optimisation(*config.build.optimisation);
            if (!parsed) {
                throw ConfigError(std::format("'build.optimisation' must be \"debug\" or \"release\", found \"{}\"",
                                              *config.build.optimisation),
                                  config_file);
            }
            resolved.optimisation = *parsed;
        } else {
            resolved.optimisation = default_optimisation;
        }

        // output: a CLI path is relative to the working directory, a project-config path
        // (or the default) to the project directory.
        if (cli.output) {
            resolved.output = fs::absolute(*cli.output).lexically_normal();
        } else {
            resolved.output = (project_dir / config.build.output.value_or(fs::path(default_output))).lexically_normal();
        }
        if (!resolved.output.has_filename() && resolved.output.has_relative_path()) {
            resolved.output = resolved.output.parent_path();
        }

        // capabilities: environment defaults, then target overrides, then project overrides
        resolved.capabilities = environment.capabilities;
        for (const auto& [name, enabled] : resolved.target.capabilities) {
            resolved.capabilities[name] = enabled;
        }
        for (const auto& [name, enabled] : config.capabilities) {
            resolved.capabilities[name] = enabled;
        }

        // source files: absolute, existing, no duplicates
        if (config.source_files.empty()) {
            throw ConfigError("'project.source_files' must list at least one file", config_file);
        }
        std::set<fs::path> seen;
        for (const fs::path& relative : config.source_files) {
            const fs::path absolute = (project_dir / relative).lexically_normal();
            std::error_code ec;
            if (!fs::is_regular_file(absolute, ec)) {
                throw ConfigError(std::format("source file '{}' not found (resolved to '{}')",
                                              relative.string(), absolute.string()),
                                  config_file);
            }
            if (!seen.insert(fs::weakly_canonical(absolute, ec)).second) {
                throw ConfigError(std::format("source file '{}' is listed more than once", relative.string()),
                                  config_file);
            }
            resolved.source_files.push_back(absolute);
        }

        resolved.environment = std::move(environment);
        resolved.stdlib_directory = stdlib_directory;
        resolved.stdlibs = std::move(stdlibs);
        return resolved;
    }

    ResolvedConfigs load_and_resolve(const CLIArgs& cli, const fs::path& installation_dir_in) {
        const fs::path installation_dir = normalise_directory(installation_dir_in);
        const fs::path project_dir = normalise_directory(cli.project_dir);
        const fs::path config_file = project_config_path(project_dir);

        const ProjectConfig config = load_project_config(config_file);

        const std::string environment_name = config.build.environment.value_or(default_environment);
        if (environment_name.empty() || environment_name == "." || environment_name == ".." ||
            environment_name.find_first_of("/\\") != std::string::npos) {
            throw ConfigError(std::format("invalid environment name '{}'", environment_name), config_file);
        }

        const fs::path environment_dir = installation_dir / environment_name;
        std::error_code ec;
        if (!fs::is_regular_file(environment_dir / "environment.toml", ec)) {
            throw ConfigError(std::format("environment '{}' is not installed (no environment.toml in '{}'). Installed environments: {}",
                                          environment_name, environment_dir.string(), installed_environments(installation_dir)),
                              config_file);
        }

        Environment environment = load_environment(environment_dir);
        const fs::path stdlib_directory = installation_dir / "stdlib";
        auto stdlibs = discover_stdlib(stdlib_directory);

        return resolve_configs(cli, config, std::move(environment), std::move(stdlibs), stdlib_directory);
    }

} // namespace xenon::config
