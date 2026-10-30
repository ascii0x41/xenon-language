#pragma once

#include "common/dataclasses.h"
#include <optional>
#include <string>
#include <vector>
#include <iostream>

namespace xenon::cli {
    
    using config::CLIOverrides;
    using config::Command;
    using config::OptimisationLevel;
    using config::WarningLevel;

    void print_usage();

    CLIOverrides parse_cli(int argc, char* argv[]);
}