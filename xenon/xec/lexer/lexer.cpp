#include "lexer.h"

namespace xenon::lexer {

    TokenStream Lexer::lex() {
        if (source_.empty()) {
            tokens_.emplace_back(TokenType::EOF_TOKEN, "", SourceLocation(1, 1, file_));
            return std::move(tokens_);
        }
        
        try {
            while (!is_at_end()) {
                start_ = current_;
                scan_token();
            }

            tokens_.emplace_back(TokenType::EOF_TOKEN, "", SourceLocation(line_, column_, file_));
            return std::move(tokens_);
            
        } catch (const CompilerException& e) {
            g_diagnostics.error(e.what(), e.location);
            return {};
        }
    }

    void Lexer::add_token(TokenType type, std::string lexeme) {
        std::string view = lexeme;
        if (view.empty()) {
            view = std::string(source_.data() + start_, current_ - start_);
        } else {
            owned_lexemes_.emplace_back(view);
            view = owned_lexemes_.back();
        }
        tokens_.emplace_back(type, view, SourceLocation(line_, token_start_column(), file_));
        std::cout << "Added token: " << view << std::endl;
    }

    uint32_t Lexer::token_start_column() const {
        uint32_t codepoints = 0;
        for (size_t i = start_; i < current_; ++i) {
            unsigned char b = static_cast<unsigned char>(source_[i]);
            if (b != '\n' && !is_utf8_continuation(b))
                ++codepoints;
        }
        return column_ - codepoints;
    }
    

    void Lexer::scan_token() {
        unsigned char c = advance();

        switch (c) {
            // -- Single-character tokens -------------------------------------------
            case '(': add_token(TokenType::LPAREN);    break;
            case ')': add_token(TokenType::RPAREN);    break;
            case '{': add_token(TokenType::LBRACE);    break;
            case '}': add_token(TokenType::RBRACE);    break;
            case '[': add_token(TokenType::LBRACKET);  break;
            case ']': add_token(TokenType::RBRACKET);  break;
            case ',': add_token(TokenType::COMMA);     break;
            case ';': add_token(TokenType::SEMICOLON); break;
            case '?': add_token(TokenType::QUESTION);  break;

            // -- Colon  :  ::  -----------------------------------------------
            case ':':
                if      (match(':')) add_token(TokenType::COLON_COLON);
                else                 add_token(TokenType::COLON);
                break;

            // -- Dot  .  ----------------------------------------------------------
            case '.':
                add_token(TokenType::DOT);
                break;

            // -- Plus  +  +=  -----------------------------------------------------
            case '+':
                add_token(match('=') ? TokenType::PLUS_EQ : TokenType::PLUS);
                break;

            // -- Minus  -  -=  ->  ------------------------------------------------
            case '-':
                if      (match('>')) add_token(TokenType::ARROW);
                else if (match('=')) add_token(TokenType::MINUS_EQ);
                else                 add_token(TokenType::MINUS);
                break;

            // -- Star  *  *=  -----------------------------------------------------
            case '*':
                add_token(match('=') ? TokenType::STAR_EQ : TokenType::STAR);
                break;

            // -- Percent  %  %=  --------------------------------------------------
            case '%':
                add_token(match('=') ? TokenType::PERCENT_EQ : TokenType::PERCENT);
                break;

            // -- Slash  /  /=  //  /*  --------------------------------------------
            case '/':
                if      (match('/')) skip_line_comment();
                else if (match('*')) skip_block_comment();
                else if (match('=')) add_token(TokenType::SLASH);
                else                 add_token(TokenType::SLASH_EQ);
                break;

            // -- Equals  =  ==  =>  -----------------------------------------------
            case '=':
                if      (match('=')) add_token(TokenType::EQ_EQ);
                else                 add_token(TokenType::EQ);
                break;

            // -- Bang  !  !=  -----------------------------------------------------
            case '!':
                add_token(match('=') ? TokenType::BANG_EQ : TokenType::BANG);
                break;

            // -- Less  <  <=  <<  <<=  --------------------------------------------
            case '<':
                if      (match('<')) add_token(match('=') ? TokenType::LT_LT_EQ : TokenType::LT_LT);
                else if (match('=')) add_token(TokenType::LTE);
                else                 add_token(TokenType::LT);
                break;

            // -- Greater  >  >=  >>  >>=  -----------------------------------------
            case '>':
                if      (match('>')) add_token(match('=') ? TokenType::GT_GT_EQ : TokenType::GT_GT);
                else if (match('=')) add_token(TokenType::GTE);
                else                 add_token(TokenType::GT);
                break;

            // -- Ampersand  &  &&  &=  --------------------------------------------
            case '&':
                if      (match('&')) add_token(TokenType::AND);
                else if (match('=')) add_token(TokenType::AMP_EQ);
                else                 add_token(TokenType::AMP);
                break;

            // -- Pipe  |  ||  |=  -------------------------------------------------
            case '|':
                if      (match('|')) add_token(TokenType::OR);
                else if (match('=')) add_token(TokenType::PIPE_EQ);
                else                 add_token(TokenType::PIPE);
                break;

            // -- Caret  ^  ^=  ----------------------------------------------------
            case '^':
                add_token(match('=') ? TokenType::CARET_EQ : TokenType::CARET);
                break;

            // -- Tilde  ~  ----------------------------------------------------
            case '~':
                add_token(TokenType::TILDE);
                break;

            // -- String literals  "…"  --------------------------------------------
            case '"':
                scan_string();
                break;

            // -- Whitespace --------------------------------------------------------
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                break;

            // -- Numbers & identifiers ---------------------------------------------
            default: {
                if (c == '0' && (peek() == 'x' || peek() == 'b' || peek() == 'o')) {
                    scan_prefixed_number();
                } else if (is_digit(c)) {
                    scan_number();
                } else if (is_ident_start(c)) {
                    scan_identifier();
                } else {
                    throw CompilerException(
                        std::format("Unexpected character U+{:04X}", static_cast<unsigned>(c)), loc()
                    );
                }
                break;
            }
        }
    }

    void Lexer::scan_identifier() {
        while (is_ident_continue(peek())) advance();

        std::string text = source_.substr(start_, current_ - start_);
        auto it = KEYWORDS.find(text);
        add_token(it != KEYWORDS.end() ? it->second : TokenType::IDENTIFIER, text);
    }

    // -- Number parsing with suffix support ---------------------------------------

    void Lexer::scan_number() {
        // The first digit was already consumed by scan_token.
        while (is_digit(peek())) advance();
        
        bool is_float = false;
        
        // Fractional part: only if followed by another digit (avoids eating the
        // '.' in range expressions like 0..10).
        if (peek() == '.' && is_digit(peek_next())) {
            is_float = true;
            advance();  // '.'
            while (is_digit(peek())) advance();
        }
        
        // Scientific notation: e or E optionally followed by + or -
        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            advance();  // 'e' / 'E'
            if (peek() == '+' || peek() == '-') advance();
            if (!is_digit(peek()))
                throw CompilerException("Expected digits after exponent", loc());
            while (is_digit(peek())) advance();
        }
        
        // Parse suffix
        std::string suffix;
        
        // Check for floating point suffixes (f32, f64)
        if ((peek() == 'f' || peek() == 'd') && !is_ident_continue(peek_next())) {
            // Single-character float suffix: f or d
            suffix = std::string(1, static_cast<char>(advance()));
            is_float = true;
        } else if (peek() == 'f' && (peek_next() == '3' || peek_next() == '6')) {
            // f32 or f64
            advance();
            std::string width;
            width += static_cast<char>(advance());  // first digit
            if (peek() == '2' || peek() == '4') {
                width += static_cast<char>(advance());  // second digit
            }
            suffix = "f" + width;
            is_float = true;
            
            // Validate suffix is known
            if (FLOATING_POINT_SUFFIXES.find(suffix) == FLOATING_POINT_SUFFIXES.end()) {
                throw CompilerException(std::format("Unknown float suffix '{}'", suffix), loc());
            }
        } else if ((peek() == 'i' || peek() == 'u') && is_digit(peek_next())) {
            // Integer suffixes: i8, i16, i32, i64, u8, u16, u32, u64
            suffix += static_cast<char>(advance());  // 'i' or 'u'
            while (is_digit(peek())) {
                suffix += static_cast<char>(advance());
            }
            
            // Validate suffix is known
            if (INTEGER_SUFFIXES.find(suffix) == INTEGER_SUFFIXES.end()) {
                throw CompilerException(std::format("Unknown integer suffix '{}'", suffix), loc());
            }
        }

        // Emit the appropriate token
        if (is_float) {
            add_token(TokenType::FLOAT_LITERAL, source_.substr(start_, current_ - start_));
        } else {
            add_token(TokenType::INT_LITERAL, source_.substr(start_, current_ - start_));
        }
    }

    // -- Prefixed integer literals 0x… 0b… 0o… -----------------------------------

    void Lexer::scan_prefixed_number() {
        // '0' already consumed; peek() == 'x' | 'b' | 'o'.
        unsigned char prefix = advance();
        
        if (prefix == 'x') {
            if (!is_hex_digit(peek()))
                throw CompilerException("Expected hex digits after '0x'", loc());
            while (is_hex_digit(peek())) advance();
        } else if (prefix == 'b') {
            if (!is_bin_digit(peek()))
                throw CompilerException("Expected binary digits after '0b'", loc());
            while (is_bin_digit(peek())) advance();
        } else {  // 'o'
            if (!is_oct_digit(peek()))
                throw CompilerException("Expected octal digits after '0o'", loc());
            while (is_oct_digit(peek())) advance();
        }
        
        // Optional integer suffix
        if ((peek() == 'u' || peek() == 'i') && is_digit(peek_next())) {
            std::string suffix;
            suffix += static_cast<char>(advance());  // 'u' or 'i'
            while (is_digit(peek())) {
                suffix += static_cast<char>(advance());
            }
            
            if (INTEGER_SUFFIXES.find(suffix) == INTEGER_SUFFIXES.end()) {
                throw CompilerException(std::format("Unknown integer suffix '{}'", suffix), loc());
            }
        }
        
        add_token(TokenType::INT_LITERAL, source_.substr(start_, current_ - start_));
    }

    // -- Regular string with escape sequences -------------------------------------

    void Lexer::scan_string() {
        // Opening '"' already consumed.
        std::string value;
        
        while (peek() != '"' && !is_at_end()) {
            if (peek() == '\\') {
                EscapeResult esc = decode_escape_sequence();
                value += esc.value;
                // Note: current_ already advanced by decode_escape_sequence
            } else {
                value += static_cast<char>(advance());
            }
        }
        
        if (is_at_end())
            throw CompilerException("Unterminated string literal", loc());
        
        advance();  // closing '"'
        add_token(TokenType::STRING_LITERAL, std::move(value));
    }

    void Lexer::scan_char() {
        // Opening '\'' already consumed.
        std::string value;
        
        if (peek() == '\\') {
            EscapeResult esc = decode_escape_sequence();
            value += esc.value;
        } else {
            value += static_cast<char>(advance());
            if (value.size() != 1)
                throw CompilerException("Character literal must be a single character", loc());
        }
        
        if (peek() != '\'')
            throw CompilerException("Unterminated character literal", loc());
        
        advance();  // closing '\''
        add_token(TokenType::STRING_LITERAL, std::move(value));
    }


    Lexer::EscapeResult Lexer::decode_escape_sequence() {
        if (peek() != '\\') {
            return {std::string(1, static_cast<char>(advance())), 1};
        }
        
        size_t start_pos = current_;
        advance();  // consume backslash
        
        if (is_at_end()) {
            throw CompilerException("Unterminated escape sequence", loc());
        }
        
        uchar c = static_cast<uchar>(advance());
        
        // Simple escapes: \n, \t, \r, \\, \", \', \0
        if (c == 'n' || c == 'r' || c == 't' || c == '\\' || c == '"' || c == '\'' || c == '0') {
            return {std::string(1, decode_simple_escape(c)), current_ - start_pos};
        }
        
        // Hex escape: \xHH
        if (c == 'x') {
            if (!is_hex_digit(peek())) {
                throw CompilerException("Expected hex digits after \\x", loc());
            }
            
            std::string hex;
            for (int i = 0; i < 2; ++i) {
                if (!is_hex_digit(peek())) break;
                hex += static_cast<char>(advance());
            }
            
            if (hex.empty()) {
                throw CompilerException("Expected hex digits after \\x", loc());
            }
            
            char value = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            return {std::string(1, value), current_ - start_pos};
        }
        
        // Unicode escape: \u{XXXX} (supports 1-6 hex digits)
        if (c == 'u') {
            if (peek() != '{') {
                throw CompilerException("Expected '{' after \\u", loc());
            }
            advance();  // consume '{'
            
            std::string hex;
            while (peek() != '}' && !is_at_end()) {
                if (!is_hex_digit(peek())) {
                    throw CompilerException("Invalid hex digit in Unicode escape", loc());
                }
                hex += static_cast<char>(advance());
            }
            
            if (is_at_end()) {
                throw CompilerException("Unterminated Unicode escape sequence", loc());
            }
            
            advance();  // consume '}'
            
            if (hex.empty()) {
                throw CompilerException("Expected hex digits in Unicode escape", loc());
            }
            
            // Convert hex to codepoint
            unsigned long codepoint = std::strtoul(hex.c_str(), nullptr, 16);
            if (codepoint > 0x10FFFF) {
                throw CompilerException("Unicode codepoint out of range", loc());
            }
            
            // Convert codepoint to UTF-8
            std::string utf8;
            if (codepoint < 0x80) {
                utf8 += static_cast<char>(codepoint);
            } else if (codepoint < 0x800) {
                utf8 += static_cast<char>(0xC0 | (codepoint >> 6));
                utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                utf8 += static_cast<char>(0xE0 | (codepoint >> 12));
                utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
            } else {
                utf8 += static_cast<char>(0xF0 | (codepoint >> 18));
                utf8 += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
            }
            
            return {utf8, current_ - start_pos};
        }
        
        // Unknown escape sequence
        throw CompilerException(std::format("Unknown escape sequence '\\{}'", c), loc());
    }

    uchar Lexer::decode_simple_escape(uchar c) {
        switch (c) {
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case '\\': return '\\';
            case '"': return '"';
            case '\'': return '\'';
            case '0': return '\0';  // null character
            default: return c;  // Unknown escape, return as-is (will be flagged)
        }
    }

    void Lexer::skip_line_comment() {
        // '//' already consumed.
        while (peek() != '\n' && !is_at_end()) advance();
    }

    void Lexer::skip_block_comment() {
        // '/*' already consumed. Supports nesting.
        int depth = 1;
        while (depth > 0 && !is_at_end()) {
            if      (peek() == '/' && peek_next() == '*') { advance(); advance(); ++depth; }
            else if (peek() == '*' && peek_next() == '/') { advance(); advance(); --depth; }
            else advance();
        }
        if (depth > 0)
            throw CompilerException("Unterminated block comment", loc());
    }

}