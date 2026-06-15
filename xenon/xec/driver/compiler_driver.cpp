#include "compiler_driver.h"

#include <iostream>
#include <format>

namespace xenon {

    using config::Command;
    using config::OptimisationLevel;
    using config::ConfigFile;

    void CompilerDriver::verbosity_print(const std::string& message) {
        if (options_.verbose) {
            std::cout << message << "\n";
        }
    }

    bool CompilerDriver::run() {
        // Route to the appropriate command handler
        if (options_.command == config::Command::BUILD) {
            return build();
        } else if (options_.command == config::Command::CHECK) {
            return check();
        } else {
            // This should not happen if main() properly filters HELP/VERSION/INIT
            g_diagnostics.error("Invalid command for driver");
            return false;
        }
    }

    bool CompilerDriver::load_config() {
        // Load xenon.toml and apply CLI overrides
        verbosity_print("Loading xenon.toml...");
        
        // Create a temporary ConfigFile with CLI defaults to pass to FsLoader
        ConfigFile cli_defaults;
        project_ = FsLoader::load_project(cli_defaults);
        if (!project_) return false;

        // Get the loaded config and merge CLI options into it
        config_ = project_->config;
        cli::reconcile_config_file_with_cli_options(options_, config_);
        
        verbosity_print("Entry: " + config_.entry_path.string());
        return true;
    }

    bool CompilerDriver::dump_and_check_entry_file() {
        // Load entry file for debug dumps if requested
        if (!project_) {
            return false;
        }

        const SourceFile& entry_sf = project_->entry;

        if (!options_.dump_tokens && !options_.dump_ast) {
            return true;  // Nothing to dump
        }

        // Lex
        auto tokens = Lexer::lex(entry_sf.source, entry_sf.rel_path);
        if (options_.dump_tokens) {
            std::cout << "=== Tokens: " << entry_sf.rel_path << " ===\n";
            for (const auto& t : tokens) {
                std::cout << t.to_string() << "\n";
            }
            std::cout << "\n";
        }

        // Parse
        ParserResult result = Parser::parse(tokens, entry_sf.rel_path);
        if (options_.dump_ast) {
            std::cout << "=== AST: " << entry_sf.rel_path << " ===\n";
            ASTPrinter::print_ast(result);
            std::cout << "\n";
        }

        return !g_diagnostics.has_errors();
    }

    bool CompilerDriver::process_module(
        const std::string& path,
        std::unordered_map<std::string, Module>& cache,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& in_stack)
    {
        if (in_stack.count(path)) {
            g_diagnostics.error("Circular import detected: " + path);
            return true;  // Error
        }

        if (visited.count(path)) {
            return false;  // Already processed
        }

        in_stack.insert(path);

        // Load and parse if not cached
        if (!cache.count(path)) {
            auto sf = FsLoader::load_file(path);
            if (!sf) {
                g_diagnostics.error("Failed to load file: " + path);
                return true;  // Error
            }

            auto tokens = Lexer::lex(sf->source, sf->rel_path);
            auto result = Parser::parse(tokens, sf->rel_path);

            Module mod{std::move(*sf), std::move(result)};
            cache[path] = std::move(mod);
        }

        Module& mod = cache[path];

        // Process dependencies recursively
        for (const auto& imp : mod.parsed.imports) {
            // Skip stdlib imports (starting with "std/")
            if (imp.module_path.rfind("std/", 0) == 0) {
                continue;
            }

            std::string resolved = FsLoader::resolve_import_path(
                imp.module_path, path, config_.project_root
            );
            
            if (resolved.empty()) {
                g_diagnostics.error(std::format("Cannot resolve import: {}", imp.module_path));
                return true;  // Error
            }

            if (process_module(resolved, cache, visited, in_stack)) {
                return true;  // Error in dependency
            }
        }

        in_stack.erase(path);
        visited.insert(path);
        modules_.push_back(std::move(cache[path]));

        return false;  // Success
    }

    bool CompilerDriver::load_and_order_modules() {
        verbosity_print("Phase 1: Loading and ordering modules...");

        if (!project_) {
            return false;
        }

        std::string entry_path = config_.entry_path.string();

        fs::path abs_entry = fs::absolute(entry_path);
        std::unordered_map<std::string, Module> cache;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> in_stack;

        if (process_module(abs_entry.string(), cache, visited, in_stack)) {
            return false;  // Error
        }

        std::cout << "  Loaded " << modules_.size() << " module(s)\n";
        for (const auto& mod : modules_) {
            std::cout << "    - " << mod.source_file.rel_path << "\n";
        }
        std::cout << "\n";

        return true;
    }

    bool CompilerDriver::validate_modules() {
        verbosity_print("Phase 2: Validating modules...");
        
        // TODO: Implement full validation pipeline
        // For now, just check that parsing didn't produce errors
        if (g_diagnostics.has_errors()) {
            return false;
        }

        verbosity_print("  Validation OK");
        return true;
    }

    void CompilerDriver::print_build_info() {
        if (project_) {
            // Project mode: full build info
            std::cout << "Project: " << config_.project.name
                      << "  v" << config_.project.version << "\n"
                      << "  Build: " << (config_.build.opt_level == OptimisationLevel::RELEASE ? "release" : "debug")
                      << ", target: " << config_.build.target_triple << "\n\n";
            verbosity_print(std::format("Output directory: {}", config_.build.output_dir.string()));
        }

        if (config_.pedantic) {
            verbosity_print("Pedantic mode: all warnings as errors");
        }
    }

    bool CompilerDriver::check() {
        // Load configuration
        if (!load_config()) return false;

        // Handle debug dumps (early exit after dumping)
        if (options_.dump_tokens || options_.dump_ast) {
            return dump_and_check_entry_file();
        }

        // Print build info
        print_build_info();

        // Load and order modules
        if (!load_and_order_modules()) return false;

        // Validate modules
        if (!validate_modules()) return false;

        std::cout << "Check passed — no errors.\n";
        return true;
    }

    bool CompilerDriver::build() {
        // Load configuration
        if (!load_config()) return false;

        // Handle debug dumps (early exit after dumping)
        if (options_.dump_tokens || options_.dump_ast) {
            return dump_and_check_entry_file();
        }

        // Print build info
        print_build_info();

        // Load and order modules
        if (!load_and_order_modules()) return false;

        // Validate modules
        if (!validate_modules()) return false;

        // Phase 3: Code generation
        verbosity_print("Phase 3: Generating code...");
        // TODO: Implement code generation (lower AST to IR, optimize, generate LLVM/C)
        verbosity_print("  Code generation OK");

        std::string output = !config_.build.output_name.empty()
            ? config_.build.output_name
            : config_.project.name;
        verbosity_print(std::format("Output: {}", output));

        return true;
    }
}
