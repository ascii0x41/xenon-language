#include "parser.h"

namespace xenon::parser {

    namespace {

        // Sets `ref` to `value` for the lifetime of the guard, then restores it.
        struct FlagGuard {
            bool& ref;
            bool  saved;
            FlagGuard(bool& r, bool value) : ref(r), saved(r) { ref = value; }
            ~FlagGuard() { ref = saved; }
            FlagGuard(const FlagGuard&) = delete;
            FlagGuard& operator=(const FlagGuard&) = delete;
        };

        // How a token is quoted in an error message.
        std::string describe_token(const Token& t) {
            switch (t.type) {
                case TokenType::EOF_TOKEN:          return "end of file";
                case TokenType::DOC_COMMENT:        return "doc comment";
                case TokenType::MODULE_DOC_COMMENT: return "module doc comment";
                default: return std::format("'{}'", escape_for_display(t.lexeme));
            }
        }

        // "42u8" -> {"42", "u8"};  "0xFFsize" -> {"0xFF", "size"}.
        // i, u and s are not digits in any base we lex, so the first one starts the suffix.
        std::pair<std::string, std::string> split_int_suffix(const std::string& lexeme) {
            const bool prefixed = lexeme.size() > 2 && lexeme[0] == '0' &&
                                  (lexeme[1] == 'x' || lexeme[1] == 'b' || lexeme[1] == 'o');
            const size_t pos = lexeme.find_first_of("ius", prefixed ? 2 : 0);
            if (pos == std::string::npos) return {lexeme, ""};
            return {lexeme.substr(0, pos), lexeme.substr(pos)};
        }

        // "1.5f32" -> {"1.5", "f32"};  "2d" -> {"2", "d"}.
        // Decimal floats only contain digits, '.', 'e', 'E', '+', '-' before the suffix.
        std::pair<std::string, std::string> split_float_suffix(const std::string& lexeme) {
            const size_t pos = lexeme.find_first_of("fd");
            if (pos == std::string::npos) return {lexeme, ""};
            return {lexeme.substr(0, pos), lexeme.substr(pos)};
        }

        bool is_overloadable_operator(TokenType t) {
            switch (t) {
                case TokenType::PLUS:    case TokenType::MINUS:   case TokenType::STAR:
                case TokenType::SLASH:   case TokenType::PERCENT:
                case TokenType::EQ_EQ:   case TokenType::BANG_EQ:
                case TokenType::LT:      case TokenType::GT:
                case TokenType::LTE:     case TokenType::GTE:
                case TokenType::BANG:    case TokenType::AND:     case TokenType::OR:
                case TokenType::AMP:     case TokenType::PIPE:    case TokenType::TILDE:
                case TokenType::CARET:   case TokenType::LT_LT:   case TokenType::GT_GT:
                case TokenType::PLUS_EQ: case TokenType::MINUS_EQ:
                case TokenType::STAR_EQ: case TokenType::SLASH_EQ:
                case TokenType::PERCENT_EQ:
                case TokenType::AMP_EQ:  case TokenType::PIPE_EQ: case TokenType::CARET_EQ:
                case TokenType::LT_LT_EQ: case TokenType::GT_GT_EQ:
                    return true;
                default:
                    return false;
            }
        }

    } // namespace

    bool Parser::match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }
    bool Parser::check(TokenType type) const {
        if (is_at_end()) return false;
        return peek().type == type;
    }
    bool Parser::accept(TokenType type) {
        if (check(type)) { advance(); return true; }
        return false;
    }

    Token Parser::expect(TokenType type, const std::string& msg) {
        if (accept(type)) {
            return previous();
        }

        std::string error_msg = msg;
        error_msg += std::format(" (expected '{}', got {})",
                                token_type_to_string(type), describe_token(peek()));

        throw CompilerException(error_msg, peek().location, Severity::ERROR);
    }

    ModuleAST Parser::parse() {
        ModuleAST ast;
        if (tokens_.empty()) return ast;   // the lexer failed; its diagnostics are already recorded

        parse_header(ast);
        while (!is_at_end()) {
            ast.root.declarations.push_back(parse_declaration());
        }
        check_dangling_metadata(pending_meta_);   // e.g. a file that ends with a `///`
        return ast;
    }

    // -- Doc comments & attributes ---------------------------------------------------

    // Collects any mix of `///` lines and `#[...]` groups. Consecutive `///` lines
    // become one doc string joined with '\n'.
    Parser::Metadata Parser::parse_metadata() {
        Metadata meta = std::move(pending_meta_);
        pending_meta_ = Metadata{};

        while (true) {
            if (check(TokenType::DOC_COMMENT)) {
                if (!meta.has_doc) {
                    meta.has_doc = true;
                    meta.doc_location = loc();
                } else {
                    meta.doc += '\n';
                }
                meta.doc += advance().lexeme;
            }
            else if (check(TokenType::HASH)) {
                if (meta.attributes.empty()) meta.attribute_location = loc();
                parse_attribute_group(meta.attributes);
            }
            else if (check(TokenType::MODULE_DOC_COMMENT)) {
                throw CompilerException("'//!' comments are only allowed at the very top of a file",
                                        peek().location, Severity::ERROR);
            }
            else {
                break;
            }
        }
        return meta;
    }

    // #[name]   #[name(a, b)]   #[first, second(1)]
    void Parser::parse_attribute_group(std::vector<Attribute>& out) {
        expect(TokenType::HASH, "Expected '#' to start an attribute");
        expect(TokenType::LBRACKET, "Expected '[' after '#' to start an attribute");
        do {
            if (!check(TokenType::IDENTIFIER) && !check(TokenType::COLON_COLON)) {
                throw CompilerException("Expected attribute name", peek().location, Severity::ERROR);
            }
            Attribute attr;
            attr.location = loc();
            attr.name = parse_name();
            if (check(TokenType::LPAREN)) {
                attr.args = parse_arguments();
            }
            out.push_back(std::move(attr));
        } while (accept(TokenType::COMMA));
        expect(TokenType::RBRACKET, "Expected ']' to close attribute");
    }

    void Parser::check_dangling_metadata(const Metadata& meta) {
        if (meta.has_doc) {
            throw CompilerException("Doc comment is not followed by a declaration",
                                    meta.doc_location, Severity::ERROR);
        }
        if (!meta.attributes.empty()) {
            throw CompilerException("Attribute is not followed by a declaration",
                                    meta.attribute_location, Severity::ERROR);
        }
    }

    void Parser::attach_metadata(Declaration& decl, Metadata&& meta) {
        decl.doc = std::move(meta.doc);
        decl.attributes = std::move(meta.attributes);
    }

    NamePtr Parser::parse_name() {
        bool is_global = accept(TokenType::COLON_COLON);

        if (!check(TokenType::IDENTIFIER)) {
            if (is_global) {
                throw CompilerException("Expected identifier after leading '::' in name", peek().location, Severity::ERROR);
            }
            throw CompilerException("Expected identifier for name", peek().location, Severity::ERROR);
        }

        auto name = std::make_unique<Name>(loc(), peek().lexeme, nullptr, is_global);
        advance();

        Name* tail = name.get();
        while (match(TokenType::COLON_COLON)) {
            if (!check(TokenType::IDENTIFIER)) {
                throw CompilerException("Expected identifier after '::' in qualified name", peek().location, Severity::ERROR);
            }

            auto next_name = std::make_unique<Name>(loc(), peek().lexeme);
            advance();
            tail->next = std::move(next_name);
            tail = tail->next.get();
        }

        return name;
    }

    TypeExprPtr Parser::parse_type_expression() {
        SourceLocation l = loc();

        if (accept(TokenType::STAR)) {
            bool mut = accept(TokenType::MUT);
            return std::make_unique<PointerTypeExpr>(l, parse_type_expression(), mut);
        }

        if (accept(TokenType::AMP)) {
            bool mut = accept(TokenType::MUT);
            return std::make_unique<ReferenceTypeExpr>(l, parse_type_expression(), mut);
        }

        if (accept(TokenType::LBRACKET)) {
            auto element_type = parse_type_expression();
            ExpressionPtr size_expr = nullptr;
            if (accept(TokenType::SEMICOLON)) {
                FlagGuard guard(no_struct_literal_, false);
                size_expr = parse_expression();
            }
            expect(TokenType::RBRACKET, "Expected ']' to close array type");
            return std::make_unique<ArrayTypeExpr>(l, std::move(element_type), std::move(size_expr));
        }

        auto name = parse_name();
        return std::make_unique<NamedTypeExpr>(l, std::move(name));
    }

    // `-> T`, or `void` when there is no arrow.
    TypeExprPtr Parser::parse_return_type() {
        SourceLocation l = loc();
        if (accept(TokenType::ARROW)) {
            return parse_type_expression();
        }
        return std::make_unique<NamedTypeExpr>(l, std::make_unique<Name>(l, "void"));
    }

    // `{ ... }`, or `;` for a declaration without a body (e.g. an extern function).
    BlockPtr Parser::parse_optional_body() {
        if (check(TokenType::LBRACE)) return parse_block();
        expect(TokenType::SEMICOLON, "Expected '{' or ';' after function signature");
        return nullptr;
    }


    std::vector<ExpressionPtr> Parser::parse_arguments() {
        std::vector<ExpressionPtr> args;

        expect(TokenType::LPAREN, "Expected '(' to start argument list");
        FlagGuard guard(no_struct_literal_, false);

        if (!check(TokenType::RPAREN)) {
            do {
                args.push_back(parse_expression());
            } while (accept(TokenType::COMMA));
        }

        expect(TokenType::RPAREN, "Expected ')' after argument list");
        return args;
    }

    std::vector<VariableDeclPtr> Parser::parse_parameters() {
        std::vector<VariableDeclPtr> params;

        expect(TokenType::LPAREN, "Expected '(' to start parameter list");

        if (!check(TokenType::RPAREN)) {
            do {
                SourceLocation param_loc = loc();
                bool mut = accept(TokenType::MUT);
                auto name = expect(TokenType::IDENTIFIER, "Expected parameter name in parameter list").lexeme;
                TypeExprPtr type_expr = nullptr;
                if (accept(TokenType::COLON)) {
                    type_expr = parse_type_expression();
                }
                params.push_back(std::make_unique<VariableDecl>(param_loc, std::move(name), std::move(type_expr), nullptr, mut));
            } while (accept(TokenType::COMMA));
        }

        expect(TokenType::RPAREN, "Expected ')' after parameter list");
        return params;
    }


    // Helper functions for token conversion
    BinaryOperatorKind token_to_binary_op(TokenType type) {
        switch (type) {
            case TokenType::PLUS: return BinaryOperatorKind::ADD;
            case TokenType::MINUS: return BinaryOperatorKind::SUBTRACT;
            case TokenType::STAR: return BinaryOperatorKind::MULTIPLY;
            case TokenType::SLASH: return BinaryOperatorKind::DIVIDE;
            case TokenType::PERCENT: return BinaryOperatorKind::MODULO;
            case TokenType::EQ_EQ: return BinaryOperatorKind::EQUAL;
            case TokenType::BANG_EQ: return BinaryOperatorKind::NOT_EQUAL;
            case TokenType::LT: return BinaryOperatorKind::LESS_THAN;
            case TokenType::LTE: return BinaryOperatorKind::LESS_EQUAL;
            case TokenType::GT: return BinaryOperatorKind::GREATER_THAN;
            case TokenType::GTE: return BinaryOperatorKind::GREATER_EQUAL;
            case TokenType::AND: return BinaryOperatorKind::LOGICAL_AND;
            case TokenType::OR: return BinaryOperatorKind::LOGICAL_OR;
            case TokenType::AMP: return BinaryOperatorKind::BITWISE_AND;
            case TokenType::PIPE: return BinaryOperatorKind::BITWISE_OR;
            case TokenType::CARET: return BinaryOperatorKind::BITWISE_XOR;
            case TokenType::LT_LT: return BinaryOperatorKind::SHIFT_LEFT;
            case TokenType::GT_GT: return BinaryOperatorKind::SHIFT_RIGHT;
            default:
                throw std::runtime_error("Invalid token type for binary operator");
        }
    }

    AssignmentOperatorKind token_to_assignment_op(TokenType type) {
        switch (type) {
            case TokenType::EQ: return AssignmentOperatorKind::ASSIGN;
            case TokenType::PLUS_EQ: return AssignmentOperatorKind::ADD_ASSIGN;
            case TokenType::MINUS_EQ: return AssignmentOperatorKind::SUBTRACT_ASSIGN;
            case TokenType::STAR_EQ: return AssignmentOperatorKind::MULTIPLY_ASSIGN;
            case TokenType::SLASH_EQ: return AssignmentOperatorKind::DIVIDE_ASSIGN;
            case TokenType::PERCENT_EQ: return AssignmentOperatorKind::MODULO_ASSIGN;
            case TokenType::AMP_EQ: return AssignmentOperatorKind::BITWISE_AND_ASSIGN;
            case TokenType::PIPE_EQ: return AssignmentOperatorKind::BITWISE_OR_ASSIGN;
            case TokenType::CARET_EQ: return AssignmentOperatorKind::BITWISE_XOR_ASSIGN;
            case TokenType::LT_LT_EQ: return AssignmentOperatorKind::SHIFT_LEFT_ASSIGN;
            case TokenType::GT_GT_EQ: return AssignmentOperatorKind::SHIFT_RIGHT_ASSIGN;
            default:
                throw std::runtime_error("Invalid token type for assignment operator");
        }
    }

    UnaryOperatorKind token_to_unary_op(TokenType type) {
        switch (type) {
            case TokenType::PLUS: return UnaryOperatorKind::PLUS;
            case TokenType::MINUS: return UnaryOperatorKind::NEGATE;
            case TokenType::BANG: return UnaryOperatorKind::LOGICAL_NOT;
            case TokenType::TILDE: return UnaryOperatorKind::BITWISE_NOT;
            case TokenType::STAR: return UnaryOperatorKind::DEREFERENCE;
            case TokenType::AMP: return UnaryOperatorKind::ADDRESS_OF;
            default:
                throw std::runtime_error("Invalid token type for unary operator");
        }
    }

    // Parser methods
    ExpressionPtr Parser::parse_expression() { 
        return parse_assignment(); 
    }

    ExpressionPtr Parser::parse_assignment() {
        auto lhs = parse_ternary();

        if (is_assignment_token(peek().type)) {
            SourceLocation l = loc();
            auto op = token_to_assignment_op(advance().type);
            auto rhs = parse_assignment();
            return std::make_unique<AssignmentExpr>(l, op, std::move(lhs), std::move(rhs));
        }
        
        return lhs;
    }

    ExpressionPtr Parser::parse_ternary() {
        auto expr = parse_logical_or();
        SourceLocation l = loc();
        if (accept(TokenType::QUESTION)) {
            auto then_e = parse_expression();
            expect(TokenType::COLON, "Expected ':' in ternary expression");
            auto else_e = parse_expression();
            return std::make_unique<TernaryExpr>(l, std::move(expr), std::move(then_e), std::move(else_e));
        }
        return expr;
    }

    ExpressionPtr Parser::parse_logical_or() {
        auto expr = parse_logical_and();
        while (peek().type == TokenType::OR) {
            SourceLocation l = loc(); 
            advance();
            expr = std::make_unique<BinaryOpExpr>(l, BinaryOperatorKind::LOGICAL_OR, 
                                                std::move(expr), parse_logical_and());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_logical_and() {
        auto expr = parse_bitwise_or();
        while (peek().type == TokenType::AND) {
            SourceLocation l = loc(); 
            advance();
            expr = std::make_unique<BinaryOpExpr>(l, BinaryOperatorKind::LOGICAL_AND, 
                                                std::move(expr), parse_bitwise_or());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_bitwise_or() {
        auto expr = parse_bitwise_xor();
        while (peek().type == TokenType::PIPE) {
            SourceLocation l = loc(); 
            advance();
            expr = std::make_unique<BinaryOpExpr>(l, BinaryOperatorKind::BITWISE_OR, 
                                                std::move(expr), parse_bitwise_xor());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_bitwise_xor() {
        auto expr = parse_bitwise_and();
        while (peek().type == TokenType::CARET) {
            SourceLocation l = loc(); 
            advance();
            expr = std::make_unique<BinaryOpExpr>(l, BinaryOperatorKind::BITWISE_XOR, 
                                                std::move(expr), parse_bitwise_and());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_bitwise_and() {
        auto expr = parse_equality();
        while (peek().type == TokenType::AMP) {
            SourceLocation l = loc(); 
            advance();
            expr = std::make_unique<BinaryOpExpr>(l, BinaryOperatorKind::BITWISE_AND, 
                                                std::move(expr), parse_equality());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_equality() {
        auto expr = parse_comparison();
        while (peek().type == TokenType::EQ_EQ || peek().type == TokenType::BANG_EQ) {
            SourceLocation l = loc();
            auto op = token_to_binary_op(advance().type);
            expr = std::make_unique<BinaryOpExpr>(l, op, std::move(expr), parse_comparison());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_comparison() {
        auto expr = parse_shift();
        while (peek().type == TokenType::LT  || peek().type == TokenType::LTE ||
            peek().type == TokenType::GT  || peek().type == TokenType::GTE) {
            SourceLocation l = loc();
            auto op = token_to_binary_op(advance().type);
            expr = std::make_unique<BinaryOpExpr>(l, op, std::move(expr), parse_shift());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_shift() {
        auto expr = parse_term();
        while (peek().type == TokenType::LT_LT || peek().type == TokenType::GT_GT) {
            SourceLocation l = loc();
            auto op = token_to_binary_op(advance().type);
            expr = std::make_unique<BinaryOpExpr>(l, op, std::move(expr), parse_term());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_term() {
        auto expr = parse_factor();
        while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
            SourceLocation l = loc();
            auto op = token_to_binary_op(advance().type);
            expr = std::make_unique<BinaryOpExpr>(l, op, std::move(expr), parse_factor());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_factor() {
        auto expr = parse_unary();
        while (peek().type == TokenType::STAR   ||
            peek().type == TokenType::SLASH  ||
            peek().type == TokenType::PERCENT) {
            SourceLocation l = loc();
            auto op = token_to_binary_op(advance().type);
            expr = std::make_unique<BinaryOpExpr>(l, op, std::move(expr), parse_unary());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_unary() {
        if (is_unary_token(peek().type)) {
            SourceLocation l = loc();
            auto op = token_to_unary_op(advance().type);
            return std::make_unique<UnaryOpExpr>(l, op, parse_unary());
        }
        
        return parse_postfix();
    }

    // Helper functions
    bool Parser::is_assignment_token(TokenType type) {
        switch (type) {
            case TokenType::EQ:
            case TokenType::PLUS_EQ:   case TokenType::MINUS_EQ:
            case TokenType::STAR_EQ:   case TokenType::SLASH_EQ:
            case TokenType::PERCENT_EQ:
            case TokenType::AMP_EQ:    case TokenType::PIPE_EQ:
            case TokenType::CARET_EQ:
            case TokenType::LT_LT_EQ:  case TokenType::GT_GT_EQ:
                return true;
            default:
                return false;
        }
    }

    bool Parser::is_unary_token(TokenType type) {
        switch (type) {
            case TokenType::MINUS:
            case TokenType::PLUS:
            case TokenType::BANG:
            case TokenType::TILDE:
            case TokenType::STAR:
            case TokenType::AMP:
                return true;
            default:
                return false;
        }
    }

    ExpressionPtr Parser::parse_postfix() {
        auto expr = parse_primary();

        while (true) {
            SourceLocation l = loc();

            if (peek().type == TokenType::DOT) {
                advance();
                SourceLocation member_loc = loc();
                auto member_token = expect(TokenType::IDENTIFIER, "Expected member name after '.'");
                auto member = std::make_unique<Name>(member_loc, member_token.lexeme);
                expr = std::make_unique<MemberAccessExpr>(l, std::move(expr), std::move(member));
            }
            else if (peek().type == TokenType::LPAREN) {
                auto args = parse_arguments();
                // NOTE: no `callee(...)?` early-return syntax yet. Consuming a '?' here would
                // swallow the '?' of a ternary such as `ready() ? a : b`.
                expr = std::make_unique<CallExpr>(l, std::move(expr), std::move(args));
            }
            else if (peek().type == TokenType::LBRACKET) {
                advance();
                FlagGuard guard(no_struct_literal_, false);
                auto idx = parse_expression();
                expect(TokenType::RBRACKET, "Expected ']' after index");
                expr = std::make_unique<IndexExpr>(l, std::move(expr), std::move(idx));
            }
            else {
                break;
            }
        }

        return expr;
    }

    ExpressionPtr Parser::parse_primary() {
        SourceLocation l = loc();

        switch (peek().type) {
            case TokenType::INT_LITERAL: {
                auto [value, suffix] = split_int_suffix(advance().lexeme);
                return std::make_unique<LiteralInt>(l, std::move(value), parse_integer_suffix(suffix));
            }

            case TokenType::FLOAT_LITERAL: {
                auto [value, suffix] = split_float_suffix(advance().lexeme);
                return std::make_unique<LiteralFloat>(l, std::move(value), parse_float_suffix(suffix));
            }

            case TokenType::STRING_LITERAL:
                return std::make_unique<LiteralString>(l, advance().lexeme);

            case TokenType::CHARACTER_LITERAL:
                return std::make_unique<LiteralChar>(l, advance().lexeme);

            case TokenType::TRUE:
                advance();
                return std::make_unique<LiteralBool>(l, true);

            case TokenType::FALSE:
                advance();
                return std::make_unique<LiteralBool>(l, false);

            case TokenType::NULLPTR:
                advance();
                return std::make_unique<LiteralNullptr>(l);

            case TokenType::LBRACKET:
                return parse_array_literal();

            case TokenType::COLON_COLON:
            case TokenType::IDENTIFIER: {
                NamePtr name = parse_name();
                // `Name { ... }` is a struct literal, except where a '{' opens a body.
                if (check(TokenType::LBRACE) && !no_struct_literal_) {
                    auto type_expr = std::make_unique<NamedTypeExpr>(l, std::move(name));
                    return parse_struct_literal(l, std::move(type_expr));
                }
                return name;
            }

            case TokenType::LPAREN: {
                advance();
                FlagGuard guard(no_struct_literal_, false);
                auto expr = parse_expression();
                expect(TokenType::RPAREN, "Expected ')' to close parenthesized expression");
                return expr;
            }
            default:
                throw CompilerException(
                    std::format("Unexpected {} in expression", describe_token(peek())),
                    peek().location, Severity::ERROR);
        }
    }

    // At '[': is the matching ']' immediately followed by '{'?  That is what
    // separates `[T; N] { ... }` / `[T] { ... }` from a plain `[a, b, c]`.
    bool Parser::typed_array_literal_ahead() const {
        size_t depth = 0;
        for (size_t i = current_; i < tokens_.size(); ++i) {
            switch (tokens_[i].type) {
                case TokenType::LBRACKET:
                    ++depth;
                    break;
                case TokenType::RBRACKET:
                    if (--depth == 0) {
                        return i + 1 < tokens_.size() && tokens_[i + 1].type == TokenType::LBRACE;
                    }
                    break;
                case TokenType::EOF_TOKEN:
                    return false;
                default:
                    break;
            }
        }
        return false;
    }

    // { a, b, c }
    std::vector<ExpressionPtr> Parser::parse_braced_expressions(const char* what) {
        expect(TokenType::LBRACE, std::format("Expected '{{' to start {}", what));
        FlagGuard guard(no_struct_literal_, false);

        std::vector<ExpressionPtr> elems;
        if (!check(TokenType::RBRACE)) {
            do {
                elems.push_back(parse_expression());
            } while (accept(TokenType::COMMA));
        }
        expect(TokenType::RBRACE, std::format("Expected '}}' to close {}", what));
        return elems;
    }

    //   [i32; 4] { 4, 9, 16, 25 }   fixed, explicit element type and length
    //            [ 4, 9, 16, 25 ]   fixed, inferred element type, length from count
    ExpressionPtr Parser::parse_array_literal() {
        SourceLocation l = loc();

        if (!no_struct_literal_ && typed_array_literal_ahead()) {
            TypeExprPtr type = parse_type_expression();   // starts with '[', so always an ArrayTypeExpr
            auto* array_type = static_cast<ArrayTypeExpr*>(type.get());
            auto elems = parse_braced_expressions("array literal");
            return std::make_unique<LiteralArray>(l, std::move(elems),
                                                  std::move(array_type->element_type),
                                                  std::move(array_type->size_expr));
        }

        expect(TokenType::LBRACKET, "Expected '['");
        FlagGuard guard(no_struct_literal_, false);

        std::vector<ExpressionPtr> elems;
        if (!check(TokenType::RBRACKET)) {
            do {
                elems.push_back(parse_expression());
            } while (accept(TokenType::COMMA));
        }
        expect(TokenType::RBRACKET, "Expected ']' to close array literal");
        return std::make_unique<LiteralArray>(l, std::move(elems));
    }

    // Type { a, b }          positional
    // Type { x: a, y: b }    named
    // A literal is one style or the other; mixing them is an error.
    ExpressionPtr Parser::parse_struct_literal(SourceLocation l, TypeExprPtr type_expr) {
        expect(TokenType::LBRACE, "Expected '{' to start struct literal");
        FlagGuard guard(no_struct_literal_, false);

        std::vector<FieldInitializer> fields;
        bool is_named = false;

        if (!check(TokenType::RBRACE)) {
            do {
                FieldInitializer field;
                field.location = loc();

                // `name :` can only start a named entry. (`a::b` is a single COLON_COLON token.)
                const bool entry_named = check(TokenType::IDENTIFIER) &&
                                         peek_next().type == TokenType::COLON;

                if (fields.empty()) {
                    is_named = entry_named;
                } else if (entry_named != is_named) {
                    throw CompilerException(
                        is_named
                            ? "Cannot mix named and positional fields in a struct literal (expected 'name: value')"
                            : "Cannot mix positional and named fields in a struct literal",
                        field.location, Severity::ERROR);
                }

                if (entry_named) {
                    field.name = advance().lexeme;
                    advance();  // ':'
                }
                field.value = parse_expression();
                fields.push_back(std::move(field));
            } while (accept(TokenType::COMMA));
        }

        expect(TokenType::RBRACE, "Expected '}' to close struct literal");
        return std::make_unique<LiteralStruct>(l, std::move(type_expr), std::move(fields), is_named);
    }

    DeclarationPtr Parser::parse_declaration() {
        Metadata meta = parse_metadata();
        const bool is_public = match(TokenType::PUB);

        DeclarationPtr decl;
        if (check(TokenType::LET)) {
            decl = parse_variable_declaration(is_public);
        }
        else if (check(TokenType::FUNC)) {
            decl = parse_function_declaration(is_public);
        }
        else if (check(TokenType::TYPE)) {
            decl = parse_struct_declaration(is_public);
        }
        else if (check(TokenType::IMPL)) {
            if (is_public) {
                throw CompilerException("'impl' blocks cannot be 'pub'; mark the members 'pub' instead",
                                        previous().location, Severity::ERROR);
            }
            if (meta.has_doc) {
                throw CompilerException("Doc comments are not allowed on 'impl' blocks; document the methods instead",
                                        meta.doc_location, Severity::ERROR);
            }
            decl = parse_impl_declaration();
        }
        else {
            if (!is_public) check_dangling_metadata(meta);
            throw CompilerException(is_public ? "Expected declaration after 'pub'" : "Expected declaration",
                                    peek().location, Severity::ERROR);
        }

        attach_metadata(*decl, std::move(meta));
        return decl;
    }

    StatementPtr Parser::parse_statement() {
        SourceLocation l = loc();

        // Doc comments and attributes belong to declarations, not to statements
        // (including `let` bindings inside a body).
        if (check(TokenType::DOC_COMMENT)) {
            throw CompilerException("Doc comments cannot be attached to statements or local bindings",
                                    l, Severity::ERROR);
        }
        if (check(TokenType::HASH)) {
            throw CompilerException("Attributes cannot be attached to statements or local bindings",
                                    l, Severity::ERROR);
        }
        if (check(TokenType::MODULE_DOC_COMMENT)) {
            throw CompilerException("'//!' comments are only allowed at the very top of a file",
                                    l, Severity::ERROR);
        }

        if (check(TokenType::LET)) {
            return parse_variable_declaration(false);
        }
        if (check(TokenType::IF)) {
            return parse_if_statement();
        }
        if (check(TokenType::WHILE)) {
            return parse_while_statement();
        }

        if (match(TokenType::RETURN)) {
            ExpressionPtr ret_expr = nullptr;
            if (!check(TokenType::SEMICOLON)) {
                ret_expr = parse_expression();
            }
            expect(TokenType::SEMICOLON, "Expected ';' after return statement");
            return std::make_unique<ReturnStmt>(l, std::move(ret_expr));
        }

        if (match(TokenType::BREAK)) {
            expect(TokenType::SEMICOLON, "Expected ';' after break statement");
            return std::make_unique<BreakStmt>(l);
        }

        if (match(TokenType::CONTINUE)) {
            expect(TokenType::SEMICOLON, "Expected ';' after continue statement");
            return std::make_unique<ContinueStmt>(l);
        }

        // Fallback to expression statement
        auto expr = parse_expression();
        expect(TokenType::SEMICOLON, "Expected ';' after expression statement");
        return std::make_unique<ExpressionStmt>(l, std::move(expr));
    }

    BlockPtr Parser::parse_block() {
        SourceLocation l = loc();
        expect(TokenType::LBRACE, "Expected '{' to start block");
        std::vector<StatementPtr> statements;
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            statements.push_back(parse_statement());
        }
        expect(TokenType::RBRACE, "Expected '}' to close block");
        return std::make_unique<BlockStmt>(l, std::move(statements));
    }

    // if <condition> { ... } [else if <condition> { ... } | else { ... }]?
    StatementPtr Parser::parse_if_statement() {
        SourceLocation l = loc();
        expect(TokenType::IF, "Expected 'if' keyword");
        ExpressionPtr condition;
        {
            FlagGuard guard(no_struct_literal_, true);   // the '{' after the condition opens the body
            condition = parse_expression();
        }
        auto then_branch = parse_block();
        StatementPtr else_branch = nullptr;
        if (match(TokenType::ELSE)) {
            if (check(TokenType::IF)) {
                else_branch = parse_if_statement();
            } else {
                else_branch = parse_block();
            }
        }
        return std::make_unique<IfStmt>(l, std::move(condition), std::move(then_branch), std::move(else_branch));
    }

    // while <condition> { ... }
    StatementPtr Parser::parse_while_statement() {
        SourceLocation l = loc();
        expect(TokenType::WHILE, "Expected 'while' keyword");
        ExpressionPtr condition;
        {
            FlagGuard guard(no_struct_literal_, true);   // the '{' after the condition opens the body
            condition = parse_expression();
        }
        auto body = parse_block();
        return std::make_unique<WhileStmt>(l, std::move(condition), std::move(body));
    }

    /* == WIP ==
    // foreach <var_name> in <iterable_expr> { ... }
    StatementPtr Parser::parse_foreach_statement() {
        SourceLocation l = loc();
        expect(TokenType::FOREACH, "Expected 'foreach' keyword");
        auto var_name = parse_name();
        expect(TokenType::IN, "Expected 'in' keyword in foreach statement");
        auto iterable_expr = parse_expression();
        auto body = parse_block();
        return std::make_unique<ForeachStmt>(l, std::move(var_name), std::move(iterable_expr), std::move(body));
    }
    */

    // let [mut] <name> [: <type_expr>] [= <init_expr>];
    VariableDeclPtr Parser::parse_variable_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::LET, "Expected 'let' keyword");
        bool is_mutable = accept(TokenType::MUT);
        auto name = expect(TokenType::IDENTIFIER, std::format("Expected variable name after {}", is_mutable ? "'mut'" : "'let'")).lexeme;
        TypeExprPtr type_expr = nullptr;
        ExpressionPtr init_expr = nullptr;

        if (accept(TokenType::COLON)) {
            type_expr = parse_type_expression();
        }

        if (accept(TokenType::EQ)) {
            init_expr = parse_expression();
        }

        expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
        return std::make_unique<VariableDecl>(l, std::move(name), std::move(type_expr), std::move(init_expr), is_mutable, is_public);
    }

    // func <name>(params...) [ -> ret_t ]? { ... }
    FunctionDeclPtr Parser::parse_function_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::FUNC, "Expected 'func' keyword");
        auto name = expect(TokenType::IDENTIFIER, "Expected function name after 'func'").lexeme;
        auto params = parse_parameters();
        auto return_type = parse_return_type();
        auto body = parse_optional_body();
        return std::make_unique<FunctionDecl>(l, std::move(name), std::move(params), std::move(return_type), std::move(body), is_public);
    }

    // The symbol after `operator`: a single overloadable operator token, or `[]` / `()`.
    std::string Parser::parse_operator_symbol() {
        if (check(TokenType::LBRACKET)) {
            advance();
            expect(TokenType::RBRACKET, "Expected ']' in 'operator[]'");
            return "[]";
        }
        if (check(TokenType::LPAREN) && peek_next().type == TokenType::RPAREN) {
            advance();
            advance();
            return "()";
        }
        if (is_overloadable_operator(peek().type)) {
            return advance().lexeme;
        }
        throw CompilerException(
            std::format("Expected an overloadable operator after 'operator', got {}", describe_token(peek())),
            peek().location, Severity::ERROR);
    }

    // operator<op>(params...) -> ret_t { ... }
    OperatorOverloadDeclPtr Parser::parse_operator_overload_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::OPERATOR, "Expected 'operator' keyword");
        std::string op_lexeme = parse_operator_symbol();
        auto params = parse_parameters();
        if (params.size() == 0) throw CompilerException("Too little arguments in operator overload", l, Severity::ERROR);
        if (params.size() > 2) throw CompilerException("Too many arguments in operator overload", l, Severity::ERROR);

        auto return_type = parse_return_type();
        auto body = parse_optional_body();

        return std::make_unique<OperatorOverloadDecl>(l, std::move(op_lexeme), std::move(params), std::move(return_type), std::move(body), is_public);
    }

    // <name>: <type_expr>;
    StructFieldDeclPtr Parser::parse_struct_field_declaration(bool is_public) {
        SourceLocation l = loc();
        auto name = expect(TokenType::IDENTIFIER, "Expected struct field name").lexeme;

        expect(TokenType::COLON, "Expected ':' and a type after struct field name");
        TypeExprPtr type_expr = parse_type_expression();
        expect(TokenType::SEMICOLON, "Expected ';' after struct field declaration");
        return std::make_unique<StructFieldDecl>(l, std::move(name), std::move(type_expr), is_public);
    }


    MethodDeclPtr Parser::parse_method_declaration(bool is_public, bool is_static) {
        SourceLocation l = loc();
        expect(TokenType::FUNC, "Expected 'func' keyword");
        auto name = expect(TokenType::IDENTIFIER, "Expected method name after 'func'").lexeme;
        auto params = parse_parameters();
        auto return_type = parse_return_type();
        auto body = parse_optional_body();
        return std::make_unique<MethodDecl>(l, std::move(name), std::move(params), std::move(return_type), std::move(body), is_public, is_static);
    }

    // type <name> { [pub] field: T; ... }
    StructDeclPtr Parser::parse_struct_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::TYPE, "Expected 'type' keyword");
        auto name = expect(TokenType::IDENTIFIER, "Expected type name after 'type'").lexeme;

        expect(TokenType::LBRACE, "Expected '{' to start type body");

        std::vector<StructFieldDeclPtr> fields;

        while (!check(TokenType::RBRACE) && !is_at_end()) {
            Metadata meta = parse_metadata();
            if (check(TokenType::RBRACE) || is_at_end()) {
                check_dangling_metadata(meta);
                break;
            }

            bool field_public = match(TokenType::PUB);

            if (!check(TokenType::IDENTIFIER)) {
                throw CompilerException("Expected struct field declaration", peek().location, Severity::ERROR);
            }

            auto field = parse_struct_field_declaration(field_public);
            attach_metadata(*field, std::move(meta));
            fields.push_back(std::move(field));
        }

        expect(TokenType::RBRACE, "Expected '}' to close type declaration");
        return std::make_unique<StructDecl>(l, std::move(name), std::move(fields), is_public);
    }

    ImplDeclPtr Parser::parse_impl_declaration() {
        SourceLocation l = loc();
        expect(TokenType::IMPL, "Expected 'impl' keyword");
        auto type_name = parse_name();

        expect(TokenType::LBRACE, "Expected '{' to start impl block");

        std::vector<VariableDeclPtr> static_vars;
        std::vector<MethodDeclPtr> methods;
        std::vector<OperatorOverloadDeclPtr> operator_overloads;

        while (!check(TokenType::RBRACE) && !is_at_end()) {
            Metadata meta = parse_metadata();
            if (check(TokenType::RBRACE) || is_at_end()) {
                check_dangling_metadata(meta);
                break;
            }

            bool member_public = match(TokenType::PUB);

            if (check(TokenType::OPERATOR)) {
                auto op = parse_operator_overload_declaration(member_public);
                attach_metadata(*op, std::move(meta));
                operator_overloads.push_back(std::move(op));
                continue;
            }

            bool is_static = match(TokenType::STATIC);

            if (check(TokenType::LET)) {
                if (!is_static) {
                    throw CompilerException(
                        "Expected 'static' before variable declaration in impl block",
                        peek().location, Severity::ERROR);
                }
                auto var = parse_variable_declaration(member_public);
                attach_metadata(*var, std::move(meta));
                static_vars.push_back(std::move(var));
                continue;
            }

            if (check(TokenType::FUNC)) {
                auto method = parse_method_declaration(member_public, is_static);
                attach_metadata(*method, std::move(meta));
                methods.push_back(std::move(method));
                continue;
            }

            throw CompilerException("Expected impl member declaration", peek().location, Severity::ERROR);
        }

        expect(TokenType::RBRACE, "Expected '}' to close impl block");
        return std::make_unique<ImplDecl>(
            l, std::move(type_name), std::move(static_vars), std::move(methods),
            std::move(operator_overloads));
    }


    std::string Parser::parse_inner_doc() {
        std::string doc;
        bool first = true;
        while (check(TokenType::MODULE_DOC_COMMENT)) {
            if (!first) doc += '\n';
            first = false;
            doc += advance().lexeme;
        }
        return doc;
    }

    ModuleName Parser::parse_module_name() {
        expect(TokenType::MODULE, "Expected 'module' keyword");
        SourceLocation l = loc();
        auto name = parse_name();
        ModuleName module_name = ModuleName::from_name(name.get());
        module_name.location = l;
        expect(TokenType::SEMICOLON, "Expected ';' after module name declaration");
        return module_name;
    }

    ModuleName Parser::parse_import() {
        expect(TokenType::IMPORT, "Expected 'import' keyword");
        SourceLocation l = loc();
        auto name = parse_name();
        ModuleName dependency = ModuleName::from_name(name.get());
        dependency.location = l;
        expect(TokenType::SEMICOLON, "Expected ';' after import declaration");
        return dependency;
    }

    // Header: optional `//!` module docs, then any number of `module` / `import`
    // declarations (each may carry a `///` doc). Anything else ends the header and
    // hands whatever doc/attributes were already read to the first declaration.
    void Parser::parse_header(ModuleAST& ast) {
        ast.doc = parse_inner_doc();
        bool seen_module = false;

        while (!is_at_end()) {
            Metadata meta = parse_metadata();

            if (check(TokenType::MODULE) || check(TokenType::IMPORT)) {
                if (!meta.attributes.empty()) {
                    throw CompilerException(
                        check(TokenType::MODULE) ? "Attributes are not supported on 'module'"
                                                 : "Attributes are not supported on 'import'",
                        meta.attribute_location, Severity::ERROR);
                }
            }

            if (check(TokenType::MODULE)) {
                if (seen_module) {
                    throw CompilerException("Duplicate 'module' declaration", peek().location, Severity::ERROR);
                }
                seen_module = true;
                ast.module_name = parse_module_name();
                if (meta.has_doc) {
                    if (!ast.doc.empty()) ast.doc += '\n';
                    ast.doc += meta.doc;
                }
            } else if (check(TokenType::IMPORT)) {
                ModuleName dependency = parse_import();
                dependency.doc = std::move(meta.doc);
                ast.dependencies.push_back(std::move(dependency));
            } else {
                pending_meta_ = std::move(meta);
                break;  // End of header section
            }
        }
    }
}