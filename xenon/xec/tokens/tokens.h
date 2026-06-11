#pragma once

#include "common/dataclasses.h"

#include <string>
#include <vector>
#include <format>
#include <unordered_set>

namespace xenon {

    enum class TokenType {
        // =========================================================================
        // KEYWORDS
        // =========================================================================

        // Declarations
        LET,        // let   — immutable binding
        VAR,        // var   — mutable binding
        FUNC,       // func
        LAMBDA,     // lambda — alternative to 'func' for anonymous functions
        CLASS,      // class
        TRAIT,      // trait
        IMPL,       // impl  — trait implementation list on a class
        ENUM,       // enum
        SCOPE,      // scope — namespace-like grouping
        TYPE,       // type  — type alias
        OPERATOR,   // operator — operator overload declaration
        NEW,        // new   — heap allocation
        DELETE,     // delete - memory free

        // Modifiers
        PUB,        // pub   — public
        STATIC,     // static
        MUT,        // mut   — marks a trait method signature as mutating

        // Type qualifiers
        REF,        // ref   — borrowed reference
        PTR,        // ptr   — raw pointer (unsafe)
        BOX,        // box   — boxed pointer (heap-allocated)

        // Control flow
        IF,         // if
        ELIF,       // elif
        ELSE,       // else
        WHILE,      // while
        DO,         // do
        FOREACH,    // foreach
        IN,         // in
        MATCH,      // match

        // Jump
        RETURN,     // return
        BREAK,      // break
        CONTINUE,   // continue
        THROW,      // throw

        // Exception handling
        TRY,        // try
        CATCH,      // catch
        FINALLY,    // finally

        // Module system
        IMPORT,     // import
        EXPORT,     // export
        MODULE,     // module
        AS,         // as

        // Special value keywords
        TRUE,       // true
        FALSE,      // false
        NULLPTR,    // nullptr
        THIS,       // this  — current instance inside a class method
        SELF,       // Self  — implementing type placeholder inside a trait


        // =========================================================================
        // ATTRIBUTES  (#name or #name("..."))
        // The lexer emits these as single tokens so the parser never sees '#'.
        // =========================================================================

        ATTR_INLINE,        // #inline
        ATTR_CONSTEXPR,     // #constexpr
        ATTR_DEPRECATED,    // #deprecated  /  #deprecated("msg")
        ATTR_DOCSTRING,     // #docstring("...")

        // =========================================================================
        // IDENTIFIERS & LITERALS
        // =========================================================================

        IDENTIFIER,
        INT_LITERAL,        // 42  0xFF  0b1010  0o77
        FLOAT_LITERAL,      // 3.14  6.022E23
        COMPLEX_LITERAL,    // 4j  2.5j  — imaginary part only; real part is separate
        STRING_LITERAL,     // "hello"
        RAW_STRING_LITERAL, // r"raw\nstring"  or  `backtick`

        // Interpolated string  $"Hello {name}"
        // The lexer splits these into a stream of three token types:
        INTERP_STRING_START,    // $"          — opening marker
        INTERP_STRING_PART,     // "text "     — literal text segment
        INTERP_STRING_END,      // "           — closing marker
        // Expression tokens between START and END are ordinary tokens.

        // =========================================================================
        // OPERATORS
        // =========================================================================

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
        TILDE_EQ,       // ~=
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

        // =========================================================================
        // DELIMITERS
        // =========================================================================

        LPAREN,         // (
        RPAREN,         // )
        LBRACE,         // {
        RBRACE,         // }
        LBRACKET,       // [
        RBRACKET,       // ]

        // =========================================================================
        // PUNCTUATION
        // =========================================================================

        COMMA,          // ,
        SEMICOLON,      // ;
        COLON,          // :
        COLON_COLON,    // ::
        DOT,            // .
        ARROW,          // ->
        FAT_ARROW,      // =>
        HASH,           // #

        // =========================================================================
        // SPECIAL
        // =========================================================================

        EOF_TOKEN,
    };

    // -----------------------------------------------------------------------------

    inline std::string token_type_to_string(TokenType t) {
        switch (t) {
            // Keywords
            case TokenType::LET:          return "let";
            case TokenType::VAR:          return "var";
            case TokenType::FUNC:         return "func";
            case TokenType::LAMBDA:       return "lambda";
            case TokenType::CLASS:        return "class";
            case TokenType::TRAIT:        return "trait";
            case TokenType::IMPL:         return "impl";
            case TokenType::ENUM:         return "enum";
            case TokenType::SCOPE:        return "scope";
            case TokenType::TYPE:         return "type";
            case TokenType::OPERATOR:     return "operator";
            case TokenType::NEW:          return "new";
            case TokenType::DELETE:       return "delete";
            case TokenType::PUB:          return "pub";
            case TokenType::STATIC:       return "static";
            case TokenType::MUT:          return "mut";
            case TokenType::PTR:          return "ptr";
            case TokenType::BOX:          return "box";
            case TokenType::REF:          return "ref";
            case TokenType::IF:           return "if";
            case TokenType::ELIF:         return "elif";
            case TokenType::ELSE:         return "else";
            case TokenType::WHILE:        return "while";
            case TokenType::DO:           return "do";
            case TokenType::FOREACH:      return "foreach";
            case TokenType::IN:           return "in";
            case TokenType::MATCH:        return "match";
            case TokenType::RETURN:       return "return";
            case TokenType::BREAK:        return "break";
            case TokenType::CONTINUE:     return "continue";
            case TokenType::THROW:        return "throw";
            case TokenType::TRY:          return "try";
            case TokenType::CATCH:        return "catch";
            case TokenType::FINALLY:      return "finally";
            case TokenType::IMPORT:       return "import";
            case TokenType::EXPORT:       return "export";
            case TokenType::MODULE:       return "module";
            case TokenType::AS:           return "as";
            case TokenType::TRUE:         return "true";
            case TokenType::FALSE:        return "false";
            case TokenType::THIS:         return "this";
            case TokenType::SELF:         return "Self";

            // Attributes
            case TokenType::ATTR_INLINE:      return "#inline";
            case TokenType::ATTR_CONSTEXPR:   return "#constexpr";
            case TokenType::ATTR_DEPRECATED:  return "#deprecated";
            case TokenType::ATTR_DOCSTRING:   return "#docstring";

            // Identifiers & literals
            case TokenType::IDENTIFIER:          return "identifier";
            case TokenType::INT_LITERAL:         return "int literal";
            case TokenType::FLOAT_LITERAL:       return "float literal";
            case TokenType::COMPLEX_LITERAL:     return "complex literal";
            case TokenType::STRING_LITERAL:      return "string literal";
            case TokenType::RAW_STRING_LITERAL:  return "raw string literal";
            case TokenType::INTERP_STRING_START: return "interp string start";
            case TokenType::INTERP_STRING_PART:  return "interp string part";
            case TokenType::INTERP_STRING_END:   return "interp string end";

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
            case TokenType::TILDE_EQ:    return "~=";
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
            case TokenType::FAT_ARROW:   return "=>";
            case TokenType::HASH:        return "#";

            case TokenType::EOF_TOKEN:   return "<EOF>";
            default:                     return "<unknown>";
        }
    }

    inline const std::unordered_map<std::string, TokenType> KEYWORDS = {
        // Declarations
        {"let",       TokenType::LET},
        {"var",       TokenType::VAR},
        {"func",      TokenType::FUNC},
        {"lambda",    TokenType::LAMBDA},  // alternative to 'func' for anonymous functions
        {"class",     TokenType::CLASS},
        {"trait",     TokenType::TRAIT},
        {"impl",      TokenType::IMPL},
        {"enum",      TokenType::ENUM},
        {"scope",     TokenType::SCOPE},
        {"type",      TokenType::TYPE},
        {"operator",  TokenType::OPERATOR},
        {"new",       TokenType::NEW},
        {"delete",    TokenType::DELETE},
        // Modifiers
        {"pub",       TokenType::PUB},
        {"static",    TokenType::STATIC},
        {"mut",       TokenType::MUT},
        // Type qualifiers
        {"ptr",       TokenType::PTR},
        {"ref",       TokenType::REF},
        {"box",       TokenType::BOX},
        // Control flow
        {"if",        TokenType::IF},
        {"elif",      TokenType::ELIF},
        {"else",      TokenType::ELSE},
        {"while",     TokenType::WHILE},
        {"do",        TokenType::DO},
        {"foreach",   TokenType::FOREACH},
        {"in",        TokenType::IN},
        {"match",     TokenType::MATCH},
        // Jump
        {"return",    TokenType::RETURN},
        {"break",     TokenType::BREAK},
        {"continue",  TokenType::CONTINUE},
        {"throw",     TokenType::THROW},
        // Exception handling
        {"try",       TokenType::TRY},
        {"catch",     TokenType::CATCH},
        {"finally",   TokenType::FINALLY},
        // Module system
        {"import",    TokenType::IMPORT},
        {"export",    TokenType::EXPORT},
        {"module",    TokenType::MODULE},
        {"as",        TokenType::AS},
        // Value keywords
        {"true",      TokenType::TRUE},
        {"false",     TokenType::FALSE},
        {"nullptr",   TokenType::NULLPTR},
        {"this",      TokenType::THIS},
        {"Self",      TokenType::SELF}
    };

    inline const std::unordered_map<std::string, TokenType> ATTRIBUTES = {
        {"inline",      TokenType::ATTR_INLINE},
        {"constexpr",   TokenType::ATTR_CONSTEXPR},
        {"deprecated",  TokenType::ATTR_DEPRECATED},
        {"docstring",   TokenType::ATTR_DOCSTRING},
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

    // -----------------------------------------------------------------------------

    struct Token {
        TokenType   type;
        std::string lexeme;  // raw source text
        SourceLocation location;

        Token(TokenType t, std::string lex, SourceLocation l)
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

} // namespace xenon
