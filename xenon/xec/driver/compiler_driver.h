#pragma once

#include "common/dataclasses.h"
#include "cli/cli_parser.h"
#include "fsloader/fsloader.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast_printer.h"
#include "common/diagnostics.h"

#include <memory>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>


namespace xenon {

    using config::CompilerOpts;
    using config::ConfigFile;
    using config::Command;
    
    class CompilerDriver {
    private:
        CompilerOpts& options_;
        std::unique_ptr<FsLoader::LoadedProject> project_;
        std::vector<Module> modules_;
        ConfigFile config_;

        // Private phase methods
        bool load_config();
        bool load_and_order_modules();
        bool validate_modules();
        bool dump_and_check_entry_file();
        void print_build_info();
        void verbosity_print(const std::string& message);

        // Private command implementations
        bool build();
        bool check();

        // Helper for module ordering
        bool process_module(
            const std::string& path,
            std::unordered_map<std::string, Module>& cache,
            std::unordered_set<std::string>& visited,
            std::unordered_set<std::string>& in_stack
        );

    public:
        explicit CompilerDriver(CompilerOpts& opts) : options_(opts) {}

        // Run the requested command (build or check).
        // Internally loads config file, merges with CLI options, and routes to the appropriate command.
        // Returns true on success (no errors emitted), false otherwise.
        bool run();
    };
}