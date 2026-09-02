#include "driver/compiler_driver.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/analyser.h"
#include "toml/toml_parser.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <format>

namespace xenon::driver {

    namespace {

    constexpr const char* SAMPLE_CONFIG_CONTENT =
R"([project]
name = "my_project"
version = "0.1.0"
authors = ["Your Name"]

source_files = [
    "src/main.xe",
]

[build]
output = "build/"
target = "x86_64-linux"
optimisation = "debug"
)";

    constexpr const char* SAMPLE_MAIN_CONTENT =
R"(module main;

func main() -> i32 {
    println("Hello, World!");
    return 0;
}
)";

    std::vector<std::string> parse_string_array(const std::string& value) {
        std::vector<std::string> authors;
        if (value.size() < 2 || value.front() != '[' || value.back() != ']') {
            return authors;
        }

        std::string inner = value.substr(1, value.size() - 2);
        std::string current;
        bool in_quotes = false;

        for (char c : inner) {
            if (c == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (c == ',' && !in_quotes) {
                if (!current.empty()) {
                    authors.push_back(current);
                    current.clear();
                }
                continue;
            }
            if (in_quotes || (!std::isspace(static_cast<unsigned char>(c)) && c != ',')) {
                current += c;
            }
        }

        if (!current.empty()) {
            authors.push_back(current);
        }

        return authors;
    }

    } // namespace

    // ============================================================
    // ModuleNamespaceTree
    // ============================================================

    std::vector<std::string> ModuleNamespaceTree::split_name(const std::string& name) {
        std::vector<std::string> parts;
        std::string current;
        for (size_t i = 0; i < name.size(); ++i) {
            if (name[i] == ':' && i + 1 < name.size() && name[i + 1] == ':') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
                ++i;
                continue;
            }
            if (name[i] == ':') {
                continue;
            }
            current.push_back(name[i]);
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        return parts;
    }

    void ModuleNamespaceTree::collect_module_names(
        const std::unordered_map<std::string, Module>& children,
        std::vector<std::string>& out,
        const std::string& prefix)
    {
        for (const auto& [name, child] : children) {
            const std::string qualified = prefix.empty() ? name : prefix + "::" + name;
            out.push_back(qualified);
            collect_module_names(child.children, out, qualified);
        }
    }

    void ModuleNamespaceTree::add_module(Module module) {
        if (!module.ast || module.ast->module_name.empty()) {
            return;
        }

        const auto parts = split_name(module.ast->module_name);
        std::unordered_map<std::string, Module>* current = &modules_;

        for (size_t i = 0; i < parts.size(); ++i) {
            auto& node = (*current)[parts[i]];
            if (i == parts.size() - 1) {
                node.path = std::move(module.path);
                node.ast = std::move(module.ast);
                return;
            }
            current = &node.children;
        }
    }

    Module* ModuleNamespaceTree::get_module(const std::string& name) {
        if (name.empty()) {
            return nullptr;
        }

        std::unordered_map<std::string, Module>* current = &modules_;
        for (const auto& part : split_name(name)) {
            auto it = current->find(part);
            if (it == current->end()) {
                return nullptr;
            }
            if (part == split_name(name).back()) {
                return &it->second;
            }
            current = &it->second.children;
        }

        return nullptr;
    }

    const Module* ModuleNamespaceTree::get_module(const std::string& name) const {
        if (name.empty()) {
            return nullptr;
        }

        const std::unordered_map<std::string, Module>* current = &modules_;
        const auto parts = split_name(name);
        for (size_t i = 0; i < parts.size(); ++i) {
            auto it = current->find(parts[i]);
            if (it == current->end()) {
                return nullptr;
            }
            if (i == parts.size() - 1) {
                return &it->second;
            }
            current = &it->second.children;
        }

        return nullptr;
    }

    void ModuleNamespaceTree::report_unresolved_dependency(
        const std::string& importer_name,
        const std::string& dependency_name,
        const fs::path& importer_path) const
    {
        g_diagnostics.error(
            std::format("unresolved module '{}' imported by '{}'", dependency_name, importer_name),
            common::SourceLocation{0, 0, importer_path.string()});
    }

    bool ModuleNamespaceTree::dfs_validate_module(
        const std::string& name,
        std::unordered_map<std::string, int>& state,
        std::vector<std::string>& path,
        std::unordered_map<std::string, size_t>& path_index)
    {
        Module* node = get_module(name);
        if (!node) {
            return false;
        }

        state[name] = 1;
        path_index[name] = path.size();
        path.push_back(name);

        if (node->ast) {
            for (const auto& dep : node->ast->dependencies) {
                const Module* dep_module = get_module(dep);
                if (!dep_module) {
                    report_unresolved_dependency(name, dep, node->path);
                    return false;
                }

                const auto dep_state = state.find(dep);
                if (dep_state != state.end() && dep_state->second == 1) {
                    std::vector<std::string> cycle;
                    const auto start = path_index[dep];
                    cycle.reserve(path.size() - start + 1);
                    for (size_t i = start; i < path.size(); ++i) {
                        cycle.push_back(path[i]);
                    }
                    cycle.push_back(dep);

                    std::string cycle_text;
                    for (size_t i = 0; i < cycle.size(); ++i) {
                        if (i != 0) {
                            cycle_text += " -> ";
                        }
                        cycle_text += cycle[i];
                    }

                    g_diagnostics.error(
                        std::format("circular module dependency detected:\n  {}", cycle_text),
                        common::SourceLocation{0, 0, node->path.string()});
                    return false;
                }

                if (dep_state == state.end() || dep_state->second == 0) {
                    if (dfs_validate_module(dep, state, path, path_index)) {
                        return false;
                    }
                }
            }
        }

        path.pop_back();
        path_index.erase(name);
        state[name] = 2;
        return false;
    }

    bool ModuleNamespaceTree::is_valid() {
        if (g_diagnostics.has_errors()) {
            return false;
        }

        std::unordered_map<std::string, int> state;
        std::vector<std::string> path;
        std::unordered_map<std::string, size_t> path_index;

        std::vector<std::string> module_names;
        collect_module_names(modules_, module_names);
        for (const auto& name : module_names) {
            state[name] = 0;
        }

        for (const auto& name : module_names) {
            if (state[name] == 0 && dfs_validate_module(name, state, path, path_index)) {
                return false;
            }
        }

        return !g_diagnostics.has_errors();
    }

    std::optional<fs::path> ModuleNamespaceTree::find_project_root(const fs::path& start) {
        fs::path current = fs::absolute(start);

        while (true) {
            if (fs::exists(current / "xenon.toml")) {
                return current;
            }

            const fs::path parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = parent;
        }

        return std::nullopt;
    }

    std::string ModuleNamespaceTree::read_file_or_throw(const fs::path& path) {
        std::ifstream file(path);
        if (!file) {
            throw CompilerException("Cannot read file (disappeared after resolution?): " + path.string());
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // ============================================================
    // Driver
    // ============================================================

    void Driver::verbosity_print(const std::string& message) {
        if (options_.verbose) {
            std::cout << message << '\n';
        }
    }

    config::CompilerConfig Driver::config_from_toml_map(const toml::TOMLMap& map) {
        config::CompilerConfig cfg;

        if (auto it = map.find("project.name"); it != map.end()) {
            cfg.project_name = it->second;
        }
        if (auto it = map.find("project.version"); it != map.end()) {
            cfg.version = it->second;
        }
        if (auto it = map.find("project.source_files"); it != map.end()) {
            cfg.source_files = parse_string_array(it->second);
        }
        if (auto it = map.find("project.authors"); it != map.end()) {
            cfg.authors = parse_string_array(it->second);
        }
        if (auto it = map.find("build.output"); it != map.end()) {
            cfg.output_dir = it->second;
        }
        if (auto it = map.find("build.target"); it != map.end()) {
            cfg.target_triple = it->second;
        }
        if (auto it = map.find("build.optimisation"); it != map.end()) {
            cfg.opt_level = config::parse_opt_level(it->second);
        }

        return cfg;
    }

    void Driver::load_toml_config() {
        verbosity_print("Loading xenon.toml...");

        const auto root = ModuleNamespaceTree::find_project_root(fs::current_path());
        if (!root) {
            throw CompilerException("No xenon.toml found. Are you inside a Xenon project?");
        }

        const fs::path toml_path = *root / "xenon.toml";
        toml_config_ = toml::parse_toml_file(toml_path);
        if (!toml_config_) {
            throw CompilerException("Failed to parse xenon.toml", common::SourceLocation{0, 0, toml_path.string()});
        }

        const config::CompilerConfig cli = options_;
        config::CompilerConfig merged = config_from_toml_map(*toml_config_);

        merged.command = cli.command;
        merged.dump_tokens = cli.dump_tokens;
        merged.dump_ast = cli.dump_ast;
        merged.no_colour = cli.no_colour;
        merged.verbose = cli.verbose;

        if (!cli.output_name.empty()) {
            merged.output_name = cli.output_name;
        }
        if (cli.output_dir != "build/") {
            merged.output_dir = cli.output_dir;
        }
        if (cli.target_triple != "x86_64-linux") {
            merged.target_triple = cli.target_triple;
        }
        if (cli.opt_level != config::OptimisationLevel::DEBUG) {
            merged.opt_level = cli.opt_level;
        }
        if (cli.warning_level != config::WarningLevel::WARN) {
            merged.warning_level = cli.warning_level;
        }

        merged.project_root = *root;
        g_diagnostics.set_project_root(merged.project_root.string());

        if (merged.source_files.empty()) {
            throw CompilerException("xenon.toml: missing required field 'project.source_files'",
                common::SourceLocation{0, 0, toml_path.string()});
        }
        if (merged.project_name.empty()) {
            throw CompilerException("xenon.toml: missing required field 'project.name'",
                common::SourceLocation{0, 0, toml_path.string()});
        }

        // Resolve and validate each source file path
        for (const auto& rel : merged.source_files) {
            const fs::path abs = merged.project_root / rel;
            if (!fs::exists(abs)) {
                throw CompilerException(std::format("Source file '{}' not found (resolved to '{}')", rel, abs.string()));
            }
            merged.source_paths.push_back(abs);
        }

        options_ = std::move(merged);
        verbosity_print("Sources: " + std::to_string(options_.source_paths.size()) + " file(s)");
    }

    void Driver::resolve_modules() {
        verbosity_print("Resolving modules...");

        module_tree_ = ModuleNamespaceTree{};

        std::vector<Module> modules;
        modules.reserve(options_.source_paths.size());

        for (const auto& source_path : options_.source_paths) {
            const std::string source = ModuleNamespaceTree::read_file_or_throw(source_path);
            const auto tokens = lexer::Lexer::lex(source, source_path.string());
            auto parsed = parser::Parser::parse(tokens, source_path.string());

            if (g_diagnostics.has_errors()) {
                return;
            }

            modules.emplace_back(source_path, std::move(parsed));
        }

        for (auto& module : modules) {
            module_tree_.add_module(std::move(module));
        }

        if (!module_tree_.is_valid()) {
            return;
        }

        verbosity_print(std::format("Loaded {} module(s)", module_tree_.size()));
    }

    void Driver::dump_entry_debug(const std::string& source, const fs::path& path) {
        const std::string path_key = path.string();
        const auto tokens = lexer::Lexer::lex(source, path_key);

        if (options_.dump_tokens) {
            std::cout << "=== Tokens: " << path_key << " ===\n";
            for (const auto& token : tokens) {
                std::cout << token.to_string() << '\n';
            }
            std::cout << '\n';
        }

        if (options_.dump_ast) {
            const ast::ModuleAST ast = parser::Parser::parse(tokens, path_key);
            std::cout << "=== AST: " << path_key << " ===\n";
            std::cout << "module " << ast.module_name << '\n';
            for (const auto& dep : ast.dependencies) {
                std::cout << "  import \"" << dep << "\"\n";
            }
            std::cout << "  declarations: " << ast.root.declarations.size() << '\n';
            std::cout << '\n';
        }
    }

    void Driver::print_build_info() {
        std::cout << "Project: " << options_.project_name
                << "  v" << options_.version << '\n'
                << "  Build: "
                << (options_.opt_level == config::OptimisationLevel::RELEASE ? "release" : "debug")
                << ", target: " << options_.target_triple << "\n\n";
        verbosity_print(std::format("Output directory: {}", options_.output_dir.string()));
    }

    bool Driver::check() {
        load_toml_config();

        if (options_.dump_tokens || options_.dump_ast) {
            const std::string source = ModuleNamespaceTree::read_file_or_throw(options_.source_paths.front());
            dump_entry_debug(source, options_.source_paths.front());
            return !g_diagnostics.has_errors();
        }

        print_build_info();
        resolve_modules();

        if (g_diagnostics.has_errors()) {
            return false;
        }

        if (!semantic::SemanticAnalyser::validate(module_tree_, options_)) {
            return false;
        }

        std::cout << "Check passed — no errors.\n";
        return true;
    }

    bool Driver::build() {
        std::cout << "Building..." << std::endl;
        load_toml_config();

        print_build_info();
        resolve_modules();

        if (g_diagnostics.has_errors()) {
            return false;
        }

        if (!semantic::SemanticAnalyser::validate(module_tree_, options_)) {
            return false;
        }

        verbosity_print("Phase 3: Generating code...");
        verbosity_print("  Code generation OK");

        const std::string output = !options_.output_name.empty()
            ? options_.output_name
            : options_.project_name;
        verbosity_print(std::format("Output: {}", output));

        return true;
    }

    void Driver::run() {
        // Deliberately no try/catch here: driver-level fatal project/setup
        // failures are raised as CompilerException and are handled in main().
        // Per-file lexer/parser issues are reported directly into
        // g_diagnostics and checked after each phase via has_errors().
        if (options_.command == config::Command::BUILD) {
            build();
        } else if (options_.command == config::Command::CHECK) {
            check();
        } else {
            g_diagnostics.error("Invalid command for driver");
        }
    }

    int Driver::init_project() {
        const fs::path current = fs::current_path();
        const fs::path toml_path = current / "xenon.toml";
        const fs::path src_dir = current / "src";
        const fs::path main_path = src_dir / "main.xe";

        if (!fs::exists(src_dir)) {
            fs::create_directory(src_dir);
        }

        if (fs::exists(toml_path)) {
            g_diagnostics.error("xenon.toml already exists in the current directory.");
        } else {
            std::ofstream toml_file(toml_path);
            if (!toml_file) {
                std::cerr << "Error: failed to create xenon.toml\n";
                return 1;
            }
            toml_file << SAMPLE_CONFIG_CONTENT;
        }

        if (fs::exists(main_path)) {
            g_diagnostics.error("src/main.xe already exists.");
        } else {
            std::ofstream main_file(main_path);
            if (!main_file) {
                std::cerr << "Error: failed to create src/main.xe\n";
                return 1;
            }
            main_file << SAMPLE_MAIN_CONTENT;
        }

        return g_diagnostics.exit_gracefully();
    }

} // namespace xenon::driver