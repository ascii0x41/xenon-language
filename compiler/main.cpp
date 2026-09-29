#include "cli/cli_parser.h"
#include "driver/compiler_driver.h"
#include "common/diagnostics.h"

#define XENON_COMPILER_VERSION \
"xec (Xenon Compiler) version 0.1.0 DEVELOPMENT BUILD\n" \
"Copyright (c) 2026 Gabriel Aryee\n"  \
"This is a development build. Expect bugs and incomplete features.\n" \
"Things may break. That's part of the process.\n" \
"I mean, it's LITERALLY built by a 15 year old. What did you expect?\n" \
"I'm surrounded by a bunch of coding larpers who can't even change their GRUB config!\n" \
"IT IS NOT THAT HARD TO CHANGE YOUR BOOT ORDER. AT THAT POINT JUST GO BACK TO MICROSLOP...\n" \
"Or use... Ubuntu... But I digress. Anyway, this is a development build. Expect bugs and incomplete features.\n" \
"NO WARRANTY, express or implied. Use at your own risk :)"

using namespace xenon;

int main(int argc, char** argv) {
    // Declared outside the try block (rather than defaulted in-place)
    // so the catch block below can still read opts.no_colour even if
    // the exception is thrown after CLI parsing succeeded. If
    // cli::parse_cli itself throws, no_colour just stays at its
    // CompilerConfig default.
    config::CompilerConfig opts;

    try {
        // 1. Parse CLI
        opts = cli::parse_cli(argc, argv);

        // 2. Handle HELP/VERSION/INIT (early exit)
        if (opts.command == config::Command::HELP) {
            cli::print_usage();
            return 0;
        }
        if (opts.command == config::Command::VERSION) {
            std::cout << XENON_COMPILER_VERSION << std::endl;
            return 0;
        }
        if (opts.command == config::Command::INIT) {
            return driver::Driver::init_project();
        }

        // 3. Run (internally loads config and routes to build/check)
        driver::Driver::run_compiler(opts);

        // 4. Exit
        return g_diagnostics.exit_gracefully(!opts.no_colour);

    } catch (const CompilerException& e) {
        // Fatal/unrecoverable driver errors (missing xenon.toml, missing
        // entry, unresolved import, circular dependency) default to
        // Severity::FATAL. Parser/lexer errors tag themselves as
        // Severity::ERROR — routine syntax mistakes, not a sign the
        // compiler itself is broken — so they're reported the same way
        // any other error would be, not badged "fatal error:".
        if (e.severity == Severity::FATAL) {
            g_diagnostics.fatal(e.message, e.location);
        } else {
            g_diagnostics.error(e.message, e.location);
        }
        return g_diagnostics.exit_gracefully(!opts.no_colour);
 


    } catch (const std::exception& e) {
        std::cerr << "internal compiler error: " << e.what() << "\n";
        return 1;
    }
}