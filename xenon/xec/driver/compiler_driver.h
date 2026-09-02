#pragma once

#include "common/dataclasses.h"
#include "ast/astlib.h"
#include "toml/toml_parser.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xenon::driver {

    struct Module {
        fs::path path;
        std::optional<ast::ModuleAST> ast;
        std::unordered_map<std::string, Module> children;

        Module() = default;
        explicit Module(fs::path module_path, std::optional<ast::ModuleAST> module_ast = std::nullopt)
            : path(std::move(module_path)), ast(std::move(module_ast)) {}
    };

    // A namespace tree for module names. This is intentionally separate from
    // dependency resolution: it answers where a module lives in the namespace,
    // not which modules depend on which other modules.
    class ModuleNamespaceTree {
    public:
        void add_module(Module module);

        Module* get_module(const std::string& name);
        const Module* get_module(const std::string& name) const;

        bool is_valid();
        size_t size() const { return modules_.size(); }

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

    class Driver {
    public:
        static void run_compiler(const config::CompilerConfig& opts) {
            Driver(opts).run();
        }

        static int init_project();
    private:
        explicit Driver(const config::CompilerConfig& opts) : options_(opts) {}
        void run();

        // -- Compilation state ------------------------------------------------------

        config::CompilerConfig options_;
        ModuleNamespaceTree module_tree_;
        std::optional<toml::TOMLMap> toml_config_;

        // Private phase methods. Fatal/unrecoverable conditions (missing
        // xenon.toml, missing required fields, missing entry file) throw
        // CompilerException — there's a single catch point in main()
        // that converts it to a diagnostic and exits. Per-file lex/parse
        // errors are left in g_diagnostics for check()/build() to test
        // via g_diagnostics.has_errors() after these return.
        void load_toml_config();
        void resolve_modules();
        static config::CompilerConfig config_from_toml_map(const toml::TOMLMap& map);

        void dump_entry_debug(const std::string& source, const fs::path& path);
        void print_build_info();
        void verbosity_print(const std::string& message);

        // Private command implementations
        bool build();
        bool check();
    };

} // namespace xenon::driver