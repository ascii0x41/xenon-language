#include "common/cli/cli_parser.h"
#include "common/config/config_resolver.h"
#include "common/config/config_error.h"
#include "common/diagnostics/diagnostic_formatter.h"
#include "common/installation/installation_paths.h"
#include "common/source/source_file_manager.h"
#include "compiler/compiler.h"

#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ostream>


int main(int argc, char* argv[]) {
    try {

        if (argc == 1) {
            std::cout << XEC_VERSION << "\n";
            xenon::cli::CLIParser::print_usage();

            return 0;
        }
        const xenon::config::CLIArgs args = xenon::cli::CLIParser::parse_cli(argc, argv);

        auto diagnostic_formatter = xenon::diagnostics::DiagnosticFormatter(
            xenon::diagnostics::FormatOptions{
                .color = args.show_color
            }
        );

        auto diagnostic_collector = xenon::diagnostics::DiagnosticCollector();

        if (args.command == "help") {
            xenon::cli::CLIParser::print_usage(args.help_command);
            return 0;
        }
        if (args.command == "version") {
            std::cout << XEC_VERSION << "\n";
            return 0;
        }


        if (args.command == "init") {
            const std::filesystem::path project = args.project_dir;
            std::filesystem::create_directories(project / "src");

            const std::filesystem::path toml = project / "xenon.toml";
            if (!std::filesystem::exists(toml)) {
                std::ofstream out(toml);
                out << "[project]\nname = \"my_project\"\nsource_files = [\"src/main.xe\"]\n\n"
                       "[build]\ntarget = \"x86_64-linux\"\noptimisation = \"debug\"\n";
            }

            const std::filesystem::path main = project / "src/main.xe";
            if (!std::filesystem::exists(main)) {
                std::ofstream out(main);
                out << "module main;\n\nfunc main() -> i32 {\n    return 0;\n}\n";
            }

            std::cout << "Initialized Xenon project at " << project << '\n';
            return 0;
        }

        const auto& installation = xenon::installation::g_installation_paths();
        xenon::config::ResolvedConfigs resolved;
        try {
            resolved = xenon::config::load_and_resolve(args, installation.root);
        } catch (const xenon::config::ConfigError& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }



        xenon::CompilationResult result = xenon::compiler::Compiler::compile(
            resolved, args.command == "build", args.show_ast);

        diagnostic_collector.merge(result.diagnostics());

        auto output = diagnostic_formatter.format_all(diagnostic_collector, xenon::source::g_source_file_manager);
        auto success = result.success() ? 0 : 1;

        std::cout << output << "\n\n" << (
            diagnostic_collector.has_fatal_errors() ? "Task failed with fatal errors"
            : diagnostic_collector.has_errors() ? "Task failed with errors"
            : diagnostic_collector.warning_count() > 0 ? std::format("Task succeeded with {} warning(s)", diagnostic_collector.warning_count())
            : "Task suceeded"
        ) << std::endl;

        return success;

    } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
