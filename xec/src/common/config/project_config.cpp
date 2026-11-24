#include "common/config/project_config.h"

#include "common/config/optimisation.h"
#include "common/config/toml_access.h"

namespace xenon::config {

    ProjectConfig load_project_config(const fs::path& config_file) {
        using namespace detail;

        std::error_code ec;
        if (!fs::is_regular_file(config_file, ec)) {
            throw ConfigError("No xenon.toml found. Are you inside a Xenon project?", config_file);
        }

        const toml::table root = parse_file(config_file);
        reject_unknown_keys(root, {"project", "build", "capabilities"}, config_file, "");

        // [project]
        const toml::table* project = optional_table(root, "project", config_file, "");
        if (project == nullptr) {
            fail(config_file, "missing required table [project]");
        }
        // `version` and `authors` are accepted (the sample xenon.toml has them) but unused.
        reject_unknown_keys(*project, {"name", "version", "authors", "source_files"}, config_file, "project.");

        ProjectConfig config;
        config.name = require_nonempty_string(*project, "name", config_file, "project.");

        auto files = optional_string_array(*project, "source_files", config_file, "project.");
        if (!files) {
            fail_at(config_file, *project, "missing required key 'project.source_files'");
        }
        if (files->empty()) {
            fail_at(config_file, *project->get("source_files"), "'project.source_files' must list at least one file");
        }
        for (const std::string& file : *files) {
            if (file.empty()) {
                fail_at(config_file, *project->get("source_files"),
                        "'project.source_files' must not contain an empty path");
            }
            config.source_files.emplace_back(file);
        }

        // [build]
        if (const toml::table* build = optional_table(root, "build", config_file, "")) {
            reject_unknown_keys(*build, {"environment", "target", "optimisation", "output"}, config_file, "build.");
            config.build.environment = optional_nonempty_string(*build, "environment", config_file, "build.");
            config.build.target = optional_nonempty_string(*build, "target", config_file, "build.");
            config.build.optimisation = optional_string(*build, "optimisation", config_file, "build.");
            if (config.build.optimisation && !parse_optimisation(*config.build.optimisation)) {
                fail_at(config_file, *build->get("optimisation"),
                        std::format("'build.optimisation' must be \"debug\" or \"release\", found \"{}\"",
                                    *config.build.optimisation));
            }
            if (auto output = optional_nonempty_string(*build, "output", config_file, "build.")) {
                config.build.output = fs::path(*output);
            }
        }

        // [capabilities]
        if (const toml::table* capabilities = optional_table(root, "capabilities", config_file, "")) {
            config.capabilities = read_bool_table(*capabilities, config_file, "capabilities.");
        }

        return config;
    }

} // namespace xenon::config
