#pragma once

#include "common/dataclasses.h"
#include "ast/astlib.h"
#include "toml/toml_parser.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xenon {

namespace compiler {

    struct Module {
        fs::path path;
        std::optional<ast::ModuleAST> ast;
        std::unordered_map<std::string, Module> children;

        Module() = default;
        explicit Module(fs::path module_path, std::optional<ast::ModuleAST> module_ast = std::nullopt)
            : path(std::move(module_path)), ast(std::move(module_ast)) {}
    };

    class ModuleNamespaceTree {
    public:
        void add_module(Module module);

        Module* get_module(const std::string& name);
        const Module* get_module(const std::string& name) const;

        bool is_valid();
        size_t size() const { return modules_.size(); }
        std::vector<std::string> all_module_names() const;

        const std::unordered_map<std::string, Module>& modules() const { return modules_; }
        std::unordered_map<std::string, Module>& modules() { return modules_; }

        static std::optional<fs::path> find_project_root(const fs::path& start);
        static std::string read_file_or_throw(const fs::path& path);

    private:
        static std::vector<std::string> split_name(const std::string& name);
        static void collect_module_names(
            const std::unordered_map<std::string, Module>& children,
            std::vector<std::string>& out,
            const std::string& prefix = {});

        bool dfs_validate_module(
            const std::string& name,
            std::unordered_map<std::string, int>& state,
            std::vector<std::string>& path,
            std::unordered_map<std::string, size_t>& path_index);

        void report_unresolved_dependency(
            const std::string& importer_name,
            const std::string& dependency_name,
            const fs::path& importer_path) const;

        std::unordered_map<std::string, Module> modules_;
    };

    class Compiler {
    public:
        explicit Compiler(const config::ResolvedConfig& cfg)
            : config_(cfg), module_tree_() {}

        explicit Compiler(const config::CLIOverrides& cli_overrides)
            : cli_overrides_(cli_overrides), config_(), module_tree_() {
            config_.command = cli_overrides.command;
            config_.dump_tokens = cli_overrides.dump_tokens.value_or(false);
            config_.dump_ast = cli_overrides.dump_ast.value_or(false);
            config_.no_colour = cli_overrides.no_colour.value_or(false);
            config_.verbose = cli_overrides.verbose.value_or(false);
            if (cli_overrides.output_name) {
                config_.output_name = *cli_overrides.output_name;
            }
            if (cli_overrides.output_dir) {
                config_.output_dir = *cli_overrides.output_dir;
            }
            if (cli_overrides.target_triple) {
                config_.target_triple = *cli_overrides.target_triple;
            }
            if (cli_overrides.optimisation) {
                config_.optimisation = *cli_overrides.optimisation;
            }
            if (cli_overrides.warning) {
                config_.warning = *cli_overrides.warning;
            }
        }

        static void run_compiler(const config::CLIOverrides& cli_overrides) {
            Compiler(cli_overrides).run();
        }

        static int init_project();

        bool check();
        bool build();
        bool _compile(bool generate_code);
        bool _load_modules();
        std::optional<ast::ModuleAST> _analyse();
        bool _generate_code(const ast::ModuleAST& typed_ast);
        void _print_build_info() const;

    private:
        void run();
        void load_toml_config();
        void resolve_modules();
        void verbosity_print(const std::string& message) const;
        static config::TomlSettings config_from_toml_map(const toml::TOMLMap& map);
        void dump_entry_debug(const std::string& source, const fs::path& path);
        void print_build_info() const;

        config::CLIOverrides cli_overrides_;
        config::ResolvedConfig config_;
        ModuleNamespaceTree module_tree_;
        std::optional<toml::TOMLMap> toml_config_;
    };

} // namespace compiler

namespace driver {
    using compiler::Compiler;
    using compiler::Module;
    using compiler::ModuleNamespaceTree;
}

} // namespace xenon
