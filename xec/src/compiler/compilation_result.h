#pragma once

#include "common/result/result.h"

#include <filesystem>
#include <optional>

namespace xenon {

    struct CompilationOutput {
        std::optional<std::filesystem::path> output_path;
    };

    using CompilationResult = result::Output<CompilationOutput>;

} // namespace xenon