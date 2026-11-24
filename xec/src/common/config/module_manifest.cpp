#include "common/config/module_manifest.h"

#include "common/config/toml_access.h"

#include <algorithm>

namespace xenon::config {

    ModuleManifest load_module_manifest(const fs::path& module_dir_in) {
        using namespace detail;

        const fs::path module_dir = fs::absolute(module_dir_in).lexically_normal();
        const fs::path file = module_dir / "module.toml";
        const toml::table root = parse_file(file);
        reject_unknown_keys(root, {"name", "source", "c_source", "link_libs"}, file, "");

        fs::path directory_path = module_dir;
        if (!directory_path.has_filename()) {
            directory_path = directory_path.parent_path();
        }
        const std::string directory = directory_path.filename().string();

        ModuleManifest manifest;
        manifest.name = require_nonempty_string(root, "name", file, "");
        if (manifest.name != directory) {
            fail_at(file, *root.get("name"),
                    std::format("module name '{}' does not match its directory name '{}'", manifest.name, directory));
        }
        manifest.directory = module_dir;
        manifest.source = (module_dir / require_nonempty_string(root, "source", file, "")).lexically_normal();
        if (auto c_source = optional_nonempty_string(root, "c_source", file, "")) {
            manifest.c_source = (module_dir / *c_source).lexically_normal();
        }
        if (auto libs = optional_string_array(root, "link_libs", file, "")) {
            for (const std::string& lib : *libs) {
                if (lib.empty()) {
                    fail_at(file, *root.get("link_libs"), "'link_libs' must not contain an empty name");
                }
            }
            manifest.link_libs = std::move(*libs);
        }
        return manifest;
    }

    std::map<std::string, ModuleManifest> discover_stdlib(const fs::path& stdlib_dir) {
        std::map<std::string, ModuleManifest> modules;

        std::error_code ec;
        if (!fs::is_directory(stdlib_dir, ec)) {
            return modules;
        }

        std::vector<fs::path> directories;
        for (const auto& entry : fs::directory_iterator(stdlib_dir, ec)) {
            if (entry.is_directory(ec) && fs::is_regular_file(entry.path() / "module.toml", ec)) {
                directories.push_back(entry.path());
            }
        }
        std::sort(directories.begin(), directories.end());   // deterministic order

        for (const fs::path& directory : directories) {
            ModuleManifest manifest = load_module_manifest(directory);
            modules.emplace(manifest.name, std::move(manifest));
        }
        return modules;
    }

} // namespace xenon::config
