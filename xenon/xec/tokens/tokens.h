#pragma once

#include "common/dataclasses.h"

#include <string>
#include <vector>
#include <format>
#include <unordered_set>

namespace xenon::tokens {

    enum class TokenType {
        // =========================================================================
        // KEYWORDS
        // =========================================================================

        // Declarations
        LET,        // let   - variable declaration
        FUNC,       // func
        OPERATOR,   // operator - operator overload declaration
        CLASS,      // class - class declaration
        IMPL,       // impl - class implementation block
        STATIC,     // static - static member
        PUB,        // pub - public declaration
        NEW,        // new   - heap allocation
        DELETE,     // delete - memory free

        MUT,        // mut   - marks a trait method signature as mutating
    
        // Type qualifiers
        REF,        // ref   - borrowed reference
        PTR,        // ptr   - raw pointer (unsafe)
//        BOX,        // box   - boxed pointer (heap-allocated)

        // Control flow
        IF,         // if
        ELSE,       // else
        WHILE,      // while
        FOREACH,    // foreach
        IN,         // in

        // Jump
        RETURN,     // return
        BREAK,      // break
        CONTINUE,   // continue

        // Module system
        IMPORT,     // import
        EXPORT,     // export
        MODULE,     // module

        // Special value keywords
        TRUE,       // true
        FALSE,      // false
        NULLPTR,    // nullptr

        // IDENTIFIERS & LITERALS

        IDENTIFIER,
        INT_LITERAL,        // 42  0xFF  0b1010  0o77
        FLOAT_LITERAL,      // 3.14  6.022E23
        STRING_LITERAL,     // "hello"

        // OPERATORS

        // Initialisation / assignment
        EQ,             // =    
        PLUS_EQ,        // +=
        MINUS_EQ,       // -=
        STAR_EQ,        // *=
        SLASH_EQ,       // /=
        PERCENT_EQ,     // %=
        AMP_EQ,         // &=
        PIPE_EQ,        // |=
        CARET_EQ,       // ^=
        LT_LT_EQ,       // <<=
        GT_GT_EQ,       // >>=

        // Arithmetic
        PLUS,           // +
        MINUS,          // -
        STAR,           // *
        SLASH,          // /
        PERCENT,        // %

        // Comparison
        EQ_EQ,          // ==
        BANG_EQ,        // !=
        LT,             // <
        GT,             // >
        LTE,            // <=
        GTE,            // >=

        // Logical
        BANG,           // !
        AND,            // &&
        OR,             // ||

        // Bitwise
        AMP,            // &
        PIPE,           // |
        TILDE,          // ~
        CARET,          // ^
        LT_LT,          // <<
        GT_GT,          // >>

        // Other expression operators
        QUESTION,       // ?    ternary condition

        // DELIMITERS

        LPAREN,         // (
        RPAREN,         // )
        LBRACE,         // {
        RBRACE,         // }
        LBRACKET,       // [
        RBRACKET,       // ]

        // PUNCTUATION

        COMMA,          // ,
        SEMICOLON,      // ;
        COLON,          // :
        COLON_COLON,    // ::
        DOT,            // .
        ARROW,          // ->

        // SPECIAL
        EOF_TOKEN,
    };

    inline const std::unordered_map<std::string, TokenType> KEYWORDS = {
        // Declarations
        {"let",       TokenType::LET},
        {"func",      TokenType::FUNC},
        {"operator",  TokenType::OPERATOR},
        {"class",     TokenType::CLASS},
        {"impl",      TokenType::IMPL},
        {"static",    TokenType::STATIC},
        {"pub",       TokenType::PUB},
        {"new",       TokenType::NEW},
        {"delete",    TokenType::DELETE},
        // Modifiers
        {"mut",       TokenType::MUT},
        // Type qualifiers
        {"ptr",       TokenType::PTR},
        {"ref",       TokenType::REF},
//        {"box",       TokenType::BOX},
        // Control flow
        {"if",        TokenType::IF},
        {"else",      TokenType::ELSE},
        {"while",     TokenType::WHILE},
        {"foreach",   TokenType::FOREACH},
        {"in",        TokenType::IN},
        // Jump
        {"return",    TokenType::RETURN},
        {"break",     TokenType::BREAK},
        {"continue",  TokenType::CONTINUE},
        // Module system
        {"import",    TokenType::IMPORT},
        {"export",    TokenType::EXPORT},
        {"module",    TokenType::MODULE},
        // Value keywords
        {"true",      TokenType::TRUE},
        {"false",     TokenType::FALSE},
        {"nullptr",   TokenType::NULLPTR},
    };

    inline const std::unordered_set<std::string> INTEGER_SUFFIXES = {
        "i8",
        "i16",
        "i32",
        "i64",
        "u8",
        "u16",
        "u32",
        "u64",
    };

    inline const std::unordered_set<std::string> FLOATING_POINT_SUFFIXES = {
        "f32",
        "f64"
    };

    inline std::string token_type_to_string(TokenType t) {
        switch (t) {
            // Keywords
            case TokenType::LET:            return "let";
            case TokenType::FUNC:           return "func";
            case TokenType::OPERATOR:       return "operator";
            case TokenType::CLASS:          return "class";
            case TokenType::IMPL:           return "impl";
            case TokenType::STATIC:         return "static";
            case TokenType::PUB:            return "pub";
            case TokenType::NEW:            return "new";
            case TokenType::DELETE:         return "delete";
            case TokenType::MUT:            return "mut";
            case TokenType::PTR:            return "ptr";
//            case TokenType::BOX:            return "box";
            case TokenType::REF:            return "ref";
            case TokenType::IF:             return "if";
            case TokenType::ELSE:           return "else";
            case TokenType::WHILE:          return "while";
            case TokenType::FOREACH:        return "foreach";
            case TokenType::IN:             return "in";
            case TokenType::RETURN:         return "return";
            case TokenType::BREAK:          return "break";
            case TokenType::CONTINUE:       return "continue";
            case TokenType::IMPORT:         return "import";
            case TokenType::EXPORT:         return "export";
            case TokenType::MODULE:         return "module";
            case TokenType::TRUE:           return "true";
            case TokenType::FALSE:          return "false";

            // Identifiers & literals
            case TokenType::IDENTIFIER:         return "identifier";
            case TokenType::INT_LITERAL:        return "int literal";
            case TokenType::FLOAT_LITERAL:      return "float literal";
            case TokenType::STRING_LITERAL:     return "string literal";

            // Operators
            case TokenType::EQ:          return "=";
            case TokenType::PLUS_EQ:     return "+=";
            case TokenType::MINUS_EQ:    return "-=";
            case TokenType::STAR_EQ:     return "*=";
            case TokenType::SLASH_EQ:    return "/=";
            case TokenType::PERCENT_EQ:  return "%=";
            case TokenType::AMP_EQ:      return "&=";
            case TokenType::PIPE_EQ:     return "|=";
            case TokenType::CARET_EQ:    return "^=";
            case TokenType::LT_LT_EQ:    return "<<=";
            case TokenType::GT_GT_EQ:    return ">>=";
            case TokenType::PLUS:        return "+";
            case TokenType::MINUS:       return "-";
            case TokenType::STAR:        return "*";
            case TokenType::SLASH:       return "/";
            case TokenType::PERCENT:     return "%";
            case TokenType::EQ_EQ:       return "==";
            case TokenType::BANG_EQ:     return "!=";
            case TokenType::LT:          return "<";
            case TokenType::GT:          return ">";
            case TokenType::LTE:         return "<=";
            case TokenType::GTE:         return ">=";
            case TokenType::BANG:        return "!";
            case TokenType::AND:         return "&&";
            case TokenType::OR:          return "||";
            case TokenType::AMP:         return "&";
            case TokenType::PIPE:        return "|";
            case TokenType::TILDE:       return "~";
            case TokenType::CARET:       return "^";
            case TokenType::LT_LT:       return "<<";
            case TokenType::GT_GT:       return ">>";
            case TokenType::QUESTION:    return "?";

            // Delimiters
            case TokenType::LPAREN:      return "(";
            case TokenType::RPAREN:      return ")";
            case TokenType::LBRACE:      return "{";
            case TokenType::RBRACE:      return "}";
            case TokenType::LBRACKET:    return "[";
            case TokenType::RBRACKET:    return "]";

            // Punctuation
            case TokenType::COMMA:       return ",";
            case TokenType::SEMICOLON:   return ";";
            case TokenType::COLON:       return ":";
            case TokenType::COLON_COLON: return "::";
            case TokenType::DOT:         return ".";
            case TokenType::ARROW:       return "->";

            case TokenType::EOF_TOKEN:   return "<EOF>";
            default:                     return "<unknown>";
        }
    }


    struct Token {
        TokenType type;
        std::string lexeme;  // raw source text
        common::SourceLocation location;

        Token(TokenType t, std::string lex, common::SourceLocation l)
            : type(t), lexeme(std::move(lex)), location(std::move(l)) {}

        std::string to_string() const {
            const auto loc = location.format();

            if (loc.empty()) {
                return std::format("Token {}({})",
                    token_type_to_string(type),
                    lexeme);
            }

            return std::format("{}: Token {}({})",
                loc,
                token_type_to_string(type),
                lexeme);
        }
    };

    using TokenStream = std::vector<Token>;

}
