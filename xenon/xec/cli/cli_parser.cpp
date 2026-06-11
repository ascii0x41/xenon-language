#include "cli_parser.h"

#include <iostream>
#include <cstdlib>
#include <string>

namespace xenon::cli {

using config::CompilerOpts;
using config::ConfigFile;
using config::Command;
using config::OptimisationLevel;
using config::WarningLevel;

void print_usage() {
    std::cout << R"(Usage: xec [command] [options]

Xenon Compiler - build and manage Xenon projects.

Commands:
  build              Build the project (default)
  check              Check the project without building
  init               Initialize a new project

Options:
  -h, --help             Show this help message
  --version              Show version information
  -o, --output <name>    Set output binary name
  --target <triple>      Set compilation target triple
  --debug                Debug build (no optimizations)
  --release              Release build (full optimizations)
  -Werror                Treat all warnings as errors
  -Wno-<warning>         Ignore a specific warning
  -Werror=<warning>      Treat a specific warning as error
  --no-colour            Disable coloured output
  -v, --verbose          Enable verbose output

Debug Options:
  --dump-tokens          Dump lexer tokens
  --dump-ast             Dump AST

Examples:
  xec                              Build the project (default)
  xec build --release              Build with optimizations
  xec check                        Check the entire project
  xec init                         Create a new project
  xec build --release -o myapp     Custom output name
  xec build -Wno-unused-var        Ignore unused variable warnings
)";
}

CompilerOpts parse_cli(int argc, char* argv[]) {
    CompilerOpts opts;

    // No arguments = help
    if (argc < 2) {
        opts.command = Command::HELP;
        return opts;
    }

    std::string cmd = argv[1];

    // Help and version exit immediately
    if (cmd == "-h" || cmd == "--help") {
        opts.command = Command::HELP;
        return opts;
    }
    if (cmd == "--version") {
        opts.command = Command::VERSION;
        return opts;
    }

    // Parse command
    if (cmd == "build") {
        opts.command = Command::BUILD;
    } else if (cmd == "check") {
        opts.command = Command::CHECK;
    } else if (cmd == "init") {
        opts.command = Command::INIT;
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        print_usage();
        opts.command = Command::HELP;
        return opts;
    }

    // Parse remaining flags
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--no-colour") {
            opts.no_colour = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--dump-tokens") {
            opts.dump_tokens = true;
        } else if (arg == "--dump-ast") {
            opts.dump_ast = true;
        } else if (arg == "-Werror") {
            opts.warnings.warnings_as_errors = true;
        } else if (arg == "--debug") {
            opts.build.opt_level = OptimisationLevel::DEBUG;
        } else if (arg == "--release") {
            opts.build.opt_level = OptimisationLevel::RELEASE;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.build.output_name = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a value\n";
                std::exit(1);
            }
        } else if (arg == "--target") {
            if (i + 1 < argc) {
                opts.build.target_triple = argv[++i];
            } else {
                std::cerr << "Error: --target requires a value\n";
                std::exit(1);
            }
        } else if (arg.starts_with("-Wno-") && arg.size() > 5) {
            std::string warning = arg.substr(5);
            opts.warnings.warnings[warning] = WarningLevel::IGNORE;
        } else if (arg.starts_with("-Werror=") && arg.size() > 8) {
            std::string warning = arg.substr(8);
            opts.warnings.warnings[warning] = WarningLevel::ERROR;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            opts.command = Command::HELP;
            return opts;
        }
    }

    return opts;
}

void reconcile_config_file_with_cli_options(const CompilerOpts& opts, ConfigFile& config) {
    // Merge CLI options into config, with CLI taking precedence
    
    // Build settings: CLI overrides config for explicitly-set values
    if (!opts.build.output_name.empty()) {
        config.build.output_name = opts.build.output_name;
    }
    
    if (opts.build.target_triple != "x86_64-linux") {  // CLI value differs from default
        config.build.target_triple = opts.build.target_triple;
    }
    
    if (opts.build.opt_level != OptimisationLevel::DEBUG) {  // CLI value differs from default
        config.build.opt_level = opts.build.opt_level;
    }
    
    // Warning settings: merge CLI warnings into config (CLI takes precedence)
    for (const auto& [warning_name, warning_level] : opts.warnings.warnings) {
        config.warnings.warnings[warning_name] = warning_level;
    }
    
    // If CLI set warnings_as_errors, propagate to config
    if (opts.warnings.warnings_as_errors) {
        config.warnings.warnings_as_errors = true;
    }
}

} // namespace xenon::cli