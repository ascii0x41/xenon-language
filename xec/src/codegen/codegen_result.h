#pragma once

#include "common/result/result.h"

#include <filesystem>
#include <vector>

namespace xenon::codegen {

    struct Output {
        std::vector<std::filesystem::path> object_files;
    };

    using CodegenOutput = result::Output<Output>;

} // namespace xenon::codegen
