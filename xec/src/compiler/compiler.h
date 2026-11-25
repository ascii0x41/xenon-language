#pragma once

#include "common/config/resolved_configs.h"
#include "compiler/compilation_result.h"
#include "compiler/module.h"
#include "parser/parser_result.h"
#include "semantic/analyser_result.h"
#include "codegen/codegen_result.h"

namespace xenon::compiler {

class Compiler {
public:
    explicit Compiler(const config::ResolvedConfigs& config, bool build)
        : config_(config), build_(build) {}

    xenon::CompilationResult compile();

    static xenon::CompilationResult compile(
        const config::ResolvedConfigs& config,
        bool build
    ) {
        return Compiler(config, build).compile();
    }

private:
    const config::ResolvedConfigs& config_;
    std::vector<Module> modules_;
    bool build_;

    /**
     * Invoke Parser
     * Invoke Lexer
     */
    parser::ParserOutput parse_source(
        const std::filesystem::path& source_path
    );

    std::vector<parser::ParserOutput> parse_sources();

    /**
     * Check for duplicate module names (error)
     * Merge root-level modules
     * Sort modules_ into topological order
     */
    void pre_analyse();

    /**
     * Invoke Semantic Analyser
     */
    sema::AnalyserOutput analyse();

    /**
     * Invoke CodeGenerator
     * Invoke Linker
     */
    codegen::CodegenOutput generate_code();

};

} // namespace xenon::compiler
