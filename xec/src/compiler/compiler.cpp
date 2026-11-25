#include "compiler/compiler.h"
#include "common/source/source_file_manager.h"
#include "compiler/compilation_result.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <utility>
#include <vector>

namespace xenon::compiler {

    parser::ParserOutput Compiler::parse_source(const std::filesystem::path& source_path) {
        const source::FileID file = source::g_source_file_manager.load(source_path);
        const source::SourceFile& source = source::g_source_file_manager.get(file);

        auto lexed = lexer::Lexer::lex(source);
        if (!lexed) {
            return lexed.error();
        }

        return parser::Parser::parse(lexed.value(), file);
    }

    std::vector<parser::ParserOutput> Compiler::parse_sources() {
        std::vector<parser::ParserOutput> results;
        results.reserve(config_.source_files.size());

        for (const std::filesystem::path& source_path : config_.source_files) {
            results.push_back(parse_source(source_path));
        }

        return results;
    }

    void Compiler::pre_analyse() {}

    sema::AnalyserOutput Compiler::analyse() {
        sema::Output output;
        return output;
    }

    codegen::CodegenOutput Compiler::generate_code() {
        codegen::Output output;
        return output;
    }

    xenon::CompilationResult Compiler::compile() {
        diagnostics::DiagnosticCollector diagnostics;

        auto parser_results = parse_sources();
    
        for (const auto& parser_result : parser_results) {
            if (!parser_result.success()) {
                diagnostics.merge(parser_result.diagnostics());

                return {std::move(diagnostics), false};
            }
        }

        if (diagnostics.has_errors()) {
            return {std::move(diagnostics), false};
        }

        pre_analyse();

        auto analyse_result = analyse();

        if (!analyse_result.success()) {
            diagnostics.merge(analyse_result.diagnostics());

            return {std::move(diagnostics), false};
        }

        if (!build_) return {std::move(diagnostics), true};

        CompilationOutput output;

        auto codegen_result = generate_code();
        if (!codegen_result.success()) {
            diagnostics.merge(codegen_result.diagnostics());

            return {std::move(diagnostics), false};
        }

        return CompilationResult(std::move(output), std::move(diagnostics), true);
    }

} // namespace xenon::compiler
