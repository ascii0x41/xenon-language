#include "cli_parser.h"

#include <iostream>
#include <cstdlib>
#include <string>

namespace xenon::cli {

using config::CompilerConfig;
using config::Command;
using config::OptimisationLevel;
using config::WarningLevel;

void print_usage() {
    std::cout <<

R"(Usage: xec [command] [options]
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
  --O0                   No optimizations 
  --O1, --debug          Debug build (default)
  --O2, --release        Release build with optimizations
  --W0                   Ignore warnings
  --W1                   Show warnings (default)
  --W2                   Treat warnings as errors
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

    CompilerConfig parse_cli(int argc, char* argv[]) {
        CompilerConfig config;

        // No arguments = help
        if (argc < 2) {
            config.command = Command::HELP;
            return config;
        }

        std::string cmd = argv[1];

        // Help and version exit immediately
        if (cmd == "-h" || cmd == "--help") {
            config.command = Command::HELP;
            return config;
        }
        if (cmd == "--version") {
            config.command = Command::VERSION;
            return config;
        }

        // Parse command
        if (cmd == "build") {
            config.command = Command::BUILD;
        } else if (cmd == "check") {
            config.command = Command::CHECK;
        } else if (cmd == "init") {
            config.command = Command::INIT;
        } else {
            std::cerr << "Unknown command: " << cmd << "\n";
            print_usage();
            config.command = Command::HELP;
            return config;
        }

        // Parse remaining flags
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];

            if (arg == "--no-colour") {
                config.no_colour = true;
            } else if (arg == "-v" || arg == "--verbose") {
                config.verbose = true;
            } else if (arg == "--dump-tokens") {
                config.dump_tokens = true;
            } else if (arg == "--dump-ast") {
                config.dump_ast = true;
            } else if (arg == "-W0") {
                config.warning_level = WarningLevel::IGNORE;
            } else if (arg == "-W1") {
                config.warning_level = WarningLevel::WARN;
            } else if (arg == "-W2") {
                config.warning_level = WarningLevel::ERROR;
            } else if (arg == "-O0") {
                config.opt_level = OptimisationLevel::NONE;
            } else if (arg == "--debug" || arg == "-O1") {
                config.opt_level = OptimisationLevel::DEBUG;
            } else if (arg == "--release" || arg == "-O2") {
                config.opt_level = OptimisationLevel::RELEASE;
            } else if (arg == "-o" || arg == "--output") {
                if (i + 1 < argc) {
                    config.output_name = argv[++i];
                } else {
                    std::cerr << "Error: " << arg << " requires a value\n";
                    std::exit(1);
                }
            } else if (arg == "--target") {
                if (i + 1 < argc) {
                    config.target_triple = argv[++i];
                } else {
                    std::cerr << "Error: --target requires a value\n";
                    std::exit(1);
                }
            } else {
                std::cerr << "Unknown option: " << arg << "\n";
                config.command = Command::HELP;
                return config;
            }
        }

        return config;
    }
} // namespace xenon::cli