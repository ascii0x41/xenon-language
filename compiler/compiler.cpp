#include "compiler.h"

#include "common/diagnostics.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <fstream>
#include <sstream>

#include <toml++/toml.h>

namespace xenon::compiler {

    namespace {
        std::vector<std::string> parse_string_array(const std::string& text) {
            std::vector<std::string> items;
            std::string trimmed = text;
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
                trimmed.erase(trimmed.begin());
            }
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
                trimmed.pop_back();
            }
            if (trimmed == "[]") {
                return items;
            }
            if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
                trimmed = trimmed.substr(1, trimmed.size() - 2);
            }
            std::stringstream ss(trimmed);
            std::string token;
            while (std::getline(ss, token, ',')) {
                std::string item = token;
                while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
                    item.erase(item.begin());
                }
                while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
                    item.pop_back();
                }
                if (item.size() >= 2 && item.front() == '"' && item.back() == '"') {
                    item = item.substr(1, item.size() - 2);
                }
                if (!item.empty()) {
                    items.push_back(item);
                }
            }
            return items;
        }
    }

    std::vector<std::string> ModuleNamespaceTree::split_name(const std::string& name) {
        std::vector<std::string> parts;
        std::string current;

        for (char ch : name) {
            if (ch == '.') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }

        if (!current.empty()) {
            parts.push_back(current);
        }

        return parts;
    }

    void ModuleNamespaceTree::collect_module_names(
        const std::unordered_map<std::string, Module>& children,
        std::vector<std::string>& out,
        const std::string& prefix) {
        for (const auto& [name, child] : children) {
            const std::string full_name = prefix.empty() ? name : prefix + "." + name;
            out.push_back(full_name);
            collect_module_names(child.children, out, full_name);
        }
    }

    void ModuleNamespaceTree::add_module(Module module) {
        std::vector<std::string> parts = split_name(module.path.stem().string());
        if (parts.empty()) {
            return;
        }

        std::unordered_map<std::string, Module>* current_map = &modules_;
        for (size_t i = 0; i < parts.size(); ++i) {
            const std::string& part = parts[i];
            const bool is_last = i + 1 == parts.size();

            auto entry = current_map->find(part);
            if (entry == current_map->end()) {
                entry = current_map->emplace(part, Module()).first;
            }

            if (is_last) {
                entry->second = std::move(module);
                entry->second.children.clear();
            } else {
                current_map = &entry->second.children;
            }
        }
    }

    std::vector<std::string> ModuleNamespaceTree::all_module_names() const {
        std::vector<std::string> names;
        collect_module_names(modules_, names);
        return names;
    }

    Module* ModuleNamespaceTree::get_module(const std::string& name) {
        std::vector<std::string> parts = split_name(name);
        std::unordered_map<std::string, Module>* current = &modules_;
        for (const auto& part : parts) {
            auto it = current->find(part);
            if (it == current->end()) {
                return nullptr;
            }
            if (part == parts.back()) {
                return &it->second;
            }
            current = &it->second.children;
        }
        return nullptr;
    }

    const Module* ModuleNamespaceTree::get_module(const std::string& name) const {
        std::vector<std::string> parts = split_name(name);
        const std::unordered_map<std::string, Module>* current = &modules_;
        for (const auto& part : parts) {
            auto it = current->find(part);
            if (it == current->end()) {
                return nullptr;
            }
            if (part == parts.back()) {
                return &it->second;
            }
            current = &it->second.children;
        }
        return nullptr;
    }

    void ModuleNamespaceTree::report_unresolved_dependency(
        const std::string& importer_name,
        const std::string& dependency_name,
        const fs::path& importer_path) const {
        g_diagnostics.error(
            std::format("Module '{}' imports unresolved module '{}'", importer_name, dependency_name),
            common::SourceLocation{0, 0, importer_path.string()}
        );
    }

    bool ModuleNamespaceTree::dfs_validate_module(
        const std::string& name,
        std::unordered_map<std::string, int>& state,
        std::vector<std::string>& path,
        std::unordered_map<std::string, size_t>& path_index) {
        auto it = modules_.find(name);
        if (it == modules_.end()) {
            return true;
        }

        if (state[name] == 1) {
            std::string cycle;
            for (size_t i = path_index[name]; i < path.size(); ++i) {
                cycle += path[i];
                if (i + 1 != path.size()) {
                    cycle += " -> ";
                }
            }
            cycle += " -> " + name;
            g_diagnostics.error(std::format("Circular dependency detected: {}", cycle));
            return false;
        }

        if (state[name] == 2) {
            return true;
        }

        state[name] = 1;
        path.push_back(name);
        path_index[name] = path.size() - 1;

        const Module* module = &it->second;
        if (module->ast.has_value()) {
            for (const auto& dep : module->ast->dependencies) {
                const std::string dep_name = dep.to_string();
                if (!dfs_validate_module(dep_name, state, path, path_index)) {
                    return false;
                }
            }
        }

        path.pop_back();
        state[name] = 2;
        return true;
    }

    bool ModuleNamespaceTree::is_valid() {
        std::unordered_map<std::string, int> state;
        std::vector<std::string> path;
        std::unordered_map<std::string, size_t> path_index;

        for (const auto& [name, _] : modules_) {
            if (!dfs_validate_module(name, state, path, path_index)) {
                return false;
            }
        }

        return !g_diagnostics.has_errors();
    }

    std::optional<fs::path> ModuleNamespaceTree::find_project_root(const fs::path& start) {
        fs::path current = start;
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

    void Compiler::verbosity_print(const std::string& message) const {
        if (config_.verbose) {
            std::cout << message << '\n';
        }
    }

    config::TomlSettings Compiler::config_from_toml_map(const toml::TOMLMap& map) {
        config::TomlSettings cfg;

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
            cfg.output_dir = fs::path{it->second};
        }
        if (auto it = map.find("build.target"); it != map.end()) {
            cfg.target_triple = it->second;
        }
        if (auto it = map.find("build.optimisation"); it != map.end()) {
            cfg.optimisation = config::parse_opt_level(it->second);
        }
        if (auto it = map.find("build.warning"); it != map.end()) {
            cfg.warning = config::parse_warning_level(it->second);
        }

        if (cfg.target_triple) {
            const auto parsed = config::parse_target_triple(*cfg.target_triple);
            if (!parsed) {
                throw CompilerException(
                    std::format("Unsupported target triple '{}'. Supported targets: x86_64-linux, x86_64-windows, x86_64-macos", *cfg.target_triple),
                    common::SourceLocation{0, 0, "xenon.toml"}
                );
            }
        }

        return cfg;
    }

    void Compiler::load_toml_config() {
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

        const auto& cli = cli_overrides_;
        config::TomlSettings toml_settings = config_from_toml_map(*toml_config_);
        config::ResolvedConfig merged;

        merged.command = cli.command;
        merged.dump_tokens = cli.dump_tokens.value_or(false);
        merged.dump_ast = cli.dump_ast.value_or(false);
        merged.no_colour = cli.no_colour.value_or(false);
        merged.verbose = cli.verbose.value_or(false);

        if (toml_settings.project_name) {
            merged.project_name = *toml_settings.project_name;
        }
        if (toml_settings.version) {
            merged.version = *toml_settings.version;
        }
        if (toml_settings.authors) {
            merged.authors = *toml_settings.authors;
        }
        if (toml_settings.output_dir) {
            merged.output_dir = *toml_settings.output_dir;
        }
        if (toml_settings.target_triple) {
            merged.target_triple = *toml_settings.target_triple;
        }
        if (toml_settings.optimisation) {
            merged.optimisation = *toml_settings.optimisation;
        }
        if (toml_settings.warning) {
            merged.warning = *toml_settings.warning;
        }

        if (cli.output_name) {
            merged.output_name = *cli.output_name;
        }
        if (cli.output_dir) {
            merged.output_dir = *cli.output_dir;
        }
        if (cli.target_triple) {
            merged.target_triple = *cli.target_triple;
        }
        if (cli.optimisation) {
            merged.optimisation = *cli.optimisation;
        }
        if (cli.warning) {
            merged.warning = *cli.warning;
        }

        if (!merged.target_triple.empty()) {
            const auto parsed = config::parse_target_triple(merged.target_triple);
            if (!parsed) {
                throw CompilerException(
                    std::format("Unsupported target triple '{}'. Supported targets: x86_64-linux, x86_64-windows, x86_64-macos", merged.target_triple),
                    common::SourceLocation{0, 0, toml_path.string()}
                );
            }
            merged.target_info = *parsed;
        }

        if (merged.target_triple.empty()) {
            throw CompilerException("xenon.toml: missing required field 'build.target'", common::SourceLocation{0, 0, toml_path.string()});
        }

        merged.project_root = *root;
        g_diagnostics.set_project_root(merged.project_root.string());

        if (!toml_settings.source_files) {
            throw CompilerException("xenon.toml: missing required field 'project.source_files'",
                common::SourceLocation{0, 0, toml_path.string()});
        }
        if (!toml_settings.project_name) {
            throw CompilerException("xenon.toml: missing required field 'project.name'",
                common::SourceLocation{0, 0, toml_path.string()});
        }

        merged.source_paths.clear();
        merged.source_paths.reserve(toml_settings.source_files->size());
        for (const auto& rel : *toml_settings.source_files) {
            const fs::path abs = merged.project_root / rel;
            if (!fs::exists(abs)) {
                throw CompilerException(std::format("Source file '{}' not found (resolved to '{}')", rel, abs.string()));
            }
            merged.source_paths.push_back(abs);
        }

        if (toml_settings.authors) {
            merged.authors = *toml_settings.authors;
        }

        config_ = std::move(merged);
        verbosity_print("Sources: " + std::to_string(config_.source_paths.size()) + " file(s)");
    }

    void Compiler::resolve_modules() {
        verbosity_print("Resolving modules...");

        module_tree_ = ModuleNamespaceTree{};
        std::vector<Module> modules;
        modules.reserve(config_.source_paths.size());

        for (const auto& source_path : config_.source_paths) {
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
    }

    void Compiler::dump_entry_debug(const std::string& source, const fs::path& path) {
        const std::string path_key = path.string();
        const auto tokens = lexer::Lexer::lex(source, path_key);

        if (config_.dump_tokens) {
            std::cout << "=== Tokens: " << path_key << " ===\n";
            for (const auto& token : tokens) {
                std::cout << token.to_string() << '\n';
            }
            std::cout << '\n';
        }

        if (config_.dump_ast) {
            const ast::ModuleAST ast = parser::Parser::parse(tokens, path_key);
            std::cout << "=== AST: " << path_key << " ===\n";
            std::cout << "module " << ast.module_name.to_string() << '\n';
            for (const auto& dep : ast.dependencies) {
                std::cout << "  import \"" << dep.to_string() << "\"\n";
            }
            std::cout << "  declarations: " << ast.root.declarations.size() << '\n';
            std::cout << '\n';
        }
    }

    void Compiler::print_build_info() const {
        std::cout << "Project: " << config_.project_name
                << "  v" << config_.version << '\n'
                << "  Build: "
                << (config_.optimisation == config::OptimisationLevel::RELEASE ? "release" : "debug")
                << ", target: " << config_.target_triple << "\n\n";
        verbosity_print(std::format("Output directory: {}", config_.output_dir.string()));
    }

    void Compiler::_print_build_info() const {
        if (!config_.verbose) {
            return;
        }
        std::cout << "Project: " << config_.project_name << '\n';
        std::cout << "Target:  " << config_.target_triple << '\n';
        std::cout << "Profile: " << (config_.optimisation == config::OptimisationLevel::RELEASE ? "release" : "debug") << '\n';
        std::cout << "Output:  " << (config_.output_name.empty() ? config_.project_name : config_.output_name) << '\n';
    }

    bool Compiler::check() {
        return _compile(false);
    }

    bool Compiler::build() {
        return _compile(true);
    }

    bool Compiler::_compile(bool generate_code) {
        _print_build_info();

        if (!_load_modules()) {
            return false;
        }

        if (!module_tree_.is_valid()) {
            return false;
        }

        const auto typed_ast = _analyse();
        if (!typed_ast.has_value() || g_diagnostics.has_errors()) {
            return false;
        }

        if (!generate_code) {
            std::cout << "Check passed — no errors.\n";
            return true;
        }

        if (!_generate_code(*typed_ast)) {
            return false;
        }

        std::cout << "Build succeeded.\n";
        return true;
    }

    bool Compiler::_load_modules() {
        for (const auto& source_path : config_.source_paths) {
            try {
                const std::string source = ModuleNamespaceTree::read_file_or_throw(source_path);
                const auto tokens = lexer::Lexer::lex(source, source_path.string());
                auto parsed = parser::Parser::parse(tokens, source_path.string());

                if (g_diagnostics.has_errors()) {
                    continue;
                }

                module_tree_.add_module(Module(source_path, std::move(parsed)));
            } catch (const std::exception& error) {
                g_diagnostics.error(std::format("Cannot read source file '{}': {}", source_path.string(), error.what()));
            }
        }

        return !g_diagnostics.has_errors();
    }

    std::optional<ast::ModuleAST> Compiler::_analyse() {
        (void)module_tree_;
        return ast::ModuleAST{};
    }

    bool Compiler::_generate_code(const ast::ModuleAST& typed_ast) {
        (void)typed_ast;
        return true;
    }

    void Compiler::run() {
        if (config_.command == config::Command::BUILD) {
            build();
        } else if (config_.command == config::Command::CHECK) {
            check();
        } else {
            g_diagnostics.error("Invalid command for compiler");
        }
    }

    int Compiler::init_project() {
        const fs::path current = fs::current_path();
        const fs::path toml_path = current / "xenon.toml";
        const fs::path src_dir = current / "src";
        const fs::path main_path = src_dir / "main.xe";

        if (!fs::exists(src_dir)) {
            fs::create_directory(src_dir);
        }

        if (!fs::exists(toml_path)) {
            std::ofstream file(toml_path);
            if (!file) {
                throw CompilerException("Failed to create xenon.toml");
            }
            file << "[project]\nname = \"my_project\"\nversion = \"0.1.0\"\nsource_files = [\"src/main.xe\"]\nauthors = [\"Your Name\"]\n\n[build]\ntarget = \"x86_64-linux\"\noutput = \"build/\"\noptimisation = \"debug\"\n";
        }

        if (!fs::exists(main_path)) {
            std::ofstream file(main_path);
            if (!file) {
                throw CompilerException("Failed to create src/main.xe");
            }
            file << "fn main() -> i32 {\n    return 0;\n}\n";
        }

        return 0;
    }

} // namespace xenon::compiler
