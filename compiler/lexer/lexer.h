#pragma once

#include "tokens/tokens.h"
#include "common/dataclasses.h"
#include "common/diagnostics.h"

#include <string>
#include <vector>

namespace xenon::lexer {

    using uchar = unsigned char;

    // UTF-8 continuation bytes have the form 10xxxxxx.
    // Only leading bytes (and ASCII) should increment the column counter.
    inline bool is_utf8_continuation(uchar c) {
        return (c & 0xC0) == 0x80;
    }

    inline bool is_whitespace(uchar c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    inline bool is_digit(uchar c) {
        return c >= '0' && c <= '9';
    }

    inline bool is_hex_digit(uchar c) {
        return is_digit(c)
            || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
    }

    inline bool is_bin_digit(uchar c) { return c == '0' || c == '1'; }
    inline bool is_oct_digit(uchar c) { return c >= '0' && c <= '7'; }

    // Identifier start: ASCII letter or underscore.
    // Non-ASCII bytes are explicitly allowed so that Unicode identifiers
    // (e.g. Greek letters for maths) are scanned without errors, though the
    // type-checker may later reject them if policy requires ASCII-only names.
    inline bool is_ident_start(uchar c) {
        return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || c == '_'
            || c > 127;   // any non-ASCII leading byte
    }

    inline bool is_ident_continue(uchar c) {
        return is_ident_start(c) || is_digit(c) || is_utf8_continuation(c);
    }



    using tokens::Token;
    using tokens::TokenType;
    using tokens::TokenStream;
    using tokens::INTEGER_SUFFIXES;
    using tokens::FLOATING_POINT_SUFFIXES;
    using tokens::KEYWORDS;
    using common::SourceLocation;

    class Lexer {
    public:
        static TokenStream lex(const std::string& source, const std::string& file) {
            return Lexer(source, file).lex();
        }
    private:
        explicit Lexer(std::string source, std::string file)
            : source_(std::move(source)), file_(std::move(file)) {}

        // Lex the full source and return a TokenStream ending with EOF_TOKEN.
        // Safe to call multiple times — resets state on each call.
        TokenStream lex();

        // -- Source & output ------------------------------------------------------
        std::string source_;
        std::string file_;
        TokenStream tokens_;
        std::vector<std::string> owned_lexemes_;

        // -- Position -------------------------------------------------------------
        size_t start_   = 0;   // byte offset of the current token's first uchar
        size_t current_ = 0;   // byte offset of the next unread uchar
        uint32_t    line_    = 1;
        uint32_t    column_  = 1;   // Unicode codepoint column (1-based)

        // -- Primitives ------------------------------------------------------------

        inline bool is_at_end() const { return current_ >= source_.size(); }

        // Consume and return the current byte.
        // Column advances by 1 for every non-continuation byte (i.e. every
        // Unicode codepoint start), so multi-byte sequences count as 1 column.
        uchar advance() {
            uchar c = static_cast<uchar>(source_[current_++]);

            if (c == '\n') {
                ++line_;
                column_ = 1;
            } else if (!is_utf8_continuation(c)) {
                ++column_;
            }

            return c;
        }

        uchar peek() const { return is_at_end() ? '\0' : static_cast<uchar>(source_[current_]); }
        uchar peek_next() const { 
            return (current_ + 1 < source_.size())
            ? static_cast<uchar>(source_[current_ + 1])
            : '\0';
        }

        // Consume current byte if it equals `expected`; return true if consumed.
        bool match(uchar expected) {
            if (is_at_end() || source_[current_] != expected) return false;
            advance();
            return true;
        }

        SourceLocation loc() const { return SourceLocation(line_, column_, file_); }

        // -- Token emission --------------------------------------------------------

        // Record a token whose column is the start of the current token.
        // Empty lexeme means source_[start_..current_) zero-copy view.
        // Non-empty lexeme is owned inside the lexer and exposed as string_view.
        void add_token(TokenType type, std::string lexeme = {});

        // Column of the first byte of the current token.
        uint32_t token_start_column() const;

        // -- Main dispatch ---------------------------------------------------------
        void scan_token();

        // -- Specialised scanners --------------------------------------------------
        void scan_identifier();
        void scan_number();
        void scan_prefixed_number();        // 0x...  0b…  0o…
        void scan_string();                 // "..."
        void scan_char();                   // '...'
        
        // -- Escape handling -------------------------------------------------------
        struct EscapeResult {
            std::string value;      // The decoded escape sequence (UTF-8 string)
            size_t bytes_consumed;  // Number of source bytes consumed
        };
        
        EscapeResult decode_escape_sequence();  // Handles \n, \t, \xHH, \u{HHHH}, etc.
        uchar decode_simple_escape(uchar c);      // \n, \t, \r, etc.

        // -- Comment skipping ------------------------------------------------------
        void skip_line_comment();          // // ...
        void skip_block_comment();         // /* ... */  (nested)
    };

} // namespace xenon
