#pragma once

#include "codegen/codegen_result.h"
#include "common/config/resolved_configs.h"

#include <filesystem>

namespace xenon::codegen {

class Linker {
public:
    explicit Linker(const config::ResolvedConfigs& config)
        : config_(config) {}

    CodegenOutput link(const CodegenOutput& objects);

private:
    const config::ResolvedConfigs& config_;
};

} // namespace xenon::codegen
