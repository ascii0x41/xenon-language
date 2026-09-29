#pragma once

#include "common/dataclasses.h"
#include <optional>
#include <string>
#include <vector>
#include <iostream>

namespace xenon::cli {
    
    using config::CompilerConfig;
    using config::Command;
    using config::OptimisationLevel;
    using config::WarningLevel;

    void print_usage();

    CompilerConfig parse_cli(int argc, char* argv[]);
}