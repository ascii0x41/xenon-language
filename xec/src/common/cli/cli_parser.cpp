#include "common/cli/cli_parser.h"

#include <iostream>
#include <filesystem>
#include <optional>
#include <string>

namespace xenon::cli {

void CLIParser::print_usage(std::string_view command) {
        if (command == "build") {
                std::cout <<
R"(Usage: xec build [project] [options]

Build a Xenon project (default: .).

Options:
    -h, --help            Show help for build
    --version             Show compiler version
    --target <name>       Select an environment target
    --environment <name>  Select an environment
    --optimisation <mode> debug or release
    -o, --output <path>   Override the build output path
    --show-ast            Print parsed source ASTs as YAML
)";
                return;
        }
        if (command == "check") {
                std::cout <<
R"(Usage: xec check [project] [options]

Parse and resolve a Xenon project without linking (default: .).

Options:
    -h, --help            Show help for check
    --version             Show compiler version
    --target <name>       Select an environment target
    --environment <name>  Select an environment
    --optimisation <mode> debug or release
    -o, --output <path>   Override the build output path
    --show-ast            Print parsed source ASTs as YAML
)";
                return;
        }
        if (command == "init") {
                std::cout <<
R"(Usage: xec init [project]

Create a new Xenon project in the specified directory (default: .).

Options:
    -h, --help            Show help for init
    --version             Show compiler version
)";
                return;
        }

    std::cout <<
R"(Usage: xec <build|check|init> [project] [options]
             xec -h [command]

Commands:
  build [project]       Build a Xenon project (default: .)
  check [project]       Parse and resolve a project without linking
  init [project]        Create a new Xenon project

Options:
  -h, --help            Show this help message
  --version             Show compiler version
  --target <name>       Select an environment target
  --environment <name>  Select an environment
  --optimisation <mode> debug or release
  -o, --output <path>   Override the build output path
)";
    std::cout << "  --show-ast            Print parsed source ASTs as YAML\n";
}

config::CLIArgs CLIParser::parse_cli(int argc, char* argv[]) {
    config::CLIArgs args;
    bool project_dir_set = false;

    if (argc < 2) {
        args.command = "build";
        args.project_dir = std::filesystem::current_path();
        return args;
    }

    std::string first = argv[1];
    if (first == "-h" || first == "--help") {
        args.command = "help";
        if (argc >= 3) {
            args.help_command = argv[2];
        }
        return args;
    }
    if (first == "--version") {
        args.command = "version";
        return args;
    }

    if (first == "build" || first == "check" || first == "init") {
        args.command = first;
    } else if (first[0] == '-') {
        args.command = "build";
    } else {
        args.command = "build";
        args.project_dir = std::filesystem::absolute(first).lexically_normal();
        project_dir_set = true;
    }

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            args.help_command = args.command;
            args.command = "help";
        } else if (arg == "--version") {
            args.command = "version";
        } else if (arg == "--target") {
            if (++i >= argc) {
                throw std::invalid_argument("--target requires a value");
            }
            args.target = argv[i];
        } else if (arg == "--optimisation") {
            if (++i >= argc) {
                throw std::invalid_argument("--optimisation requires a value");
            }
            args.optimisation = argv[i];
        } else if (arg == "-o" || arg == "--output") {
            if (++i >= argc) {
                throw std::invalid_argument(std::string(arg) + " requires a value");
            }
            args.output = argv[i];
        } else if (arg[0] == '-') {
            throw std::invalid_argument("unknown option: " + arg);
        } else if (!project_dir_set) {
            args.project_dir = std::filesystem::absolute(arg).lexically_normal();
            project_dir_set = true;
        }
    }

    return args;
}

} // namespace xenon::cli
