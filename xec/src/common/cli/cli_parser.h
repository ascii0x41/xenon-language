#pragma once

#include "common/config/cli_args.h"

#include <string_view>

namespace xenon::cli {

class CLIParser {
public:
    static void print_usage(std::string_view command = {});
    static config::CLIArgs parse_cli(int argc, char* argv[]);
};

} // namespace xenon::cli
