#pragma once

#include "common/result/result.h"
#include "common/source/source_file.h"
#include "lexer/token.h"

namespace xenon::lexer {

class Lexer {
public:
    static result::Result<TokenStream> lex(const source::SourceFile& source);
    static result::Output<TokenStream> lex_file(const source::SourceFile& source);

    explicit Lexer(const source::SourceFile& source);
    result::Result<Token> next();
    result::Output<TokenStream> tokenize();

private:
    const source::SourceFile& source_;
    std::string_view text_;
    std::size_t start_ = 0;
    std::size_t current_ = 0;

    bool is_at_end() const noexcept;
    char peek() const noexcept;
    char peek_next() const noexcept;
    char peek_at(std::size_t i) const noexcept;
    char advance() noexcept;
    bool match(char c) noexcept;

    void scan(TokenStream&, std::vector<diagnostics::Diagnostic>&);
    void emit(TokenStream&, TokenKind, std::size_t begin, std::size_t end);
    void report(std::vector<diagnostics::Diagnostic>&, std::size_t begin, std::size_t end,
                const char* message);

    void scan_identifier(TokenStream&);
    void scan_sigiled(TokenStream&, std::vector<diagnostics::Diagnostic>&, bool pct);
    void scan_number(TokenStream&, std::vector<diagnostics::Diagnostic>&);
    void scan_string(TokenStream&, std::vector<diagnostics::Diagnostic>&);
    void scan_rune(TokenStream&, std::vector<diagnostics::Diagnostic>&);
};

} // namespace xenon::lexer
