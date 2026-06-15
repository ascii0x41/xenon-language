#include "fsloader.h"
#include "common/diagnostics.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <format>
#include <functional>
#include <toml++/toml.h>

#define SAMPLE_CONFIG_CONTENT \
R"config([project]
name = "my_project"
version = "0.1.0"
entry = "src/main.xe"
authors = ["Your Name"]

[build]
output = "build/"
target = "x86_64-linux"
optimisation = "debug"
)config";

#define SAMPLE_MAIN_CONTENT \
R"main(func main() -> i32 {
    println("Hello, World!");
    return 0;
}
)main"


namespace xenon {

    // ============================================================================
    // UTILITIES
    // ============================================================================

    std::optional<std::string> FsLoader::read_file(const fs::path& path) {
        std::ifstream f(path);
        if (!f) return std::nullopt;
        std::ostringstream buf;
        buf << f.rdbuf();
        return buf.str();
    }

    // ============================================================================
    // TOML PARSING (using toml++)
    // ============================================================================

    std::optional<std::unordered_map<std::string, std::string>>
    FsLoader::parse_toml_flat(const std::string& content, const fs::path& toml_path) {
        try {
            auto tbl = toml::parse(content);
            std::unordered_map<std::string, std::string> result;

            // Recursively convert TOML table to flat key-value map
            std::function<void(const toml::table&, const std::string&)> flatten =
                [&](const toml::table& table, const std::string& prefix) {
                    for (auto& [key, node] : table) {
                        std::string full_key = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);
                        
                        if (node.is_table()) {
                            flatten(*node.as_table(), full_key);
                        } else if (node.is_array()) {
                            // Convert array to string representation
                            std::string array_str = "[";
                            auto arr = node.as_array();
                            for (size_t i = 0; i < arr->size(); ++i) {
                                if (i > 0) array_str += ", ";
                                auto& elem = (*arr)[i];
                                if (elem.is_string()) {
                                    array_str += "\"" + std::string(elem.as_string()->get()) + "\"";
                                } else if (elem.is_integer()) {
                                    array_str += std::to_string(elem.as_integer()->get());
                                } else if (elem.is_floating_point()) {
                                    array_str += std::to_string(elem.as_floating_point()->get());
                                } else if (elem.is_boolean()) {
                                    array_str += elem.as_boolean()->get() ? "true" : "false";
                                }
                            }
                            array_str += "]";
                            result[full_key] = array_str;
                        } else if (node.is_string()) {
                            result[full_key] = std::string(node.as_string()->get());
                        } else if (node.is_integer()) {
                            result[full_key] = std::to_string(node.as_integer()->get());
                        } else if (node.is_floating_point()) {
                            result[full_key] = std::to_string(node.as_floating_point()->get());
                        } else if (node.is_boolean()) {
                            result[full_key] = node.as_boolean()->get() ? "true" : "false";
                        }
                    }
                };

            flatten(tbl, "");
            return result;
        } catch (const toml::parse_error& e) {
            g_diagnostics.error(std::format("{}:{}: {}", toml_path.string(), e.source().begin.line, e.description()));
            return std::nullopt;
        }
    }

    // ============================================================================
    // PROJECT ROOT DISCOVERY
    // ============================================================================

    bool FsLoader::parse_config(const fs::path& toml_path, const ConfigFile& overrides, ConfigFile& out_config) {
        using config::OptimisationLevel;
        using config::parse_optimisation_level;
        using config::parse_warning_level;
        
        auto source = FsLoader::read_file(toml_path);
        if (!source) {
            g_diagnostics.error(std::format("Cannot read {}", toml_path.string()));
            return false;
        }

        try {
            auto tbl = toml::parse(*source);
            ConfigFile config = overrides;

            // [project] section — name, version, entry are required
            if (config.project.name.empty()) {
                auto node = tbl["project"]["name"];
                if (!node || !node.is_string()) {
                    g_diagnostics.error("xenon.toml: missing required field 'project.name'");
                    return false;
                }
                config.project.name = std::string(node.as_string()->get());
            }

            if (config.project.version.empty()) {
                auto node = tbl["project"]["version"];
                if (!node || !node.is_string()) {
                    g_diagnostics.error("xenon.toml: missing required field 'project.version'");
                    return false;
                }
                config.project.version = std::string(node.as_string()->get());
            }

            if (config.project.entry.empty()) {
                auto node = tbl["project"]["entry"];
                if (!node || !node.is_string()) {
                    g_diagnostics.error("xenon.toml: missing required field 'project.entry'");
                    return false;
                }
                config.project.entry = std::string(node.as_string()->get());
            }

            // Optional: [project] authors (array)
            if (config.project.authors.empty()) {
                auto node = tbl["project"]["authors"];
                if (node && node.is_array()) {
                    auto arr = node.as_array();
                    for (auto& elem : *arr) {
                        if (auto s = elem.as_string()) {
                            config.project.authors.push_back(std::string(s->get()));
                        }
                    }
                }
            }

            // [build] section — all optional with defaults
            if (config.build.output_dir == "build/") {
                auto node = tbl["build"]["output_dir"];
                if (node && node.is_string()) {
                    config.build.output_dir = std::string(node.as_string()->get());
                }
            }

            if (config.build.target_triple == "x86_64-linux") {
                auto node = tbl["build"]["target_triple"];
                if (node && node.is_string()) {
                    config.build.target_triple = std::string(node.as_string()->get());
                }
            }

            if (config.build.opt_level == OptimisationLevel::DEBUG) {
                auto node = tbl["build"]["opt_level"];
                if (node && node.is_string()) {
                    config.build.opt_level = parse_optimisation_level(std::string(node.as_string()->get()));
                }
            }

            // [warnings] section — maps warning name to level (optional)
            auto warnings_node = tbl["warnings"];
            if (warnings_node && warnings_node.is_table()) {
                auto warnings = warnings_node.as_table();
                for (auto& [key, val] : *warnings) {
                    if (val.is_string()) {
                        config.warnings.warnings[std::string(key)] = parse_warning_level(std::string(val.as_string()->get()));
                    }
                }
            }

            // [pedantic] section
            auto pedantic_node = tbl["pedantic"];
            if (pedantic_node) {
                if (pedantic_node.is_table()) {
                    auto enabled_node = (*pedantic_node.as_table())["enabled"];
                    if (enabled_node && enabled_node.is_boolean()) {
                        config.pedantic = enabled_node.as_boolean()->get();
                    }
                } else if (pedantic_node.is_boolean()) {
                    config.pedantic = pedantic_node.as_boolean()->get();
                }
            }

            out_config = std::move(config);
            return true;

        } catch (const toml::parse_error& e) {
            g_diagnostics.error(std::format("{}:{}: {}", toml_path.string(), e.source().begin.line, e.description()));
            return false;
        }
    }

    // ============================================================================
    // PROJECT ROOT DISCOVERY
    // ============================================================================

    std::optional<fs::path> FsLoader::find_project_root(const fs::path& start,
                                                        const std::string& filename) {
        fs::path current = fs::absolute(start);

        while (true) {
            if (fs::exists(current / filename))
                return current;

            fs::path parent = current.parent_path();
            if (parent == current) break;  // reached filesystem root
            current = parent;
        }

        return std::nullopt;
    }

    // ============================================================================
    // SINGLE FILE LOADER
    // ============================================================================

    std::unique_ptr<SourceFile> FsLoader::load_file(const fs::path& path) {
        auto abs = fs::absolute(path);
        if (!fs::exists(abs)) {
            g_diagnostics.error(std::format("File not found: {}", abs.string()));
            return nullptr;
        }
        if (!fs::is_regular_file(abs)) {
            g_diagnostics.error(std::format("Not a regular file: {}", abs.string()));
            return nullptr;
        }

        auto source = FsLoader::read_file(abs);
        if (!source) {
            g_diagnostics.error(std::format("Cannot read file: {}", abs.string()));
            return nullptr;
        }

        auto sf = std::make_unique<SourceFile>();
        sf->path     = abs;
        sf->rel_path = path.string();
        sf->source   = std::move(*source);
        return sf;
    }

    // ============================================================================
    // PROJECT LOADER
    // ============================================================================

    std::unique_ptr<FsLoader::LoadedProject> FsLoader::load_project(const ConfigFile& overrides) {
        // Find project root by walking up from cwd
        auto root = FsLoader::find_project_root(fs::current_path());
        if (!root) {
            g_diagnostics.error(
                "No xenon.toml found. Are you inside a xenon project?");
            g_diagnostics.note(
                "  Run from a directory containing xenon.toml, or use:\n"
                "    xec --print-ast <file.xe>  for single-file mode.");
            return nullptr;
        }

        // Parse config
        ConfigFile config;
        if (!parse_config(*root / "xenon.toml", overrides, config)) {
            return nullptr;
        }

        // Set derived paths
        config.project_root = *root;
        config.entry_path   = *root / config.project.entry;

        // Validate entry file exists
        if (!fs::exists(config.entry_path)) {
            g_diagnostics.error(std::format(
                "Entry file '{}' not found (resolved to '{}')",
                config.project.entry, config.entry_path.string()));
            return nullptr;
        }

        // Load entry file
        auto entry = FsLoader::load_file(config.entry_path);
        if (!entry) {
            return nullptr;  // error already logged by load_file
        }
        entry->rel_path = fs::relative(entry->path, config.project_root).string();

        return std::make_unique<LoadedProject>(LoadedProject{
            std::move(config),
            std::move(*entry)
        });
    }

    // ============================================================================
    // IMPORT RESOLUTION
    // ============================================================================

    std::string FsLoader::resolve_import_path(const std::string& import_spec, const std::string& from_file, const fs::path& project_root) {
        fs::path from_dir = fs::path(from_file).parent_path();
        std::cout << "Finding import path for " << import_spec << " relative to project root \n";

        // Helper to add .xe extension only if not already present
        auto ensure_ar_extension = [](const std::string& path_str) -> std::string {
            if (path_str.length() >= 3 && path_str.substr(path_str.length() - 3) == ".xe") {
                return path_str;  // Already has .xe
            }
            return path_str + ".xe";
        };

        // Relative imports (starting with "./"): resolve relative to current file's directory
        if (import_spec.rfind("./", 0) == 0) {
            std::string rel_part = import_spec.substr(2);
            fs::path candidate = from_dir / ensure_ar_extension(rel_part);
            std::cout << "Trying relative candidate path: " << candidate.string() << "\n";
            if (fs::exists(candidate)) {
                return fs::absolute(candidate).string();
            }
            return "";  // Relative imports don't fall back to project root
        }
        
        // Non-relative imports: resolve relative to project root
        std::string path_spec = import_spec;
        std::replace(path_spec.begin(), path_spec.end(), ':', '/');
        std::cout << "Path spec: " << path_spec << "\n";
        
        fs::path candidate = project_root / ensure_ar_extension(path_spec);
        std::cout << "Trying candidate path: " << candidate.string() << "\n";
        if (fs::exists(candidate)) {
            return fs::absolute(candidate).string();
        }
        
        return "";
    }

    int FsLoader::init_project() {
        // Check if xenon.toml already exists in current directory
        fs::path current = fs::current_path();
        fs::path toml_path = current / "xenon.toml";

        if (fs::exists(toml_path)) {
            g_diagnostics.error("Error: xenon.toml already exists in the current directory.");
            return g_diagnostics.exit_gracefully();
        }

        // Create src/ directory
        fs::path src_dir = current / "src";

        if (!fs::exists(src_dir)) {
            fs::create_directory(src_dir);
        }


        // Write xenon.toml
        {
            std::ofstream toml_file(toml_path);

            if (!toml_file) {
                std::cerr << "Error: failed to create xenon.toml\n";
                return 1;
            }

            toml_file << SAMPLE_CONFIG_CONTENT;
        }

        // Template src/main.xe content
        fs::path main_path = src_dir / "main.xe";


        // Write src/main.xe
        {
            std::ofstream main_file(main_path);

            if (!main_file) {
                std::cerr << "Error: failed to create src/main.xe\n";
                return 1;
            }

            main_file << SAMPLE_MAIN_CONTENT;
        }

        g_diagnostics.note("Project initialised successfully.");
        return g_diagnostics.exit_gracefully();
    }
} // namespace xenon
