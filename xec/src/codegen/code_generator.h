#pragma once

#include "codegen/codegen_result.h"
#include "common/config/resolved_configs.h"
#include "semantic/analyser_result.h"

#include <filesystem>

namespace xenon::codegen {

class CodeGenerator {
public:
    explicit CodeGenerator(const config::ResolvedConfigs& config)
        : config_(config) {}

    CodegenOutput generate(const sema::AnalyserOutput& analysed);

private:
    const config::ResolvedConfigs& config_;

    bool verify_pointer_width() const;
    bool write_llvm_ir(const std::filesystem::path& output_directory);
    bool compile_environment_c_source(const std::filesystem::path& output_directory);
};

} // namespace xenon::codegen
