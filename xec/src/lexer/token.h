#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "common/source/span.h"
#include "lexer/token_kind.h"

namespace xenon::lexer {

enum class NumericBase : std::uint8_t { Decimal, Hex, Binary, Octal };

struct TokenFlags {
    bool unsuffixed = false;
    bool ct_if = false;
    bool ct_else = false;
};

struct Token {
    TokenKind kind = TokenKind::Error;
    source::Span span;
    TokenFlags flags;
    NumericBase numeric_base = NumericBase::Decimal;
    std::string_view digit_text;
    std::string_view suffix;
};

using TokenStream = std::vector<Token>;

std::string_view to_string(TokenKind kind) noexcept;
inline std::string_view escape_token_kind(TokenKind kind) noexcept { return to_string(kind); }

} // namespace xenon::lexer
