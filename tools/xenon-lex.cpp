#include "common/source/source_file_manager.h"
#include "lexer/lexer.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: xenon-lex <file.xe>\n"; return 2; }
    const auto id = xenon::source::g_source_file_manager.load(argv[1]);
    const auto& source = xenon::source::g_source_file_manager.get(id);
    auto result = xenon::lexer::Lexer::lex_file(source);
    for (const auto& token : result.value()) {
        if (token.kind == xenon::lexer::TokenKind::Eof) continue;
        const auto lc = source.line_col(token.span.start);
        auto text = source.slice(token.span);
        std::cout << lc.line << ':' << lc.column << ' ' << xenon::lexer::to_string(token.kind) << " \"";
        for (char c : text) {
            if (c == '\\') std::cout << "\\\\";
            else if (c == '"') std::cout << "\\\"";
            else if (c == '\n') std::cout << "\\n";
            else if (c == '\r') std::cout << "\\r";
            else if (c == '\t') std::cout << "\\t";
            else std::cout << c;
        }
        std::cout << '"' << '\n';
    }
    for (const auto& d : result.diagnostics().diagnostics()) {
        std::cerr << d.message;
        if (d.span) { auto lc=source.line_col(d.span->start); std::cerr << " @ " << lc.line << ':' << lc.column; }
        std::cerr << '\n';
    }
    return result.success() ? 0 : 1;
}
