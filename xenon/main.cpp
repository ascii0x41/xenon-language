#include "cli/cli_parser.h"
#include "driver/compiler_driver.h"
#include "common/diagnostics.h"

#define XENON_COMPILER_VERSION \
"xec (Xenon Compiler) version 0.1.0 DEVELOPMENT BUILD\n" \
"Copyright (c) 2026 Gabriel Aryee\n"  \
"This is a development build. Expect bugs and incomplete features.\n" \
"Things may break. That's part of the process.\n" \
"NO WARRANTY, express or implied. Use at your own risk."

using namespace xenon;

int main(int argc, char** argv) {
    try {
        // 1. Parse CLI
        auto opts = cli::parse_cli(argc, argv);
        
        // 2. Handle HELP/VERSION/INIT (early exit)
        if (opts.command == Command::HELP) {
            cli::print_usage();
            return 0;
        }
        if (opts.command == Command::VERSION) {
            std::cout << XENON_COMPILER_VERSION << "\n";
            return 0;
        }
        if (opts.command == Command::INIT) {
            return FsLoader::init_project();
        }

        // 3. Initialize driver
        CompilerDriver driver(opts);

        // 4. Run (internally loads config and routes to build/check)
        driver.run();

        // 5. Exit
        return g_diagnostics.exit_gracefully(!opts.no_colour);

    } catch (const CompilerException& e) {
        return g_diagnostics.exit_gracefully();
    } catch (const std::exception& e) {
        std::cerr << "internal compiler error: " << e.what() << "\n";
        return 1;
    }
}
