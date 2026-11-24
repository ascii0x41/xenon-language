#pragma once

#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "token_kind.h"

namespace xenon::lexer {


    inline const std::unordered_map<std::string_view, TokenKind> KEYWORDS = {
        {"let", TokenKind::LET},
        {"func", TokenKind::FUNC},
        {"type", TokenKind::TYPE},
        {"impl", TokenKind::IMPL},
        {"static", TokenKind::STATIC},
        {"pub", TokenKind::PUB},
        {"mut", TokenKind::MUT},
        {"raw", TokenKind::RAW},
        {"self", TokenKind::SELF},
        {"if", TokenKind::IF},
        {"else", TokenKind::ELSE},
        {"while", TokenKind::WHILE},
        {"for", TokenKind::FOR},
        {"in", TokenKind::IN},
        {"return", TokenKind::RETURN},
        {"break", TokenKind::BREAK},
        {"continue", TokenKind::CONTINUE},
        {"import", TokenKind::IMPORT},
        {"module", TokenKind::MODULE},
        {"use", TokenKind::USE},
        {"true", TokenKind::TRUE},
        {"false", TokenKind::FALSE},
        {"nullptr", TokenKind::NULLPTR},
    };

    inline const std::unordered_set<std::string_view> INTEGER_SUFFIXES = {
        "i8", "i16", "i32", "i64",
        "u8", "u16", "u32", "u64",
        "usize",
    };

    inline const std::unordered_set<std::string_view> FLOATING_POINT_SUFFIXES = {
        "f32",
        "f64"
    };

} // namespace xenon::lexer