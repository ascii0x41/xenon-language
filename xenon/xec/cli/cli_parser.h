#pragma once

#include "common/dataclasses.h"
#include <optional>
#include <string>
#include <vector>
#include <iostream>
#include <argparse/argparse.hpp> // Uses argparse for argument parsing

namespace xenon::cli {
    
    using config::CompilerOpts;
    using config::ConfigFile;
    
    void print_usage();

    // CLI options take precedence over config file values. This function merges them together,
    // applying CLI overrides where specified, and falling back to config values otherwise.
    void reconcile_config_file_with_cli_options(const CompilerOpts& opts, ConfigFile& config);

    CompilerOpts parse_cli(int argc, char* argv[]);
}