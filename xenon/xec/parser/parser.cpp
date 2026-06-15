#include "parser.h"
#include "tokens/tokens.h"
#include <format>

namespace xenon {

// ============================================================================
// NAVIGATION
// ============================================================================

const Token& Parser::peek() const {
    if (split_gt_pending_) return split_gt_token_;
    return (idx_ < tokens_.size()) ? tokens_[idx_] : tokens_.back();
}

const Token& Parser::peek_next() const {
    if (split_gt_pending_)
        return (idx_ < tokens_.size()) ? tokens_[idx_] : tokens_.back();
    size_t next = idx_ + 1;
    return (next < tokens_.size()) ? tokens_[next] : tokens_.back();
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::EOF_TOKEN;
}

const Token& Parser::previous() const {
    size_t real_idx = split_gt_pending_ ? idx_ : (idx_ > 0 ? idx_ - 1 : 0);
    return tokens_[real_idx];
}

Token Parser::advance() {
    if (split_gt_pending_) {
        split_gt_pending_ = false;
        return split_gt_token_;
    }
    Token t = peek();
    if (!is_at_end()) ++idx_;
    return t;
}

Token Parser::expect(TokenType type, const char* msg) {
    if (accept(type)) {
        return previous();
    }
    
    std::string error_msg = msg;
    error_msg += std::format(" (expected '{}', got '{}')",
                             token_type_to_string(type), peek().lexeme);
    
    throw CompilerException(error_msg, peek().location);
}

bool Parser::accept(TokenType type) {
    if (peek().type == type) { advance(); return true; }
    return false;
}

SourceLocation Parser::get_location() const {
    return peek().location;
}

// ============================================================================
// ERROR RECOVERY
// ============================================================================

void Parser::report_and_synchronise(const CompilerException& e) {
    g_diagnostics.syntax_error(e.what(), e.location);
    had_errors_ = true;
    
    if (++error_count_ >= MAX_ERRORS) {
        g_diagnostics.note(
            std::format("Too many errors ({}), stopping compilation", MAX_ERRORS),
            get_location());
        throw std::runtime_error("Error limit reached");
    }
    
    synchronise();
}

void Parser::report_and_synchronise(const CompilerException& e, TokenType stop_delim) {
    g_diagnostics.syntax_error(e.what(), e.location);
    had_errors_ = true;
    
    if (++error_count_ >= MAX_ERRORS) {
        g_diagnostics.note(
            std::format("Too many errors ({}), stopping compilation", MAX_ERRORS),
            get_location());
        throw std::runtime_error("Error limit reached");
    }
    
    synchronise(stop_delim);
}

void Parser::synchronise(TokenType stop_delim) {
    advance();

    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) {
            return;
        }

        // Never consume the stop delimiter
        if (peek().type == stop_delim) {
            return;
        }

        // Never consume delimiters that would close enclosing scopes
        for (const auto& delim : expected_close_delims_) {
            if (peek().type == delim) {
                return;
            }
        }

        switch (peek().type) {
            // Declaration keywords
            case TokenType::FUNC:
            case TokenType::CLASS:
            case TokenType::TRAIT:
            case TokenType::ENUM:
            case TokenType::IMPORT:
            case TokenType::EXPORT:
                return;

            // Statement-level keywords
            case TokenType::LET:
            case TokenType::VAR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOREACH:
            case TokenType::MATCH:
            case TokenType::RETURN:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::THROW:
            case TokenType::TRY:
            case TokenType::DO:
                return;

            // Closing delimiters - consume and return
            case TokenType::RBRACE:
            case TokenType::RPAREN:
            case TokenType::RBRACKET:
                advance();
                return;

            // Statement terminators
            case TokenType::SEMICOLON:
                advance();
                return;

            default:
                advance();
        }
    }
}

void Parser::synchronise() {
    synchronise(TokenType::EOF_TOKEN);
}

void Parser::recover_to_next_statement() {
    while (!is_at_end()) {
        switch (peek().type) {
            case TokenType::SEMICOLON:
                advance();
                return;
                
            // Don't consume closing delimiters
            case TokenType::RBRACE:
            case TokenType::RPAREN:
            case TokenType::RBRACKET:
                return;
                
            case TokenType::FUNC:
            case TokenType::CLASS:
            case TokenType::TRAIT:
            case TokenType::ENUM:
            case TokenType::LET:
            case TokenType::VAR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOREACH:
            case TokenType::MATCH:
            case TokenType::RETURN:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::IMPORT:
            case TokenType::EXPORT:
                return;
                
            default:
                advance();
        }
    }
}

bool Parser::expect_semicolon(const char* context_msg) {
    if (accept(TokenType::SEMICOLON)) return true;
    
    g_diagnostics.syntax_error(
        std::format("Expected ';' after {}", context_msg), get_location());
    had_errors_ = true;
    recover_to_next_statement();
    return false;
}

// ============================================================================
// SCOPE TRACKING
// ============================================================================

void Parser::enter_scope(TokenType close_delim) {
    expected_close_delims_.push_back(close_delim);
}

void Parser::leave_scope() {
    if (!expected_close_delims_.empty()) {
        expected_close_delims_.pop_back();
    }
}

// ============================================================================
// >> DISAMBIGUATION
// ============================================================================

void Parser::consume_gt_gt_as_gt() {
    Token real = advance();
    const_cast<Token&>(split_gt_token_) = Token{ TokenType::GT, ">", real.location };
    split_gt_pending_ = true;
}

void Parser::close_angle(const char* msg) {
    if (peek().type == TokenType::GT) { advance(); }
    else if (peek().type == TokenType::GT_GT) { consume_gt_gt_as_gt(); }
    else { 
        throw CompilerException(
            std::format("{} — got '{}'", msg, peek().lexeme),
            peek().location);
    }
}

// ============================================================================
// DIRECTIVES
// ============================================================================

Directives Parser::parse_directives() {
    Directives dirs;
    while (true) {
        switch (peek().type) {
            case TokenType::ATTR_INLINE:
                advance();
                dirs.push_back(Directive("inline", {}));
                break;
            case TokenType::ATTR_CONSTEXPR:
                advance();
                dirs.push_back(Directive("constexpr", {}));
                break;
            case TokenType::ATTR_DEPRECATED: {
                advance();
                std::vector<ExpressionPtr> args;
                if (accept(TokenType::LPAREN)) {
                    auto msg = expect(TokenType::STRING_LITERAL, "Expected deprecation message");
                    args.push_back(std::make_unique<LiteralString>(msg.location, msg.lexeme));
                    expect(TokenType::RPAREN, "Expected ')' after deprecation message");
                }
                dirs.push_back(Directive("deprecated", std::move(args)));
                break;
            }
            case TokenType::ATTR_DOCSTRING: {
                advance();
                std::vector<ExpressionPtr> args;
                if (accept(TokenType::LPAREN)) {
                    auto doc = expect(TokenType::STRING_LITERAL, "Expected docstring content");
                    args.push_back(std::make_unique<LiteralString>(doc.location, doc.lexeme));
                    expect(TokenType::RPAREN, "Expected ')' after docstring");
                }
                dirs.push_back(Directive("docstring", std::move(args)));
                break;
            }
            default:
                return dirs;
        }
    }
}

// ============================================================================
// OPERATOR HELPERS
// ============================================================================

BinaryOp Parser::token_to_binary_op(TokenType t) const {
    switch (t) {
        case TokenType::PLUS:       return BinaryOp::ADD;
        case TokenType::MINUS:      return BinaryOp::SUBTRACT;
        case TokenType::STAR:       return BinaryOp::MULTIPLY;
        case TokenType::SLASH:      return BinaryOp::DIVIDE;
        case TokenType::PERCENT:    return BinaryOp::MODULO;
        case TokenType::EQ_EQ:      return BinaryOp::EQUAL;
        case TokenType::BANG_EQ:    return BinaryOp::NOT_EQUAL;
        case TokenType::LT:         return BinaryOp::LESS;
        case TokenType::LTE:        return BinaryOp::LESS_EQUAL;
        case TokenType::GT:         return BinaryOp::GREATER;
        case TokenType::GTE:        return BinaryOp::GREATER_EQUAL;
        case TokenType::AND:        return BinaryOp::LOGICAL_AND;
        case TokenType::OR:         return BinaryOp::LOGICAL_OR;
        case TokenType::AMP:        return BinaryOp::BITWISE_AND;
        case TokenType::PIPE:       return BinaryOp::BITWISE_OR;
        case TokenType::CARET:      return BinaryOp::BITWISE_XOR;
        case TokenType::LT_LT:      return BinaryOp::LEFT_SHIFT;
        case TokenType::GT_GT:      return BinaryOp::RIGHT_SHIFT;
        case TokenType::EQ:         return BinaryOp::ASSIGN;
        case TokenType::PLUS_EQ:    return BinaryOp::ADD_ASSIGN;
        case TokenType::MINUS_EQ:   return BinaryOp::SUBTRACT_ASSIGN;
        case TokenType::STAR_EQ:    return BinaryOp::MULTIPLY_ASSIGN;
        case TokenType::SLASH_EQ:   return BinaryOp::DIVIDE_ASSIGN;
        case TokenType::PERCENT_EQ: return BinaryOp::MODULO_ASSIGN;
        case TokenType::AMP_EQ:     return BinaryOp::BITWISE_AND_ASSIGN;
        case TokenType::PIPE_EQ:    return BinaryOp::BITWISE_OR_ASSIGN;
        case TokenType::CARET_EQ:   return BinaryOp::BITWISE_XOR_ASSIGN;
        case TokenType::TILDE_EQ:   return BinaryOp::BITWISE_XOR_ASSIGN;
        case TokenType::LT_LT_EQ:   return BinaryOp::LEFT_SHIFT_ASSIGN;
        case TokenType::GT_GT_EQ:   return BinaryOp::RIGHT_SHIFT_ASSIGN;
        default:
            throw CompilerException(
                std::format("'{}' is not a binary operator", peek().lexeme),
                peek().location);
    }
}

UnaryOp Parser::token_to_unary_op(TokenType t) const {
    switch (t) {
        case TokenType::PLUS:   return UnaryOp::UNARY_PLUS;
        case TokenType::MINUS:  return UnaryOp::UNARY_MINUS;
        case TokenType::BANG:   return UnaryOp::LOGICAL_NOT;
        case TokenType::TILDE:  return UnaryOp::BITWISE_NOT;
        case TokenType::STAR:   return UnaryOp::DEREFERENCE;
        case TokenType::AMP:    return UnaryOp::ADDRESS_OF;
        default:
            throw CompilerException(
                std::format("'{}' is not a unary operator", peek().lexeme),
                peek().location);
    }
}

/*
OverloadableOp Parser::token_to_overloadable_op(TokenType t, bool is_unary) const {
    if (is_unary) {
        switch (t) {
            case TokenType::PLUS:  return OverloadableOp::UNARY_PLUS;
            case TokenType::MINUS: return OverloadableOp::UNARY_MINUS;
            case TokenType::TILDE: return OverloadableOp::BITWISE_NOT;
            default: break;
        }
    } else {
        switch (t) {
            case TokenType::PLUS:     return OverloadableOp::ADD;
            case TokenType::MINUS:    return OverloadableOp::SUBTRACT;
            case TokenType::STAR:     return OverloadableOp::MULTIPLY;
            case TokenType::SLASH:    return OverloadableOp::DIVIDE;
            case TokenType::PERCENT:  return OverloadableOp::MODULO;
            case TokenType::EQ_EQ:    return OverloadableOp::EQUAL;
            case TokenType::BANG_EQ:  return OverloadableOp::NOT_EQUAL;
            case TokenType::LT:       return OverloadableOp::LESS;
            case TokenType::GT:       return OverloadableOp::GREATER;
            case TokenType::LTE:      return OverloadableOp::LESS_EQUAL;
            case TokenType::GTE:      return OverloadableOp::GREATER_EQUAL;
            case TokenType::PLUS_EQ:  return OverloadableOp::ADD_ASSIGN;
            case TokenType::MINUS_EQ: return OverloadableOp::SUBTRACT_ASSIGN;
            case TokenType::STAR_EQ:  return OverloadableOp::MULTIPLY_ASSIGN;
            case TokenType::SLASH_EQ: return OverloadableOp::DIVIDE_ASSIGN;
            case TokenType::LBRACKET: return OverloadableOp::INDEX;
            case TokenType::AND:      return OverloadableOp::LOGICAL_AND;
            case TokenType::OR:       return OverloadableOp::LOGICAL_OR;
            default: break;
        }
    }
    throw CompilerException(
        std::format("'{}' is not a valid overloadable operator", peek().lexeme),
        peek().location);
}
*/

// ============================================================================
// TYPE PARSING
// ============================================================================

TypePtr Parser::parse_type() {
    SourceLocation l = get_location();

    if (accept(TokenType::MUT)) {
        if (accept(TokenType::REF)) {
            return std::make_unique<ReferenceType>(l, parse_type(), true);
        } else if (accept(TokenType::PTR)) {
            return std::make_unique<RawPointerType>(l, parse_type(), true);
        } else if (accept(TokenType::BOX)) {
            return std::make_unique<BoxPointerType>(l, parse_type(), true);
        } else {
            throw CompilerException(
                "Expected 'ref', 'ptr', or 'box' after 'mut'", get_location());
        }
    }

    if (accept(TokenType::REF)) {
        return std::make_unique<ReferenceType>(l, parse_type(), false);
    }

    if (accept(TokenType::PTR)) {
        return std::make_unique<RawPointerType>(l, parse_type(), false);
    }

    if (accept(TokenType::BOX)) {
        return std::make_unique<BoxPointerType>(l, parse_type(), false);
    }

    if (accept(TokenType::FUNC)) {
        expect(TokenType::LPAREN, "Expected '(' after 'func' in function type");
        std::vector<TypePtr> param_types;
        if (peek().type != TokenType::RPAREN) {
            do { param_types.push_back(parse_type()); } while (accept(TokenType::COMMA));
        }
        expect(TokenType::RPAREN, "Expected ')' after function parameter types");
        expect(TokenType::ARROW, "Expected '->' after function parameter list");
        TypePtr return_type = parse_type();
        return std::make_unique<CallableType>(l, std::move(return_type), std::move(param_types));
    }

    if (accept(TokenType::LPAREN)) {
        std::vector<TypePtr> param_types;
        if (peek().type != TokenType::RPAREN) {
            do { param_types.push_back(parse_type()); } while (accept(TokenType::COMMA));
        }
        expect(TokenType::RPAREN, "Expected ')' after function parameter types");
        expect(TokenType::ARROW, "Expected '->' after function parameter list");
        TypePtr return_type = parse_type();
        return std::make_unique<CallableType>(l, std::move(return_type), std::move(param_types));
    }

    if (accept(TokenType::LBRACKET)) {
        TypePtr element_type = parse_type();
        if (accept(TokenType::SEMICOLON)) {
            ExpressionPtr array_size = parse_expression();
            expect(TokenType::RBRACKET, "Expected ']' after static array size");
            return std::make_unique<StaticArrayType>(l, std::move(element_type), std::move(array_size));
        }
        expect(TokenType::RBRACKET, "Expected ']' after dynamic array element type");
        return std::make_unique<DynamicArrayType>(l, std::move(element_type));
    }

    auto name = parse_name();
    return std::make_unique<ValueType>(l, std::move(name));
}

std::vector<TypePtr> Parser::parse_type_args() {
    std::vector<TypePtr> args;
    if (peek().type != TokenType::GT && peek().type != TokenType::GT_GT) {
        do { args.push_back(parse_type()); } while (accept(TokenType::COMMA));
    }
    close_angle("Expected '>' to close generic type arguments");
    return args;
}

// ============================================================================
// GENERIC PARAMETER/ARGUMENT PARSING
// ============================================================================

std::vector<TraitConstraint> Parser::parse_trait_bounds() {
    std::vector<TraitConstraint> bounds;
    do {
        std::string tname = std::string(expect(TokenType::IDENTIFIER, "Expected trait name").lexeme);
        while (accept(TokenType::COLON_COLON))
            tname += std::format("::{}", expect(TokenType::IDENTIFIER, "Expected identifier after '::'").lexeme);
        GenericArguments targs;
        if (peek().type == TokenType::LT) {
            advance();
            targs = parse_generic_arguments();
        }
        bounds.emplace_back(std::move(tname), std::move(targs));
    } while (accept(TokenType::PLUS));
    return bounds;
}

GenericParameters Parser::parse_generic_params() {
    GenericParameters params;
    bool seen_default = false;

    do {
        std::string pname = std::string(expect(TokenType::IDENTIFIER, "Expected generic parameter name").lexeme);
        std::vector<TraitConstraint> bounds;
        std::string default_type;

        if (accept(TokenType::COLON)) {
            bounds = parse_trait_bounds();
        }

        if (accept(TokenType::EQ)) {
            seen_default = true;
            default_type = expect(TokenType::IDENTIFIER, "Expected default type").lexeme;
        } else if (seen_default) {
            throw CompilerException(
                "Default type precedes non-default parameter", get_location());
        }

        params.params.emplace_back(std::move(pname), std::move(bounds));
    } while (accept(TokenType::COMMA));

    close_angle("Expected '>' after generic parameters");
    return params;
}

GenericArguments Parser::parse_generic_arguments() {
    GenericArguments args;
    if (peek().type != TokenType::GT && peek().type != TokenType::GT_GT) {
        do {
            // Try to parse as type first, fall back to expression
            if (peek().type == TokenType::IDENTIFIER || 
                peek().type == TokenType::REF ||
                peek().type == TokenType::PTR) {
                args.params.push_back(parse_type());
            } else {
                args.params.push_back(parse_expression());
            }
        } while (accept(TokenType::COMMA));
    }
    close_angle("Expected '>' to close generic arguments");
    return args;
}

// ============================================================================
// PARAMETER PARSING
// ============================================================================

Parameters Parser::parse_parameters() {
    Parameters params;
    expect(TokenType::LPAREN, "Expected '('");
    
    bool seen_default = false;
    if (peek().type != TokenType::RPAREN) {
        do {
            Param param;
            param.name = expect(TokenType::IDENTIFIER, "Expected parameter name").lexeme;
            expect(TokenType::COLON, "Expected ':' after parameter name");
            param.type = parse_type();
            param.location = param.type->location;
            
            if (accept(TokenType::EQ)) {
                param.default_value = parse_expression();
                seen_default = true;
            } else if (seen_default) {
                throw CompilerException(
                    "Non-defaulted parameter cannot follow a defaulted one", get_location());
            }
            
            params.params.push_back(std::move(param));
        } while (accept(TokenType::COMMA));
    }
    
    expect(TokenType::RPAREN, "Expected ')'");
    return params;
}

// ============================================================================
// NAME PARSING
// ============================================================================

NamePtr Parser::parse_name() {
    SourceLocation l = get_location();
    std::string first;
    if (accept(TokenType::SELF)) first = "Self";
    else if (accept(TokenType::THIS)) first = "this";
    else first = std::string(expect(TokenType::IDENTIFIER, "Expected identifier").lexeme);
    GenericArguments generics;
    
    if (peek().type == TokenType::LT && looks_like_generic_args_ahead()) {
        advance();
        generics = parse_generic_arguments();
    }
    
    auto root = std::make_unique<Name>(l, std::move(first), std::move(generics));

    Name* tail = root.get();
    while (peek().type == TokenType::COLON_COLON &&
           (peek_next().type == TokenType::IDENTIFIER ||
            peek_next().type == TokenType::SELF ||
            peek_next().type == TokenType::THIS)) {
        advance();
        SourceLocation sl = get_location();
        std::string seg;
        if (accept(TokenType::SELF)) seg = "Self";
        else if (accept(TokenType::THIS)) seg = "this";
        else seg = std::string(advance().lexeme);
        GenericArguments seg_generics;
        
        if (peek().type == TokenType::LT && looks_like_generic_args_ahead()) {
            advance();
            seg_generics = parse_generic_arguments();
        }
        
        tail->next = std::make_unique<Name>(sl, std::move(seg), std::move(seg_generics));
        tail = tail->next.get();
    }
    
    return root;
}

// ============================================================================
// EXPRESSION PARSING
// ============================================================================

ExpressionPtr Parser::parse_expression() { return parse_assignment(); }

ExpressionPtr Parser::parse_assignment() {
    auto expr = parse_ternary();
    
    switch (peek().type) {
        case TokenType::EQ:
        case TokenType::PLUS_EQ:   case TokenType::MINUS_EQ:
        case TokenType::STAR_EQ:   case TokenType::SLASH_EQ:
        case TokenType::PERCENT_EQ:
        case TokenType::AMP_EQ:    case TokenType::PIPE_EQ:
        case TokenType::CARET_EQ:  case TokenType::TILDE_EQ:
        case TokenType::LT_LT_EQ:  case TokenType::GT_GT_EQ: {
            SourceLocation l = get_location();
            BinaryOp op = token_to_binary_op(advance().type);
            auto right = parse_assignment();
            return std::make_unique<BinaryExpr>(l, op, std::move(expr), std::move(right));
        }
        default: return expr;
    }
}

ExpressionPtr Parser::parse_ternary() {
    auto expr = parse_logical_or();
    if (accept(TokenType::QUESTION)) {
        SourceLocation l = get_location();
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
        SourceLocation l = get_location(); advance();
        expr = std::make_unique<BinaryExpr>(l, BinaryOp::LOGICAL_OR, std::move(expr), parse_logical_and());
    }
    return expr;
}

ExpressionPtr Parser::parse_logical_and() {
    auto expr = parse_bitwise_or();
    while (peek().type == TokenType::AND) {
        SourceLocation l = get_location(); advance();
        expr = std::make_unique<BinaryExpr>(l, BinaryOp::LOGICAL_AND, std::move(expr), parse_bitwise_or());
    }
    return expr;
}

ExpressionPtr Parser::parse_bitwise_or() {
    auto expr = parse_bitwise_xor();
    while (peek().type == TokenType::PIPE) {
        SourceLocation l = get_location(); advance();
        expr = std::make_unique<BinaryExpr>(l, BinaryOp::BITWISE_OR, std::move(expr), parse_bitwise_xor());
    }
    return expr;
}

ExpressionPtr Parser::parse_bitwise_xor() {
    auto expr = parse_bitwise_and();
    while (peek().type == TokenType::CARET) {
        SourceLocation l = get_location(); advance();
        expr = std::make_unique<BinaryExpr>(l, BinaryOp::BITWISE_XOR, std::move(expr), parse_bitwise_and());
    }
    return expr;
}

ExpressionPtr Parser::parse_bitwise_and() {
    auto expr = parse_equality();
    while (peek().type == TokenType::AMP) {
        SourceLocation l = get_location(); advance();
        expr = std::make_unique<BinaryExpr>(l, BinaryOp::BITWISE_AND, std::move(expr), parse_equality());
    }
    return expr;
}

ExpressionPtr Parser::parse_equality() {
    auto expr = parse_comparison();
    while (peek().type == TokenType::EQ_EQ || peek().type == TokenType::BANG_EQ) {
        SourceLocation l = get_location();
        BinaryOp op = token_to_binary_op(advance().type);
        expr = std::make_unique<BinaryExpr>(l, op, std::move(expr), parse_comparison());
    }
    return expr;
}

ExpressionPtr Parser::parse_comparison() {
    auto expr = parse_shift();
    while (peek().type == TokenType::LT  || peek().type == TokenType::LTE ||
           peek().type == TokenType::GT  || peek().type == TokenType::GTE) {
        SourceLocation l = get_location();
        BinaryOp op = token_to_binary_op(advance().type);
        expr = std::make_unique<BinaryExpr>(l, op, std::move(expr), parse_shift());
    }
    return expr;
}

ExpressionPtr Parser::parse_shift() {
    auto expr = parse_term();
    while (peek().type == TokenType::LT_LT || peek().type == TokenType::GT_GT) {
        SourceLocation l = get_location();
        BinaryOp op = token_to_binary_op(advance().type);
        expr = std::make_unique<BinaryExpr>(l, op, std::move(expr), parse_term());
    }
    return expr;
}

ExpressionPtr Parser::parse_term() {
    auto expr = parse_factor();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        SourceLocation l = get_location();
        BinaryOp op = token_to_binary_op(advance().type);
        expr = std::make_unique<BinaryExpr>(l, op, std::move(expr), parse_factor());
    }
    return expr;
}

ExpressionPtr Parser::parse_factor() {
    auto expr = parse_unary();
    while (peek().type == TokenType::STAR   ||
           peek().type == TokenType::SLASH  ||
           peek().type == TokenType::PERCENT) {
        SourceLocation l = get_location();
        BinaryOp op = token_to_binary_op(advance().type);
        expr = std::make_unique<BinaryExpr>(l, op, std::move(expr), parse_unary());
    }
    return expr;
}

ExpressionPtr Parser::parse_unary() {
    if (accept(TokenType::MUT) && peek().type == TokenType::REF) {
        SourceLocation l = get_location();
        advance(); // consume ref
        return std::make_unique<UnaryExpr>(l, UnaryOp::ADDRESS_OF, parse_unary());
    }
    if (accept(TokenType::REF)) {
        SourceLocation l = get_location();
        return std::make_unique<UnaryExpr>(l, UnaryOp::ADDRESS_OF, parse_unary());
    }

    switch (peek().type) {
        case TokenType::MINUS:
        case TokenType::PLUS:
        case TokenType::BANG:
        case TokenType::TILDE:
        case TokenType::STAR:
        case TokenType::AMP: {
            SourceLocation l = get_location();
            UnaryOp op = token_to_unary_op(advance().type);
            return std::make_unique<UnaryExpr>(l, op, parse_unary());
        }
        default:
            return parse_postfix();
    }
}

ExpressionPtr Parser::parse_postfix() {
    auto expr = parse_primary();

    while (true) {
        SourceLocation l = get_location();

        if (peek().type == TokenType::DOT) {
            advance();
            auto member = parse_name();
            expr = std::make_unique<MemberAccessExpr>(l, std::move(expr), std::move(member));
        }
        else if (peek().type == TokenType::LPAREN) {
            advance();
            auto args = parse_call_args();
            expect(TokenType::RPAREN, "Expected ')' after call arguments");
            bool is_early_return = accept(TokenType::QUESTION);
            expr = std::make_unique<CallExpr>(l, std::move(expr), std::move(args), is_early_return);
        }
        else if (peek().type == TokenType::LBRACKET) {
            advance();
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

// ============================================================================
// LOOKAHEAD HEURISTICS
// ============================================================================

bool Parser::looks_like_generic_args(const ExpressionPtr& lhs) const {
    if (!lhs) return false;
    if (lhs->kind != ASTNode::NodeKind::NAME &&
        lhs->kind != ASTNode::NodeKind::MEMBER_ACCESS_EXPR) return false;
    return looks_like_generic_args_ahead();
}

bool Parser::looks_like_generic_args_ahead() const {
    if (peek().type != TokenType::LT) return false;

    size_t base = split_gt_pending_ ? idx_ : idx_ + 1;
    int depth = 1;
    size_t i = base;

    while (i < tokens_.size() && depth > 0) {
        TokenType t = tokens_[i].type;
        if      (t == TokenType::LT)          ++depth;
        else if (t == TokenType::GT)          --depth;
        else if (t == TokenType::GT_GT)       depth -= 2;
        else if (t == TokenType::EOF_TOKEN)   return false;
        else if (t == TokenType::DOT && depth == 1) return false;
        else if (t == TokenType::SEMICOLON ||
                 t == TokenType::LBRACE    ||
                 t == TokenType::RBRACE    ||
                 t == TokenType::AND       ||
                 t == TokenType::OR)           return false;
        ++i;
    }

    if (depth != 0) return false;
    if (i >= tokens_.size()) return true;

    TokenType after = tokens_[i].type;
    if (after == TokenType::IDENTIFIER) {
        const std::string& lex = tokens_[i].lexeme;
        return lex == "where" || lex == "for";
    }
    return after == TokenType::LPAREN
        || after == TokenType::DOT
        || after == TokenType::LBRACKET
        || after == TokenType::COLON_COLON
        || after == TokenType::COMMA
        || after == TokenType::RPAREN
        || after == TokenType::SEMICOLON
        || after == TokenType::EQ
        || after == TokenType::COLON
        || after == TokenType::LBRACE
        || after == TokenType::RBRACE;
}

// ============================================================================
// CALL ARGUMENTS
// ============================================================================

std::vector<ExpressionPtr> Parser::parse_call_args() {
    std::vector<ExpressionPtr> args;
    if (peek().type == TokenType::RPAREN) return args;
    do { args.push_back(parse_expression()); } while (accept(TokenType::COMMA));
    return args;
}

// ============================================================================
// PRIMARY EXPRESSION
// ============================================================================

ExpressionPtr Parser::parse_primary() {
    SourceLocation l = get_location();

    switch (peek().type) {
        case TokenType::INT_LITERAL: {
            std::string val = std::string(advance().lexeme);
            if (peek().type == TokenType::IDENTIFIER && peek().lexeme == "i") {
                advance();
                return std::make_unique<LiteralComplex>(l, val + "i");
            }
            return std::make_unique<LiteralInt>(l, std::move(val));
        }

        case TokenType::FLOAT_LITERAL: {
            std::string val = std::string(advance().lexeme);
            if (peek().type == TokenType::IDENTIFIER && peek().lexeme == "i") {
                advance();
                return std::make_unique<LiteralComplex>(l, val + "i");
            }
            return std::make_unique<LiteralFloat>(l, std::move(val));
        }

        case TokenType::COMPLEX_LITERAL:
            return std::make_unique<LiteralComplex>(l, advance().lexeme);

        case TokenType::STRING_LITERAL:
            return std::make_unique<LiteralString>(l, advance().lexeme);

        case TokenType::RAW_STRING_LITERAL:
            return std::make_unique<LiteralRawString>(l, advance().lexeme);

        case TokenType::INTERP_STRING_START:
            return parse_interp_string();

        case TokenType::TRUE:
            advance();
            return std::make_unique<LiteralBool>(l, true);

        case TokenType::FALSE:
            advance();
            return std::make_unique<LiteralBool>(l, false);

        case TokenType::NULLPTR:
            advance();
            return std::make_unique<LiteralNullptr>(l);

        case TokenType::THIS:
            advance();
            return std::make_unique<Name>(l, "this");

        case TokenType::SELF:
            advance();
            return std::make_unique<Name>(l, "Self");

        case TokenType::LBRACKET:
            return parse_array_literal();

        case TokenType::LBRACE:
            return parse_map_literal();

        case TokenType::NEW:
            advance();
            return parse_new_expr();

        case TokenType::IDENTIFIER:
            return parse_name();

        case TokenType::LAMBDA:
            advance();
            return parse_lambda();

        case TokenType::LPAREN: {
            advance();
            return parse_group_or_tuple();
        }
        default:
            throw CompilerException(
                std::format("Unexpected token '{}' in expression", peek().lexeme),
                peek().location);
    }
}

ExpressionPtr Parser::parse_array_literal() {
    SourceLocation l = get_location();
    expect(TokenType::LBRACKET, "Expected '['");
    std::vector<ExpressionPtr> elems;
    if (peek().type != TokenType::RBRACKET)
        do { elems.push_back(parse_expression()); } while (accept(TokenType::COMMA));
    expect(TokenType::RBRACKET, "Expected ']' to close array literal");
    return std::make_unique<LiteralArray>(l, std::move(elems));
}

ExpressionPtr Parser::parse_map_literal() {
    SourceLocation l = get_location();
    expect(TokenType::LBRACE, "Expected '{'");
    std::vector<std::pair<ExpressionPtr, ExpressionPtr>> pairs;
    if (peek().type != TokenType::RBRACE) {
        do {
            auto key = parse_expression();
            expect(TokenType::COLON, "Expected ':' after map key");
            auto val = parse_expression();
            pairs.emplace_back(std::move(key), std::move(val));
        } while (accept(TokenType::COMMA));
    }
    expect(TokenType::RBRACE, "Expected '}' to close map literal");
    return std::make_unique<LiteralMap>(l, std::move(pairs));
}

ExpressionPtr Parser::parse_tuple_literal() {
    SourceLocation l = get_location();
    std::vector<ExpressionPtr> elems;
    if (peek().type != TokenType::RPAREN)
        do { elems.push_back(parse_expression()); } while (accept(TokenType::COMMA));
    expect(TokenType::RPAREN, "Expected ')' to close tuple literal");
    return std::make_unique<LiteralTuple>(l, std::move(elems));
}

ExpressionPtr Parser::parse_group_or_tuple() {
    // '(' already consumed
    SourceLocation l = get_location();
    
    if (peek().type == TokenType::RPAREN) {
        advance();
        // Empty parentheses - could be empty tuple or unit value
        return std::make_unique<LiteralTuple>(l, std::vector<ExpressionPtr>{});
    }
    
    auto expr = parse_expression();
    
    if (peek().type == TokenType::COMMA) {
        // It's a tuple
        std::vector<ExpressionPtr> elems;
        elems.push_back(std::move(expr));
        do { elems.push_back(parse_expression()); } while (accept(TokenType::COMMA));
        expect(TokenType::RPAREN, "Expected ')' after tuple literal");
        return std::make_unique<LiteralTuple>(l, std::move(elems));
    }
    
    expect(TokenType::RPAREN, "Expected ')' after grouped expression");
    return expr;  // Just a parenthesized expression, return it directly
}

ExpressionPtr Parser::parse_interp_string() {
    SourceLocation l = get_location();
    expect(TokenType::INTERP_STRING_START, "Expected interpolated string start");
    std::vector<LiteralInterpString::Part> parts;
    while (peek().type != TokenType::INTERP_STRING_END && !is_at_end()) {
        if (peek().type == TokenType::INTERP_STRING_PART) {
            std::string text = std::string(advance().lexeme);
            parts.push_back({ false, std::move(text), nullptr });
        } else {
            auto e = parse_expression();
            parts.push_back({ true, {}, std::move(e) });
        }
    }
    expect(TokenType::INTERP_STRING_END, "Unterminated interpolated string");
    return std::make_unique<LiteralInterpString>(l, std::move(parts));
}

ExpressionPtr Parser::parse_new_expr() {
    SourceLocation l = get_location();
    auto alloc_type = parse_type();
    std::vector<ExpressionPtr> args;
    if (accept(TokenType::LPAREN)) {
        args = parse_call_args();
        expect(TokenType::RPAREN, "Expected ')' after new arguments");
    }
    return std::make_unique<NewExpr>(l, std::move(alloc_type), std::move(args));
}

ExpressionPtr Parser::parse_lambda() {
    // 'lambda' already consumed
    SourceLocation l = get_location();

    GenericParameters gparams;
    if (peek().type == TokenType::LT) {
        advance();
        gparams = parse_generic_params();
    }

    auto params = parse_parameters();

    TypePtr ret_type;
    if (accept(TokenType::ARROW))
        ret_type = parse_type();

    expect(TokenType::FAT_ARROW, "Expected '=>' after lambda signature");

    std::variant<ExpressionPtr, BlockPtr> body;
    if (peek().type == TokenType::LBRACE) {
        body = parse_block();
    } else {
        body = parse_expression();
    }

    return std::make_unique<Lambda>(l, std::move(gparams), std::move(params), 
                                     std::move(ret_type), std::move(body));
}

// ============================================================================
// STATEMENT PARSING
// ============================================================================

BlockPtr Parser::parse_block() {
    SourceLocation l = get_location();
    expect(TokenType::LBRACE, "Expected '{'");
    
    enter_scope(TokenType::RBRACE);
    
    std::vector<ConstructPtr> stmts;
    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        if (peek().type == TokenType::SEMICOLON) { advance(); continue; }
        try {
            stmts.push_back(parse_statement());
        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }
    
    if (peek().type == TokenType::RBRACE) {
        advance();
    } else {
        g_diagnostics.syntax_error("Expected '}'", get_location());
        had_errors_ = true;
    }
    
    leave_scope();
    return std::make_unique<Block>(l, std::move(stmts));
}

ConstructPtr Parser::parse_statement() {
    Directives dirs = parse_directives();

    // Declaration keywords
    if (accept(TokenType::LET)) {
        return parse_variable_decl(false, false, std::move(dirs));
    }
    if (accept(TokenType::VAR)) {
        return parse_variable_decl(true, false, std::move(dirs));
    }
    if (accept(TokenType::FUNC)) {
        return parse_function_decl(false, false, std::move(dirs));
    }
    if (accept(TokenType::CLASS))
        return parse_class_decl(std::move(dirs));
    if (accept(TokenType::TRAIT))
        return parse_trait_decl(std::move(dirs));
    if (accept(TokenType::IMPL))
        return parse_impl_decl(std::move(dirs));
    if (accept(TokenType::TYPE))
        return parse_type_alias_decl(std::move(dirs));
    if (accept(TokenType::ENUM))
        return parse_enum_decl(std::move(dirs));

    /*
    if (accept(TokenType::SCOPE))
        return parse_scope_decl(std::move(dirs));
    if (accept(TokenType::OPERATOR))
        return parse_operator_overload(false, std::move(dirs));
    */

    if (!dirs.empty())
        throw CompilerException("Directives must precede a declaration", get_location());

    // Control flow
    if (accept(TokenType::IF))      return parse_if_stmt();
    if (accept(TokenType::WHILE))   return parse_while_stmt();
    if (accept(TokenType::DO))      return parse_do_while_stmt();
    if (accept(TokenType::FOREACH)) return parse_foreach_stmt();
    if (accept(TokenType::MATCH))   return parse_match_stmt();

    // Heap de-allocation
    if (accept(TokenType::DELETE)) return parse_delete_stmt();

    // Exceptions
    if (accept(TokenType::TRY))   return parse_try_catch_stmt();
    if (accept(TokenType::THROW)) return parse_throw_stmt();

    // Jumps
    if (accept(TokenType::RETURN))   return parse_return_stmt();
    if (accept(TokenType::BREAK))    return parse_break_stmt();
    if (accept(TokenType::CONTINUE)) return parse_continue_stmt();

    // Module
    if (accept(TokenType::IMPORT)) { parse_import_decl(); return nullptr; }
    if (accept(TokenType::EXPORT)) { parse_export_decl(); return nullptr; }
    if (accept(TokenType::MODULE)) { parse_module_decl(); return nullptr; }

    // Expression statement
    SourceLocation l = get_location();
    auto expr = parse_expression();
    expect_semicolon("expression statement");
    return std::make_unique<ExpressionStmt>(l, std::move(expr));
}

// ============================================================================
// DECLARATIONS
// ============================================================================

Ptr<VariableDecl> Parser::parse_variable_decl(bool is_mutable, bool is_static, Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected variable name").lexeme);
    TypePtr type;
    if (accept(TokenType::COLON))
        type = parse_type();

    ExpressionPtr init;
    if (accept(TokenType::EQ))
        init = parse_expression();
    else if (!type)
        throw CompilerException(
            "Variable declaration requires either a type annotation or an initialiser", get_location());

    expect_semicolon("variable declaration");
    return std::make_unique<VariableDecl>(l, std::move(name), std::move(type), std::move(init), is_mutable, is_static, std::move(dirs));
}

Ptr<FunctionDecl> Parser::parse_function_decl(bool is_static, bool is_mut, Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected function name").lexeme);

    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }

    auto params = parse_parameters();

    TypePtr ret;
    if (accept(TokenType::ARROW)) {
        ret = parse_type();
        if (accept(TokenType::BANG))
            (void)parse_type(); // consume error type (e.g. f64!str)
    }

    BlockPtr body = parse_block();

    return std::make_unique<FunctionDecl>(l, std::move(name), std::move(gparams), std::move(params),
                                           std::move(ret), std::move(body), is_static, is_mut, std::move(dirs));
}


/**
 * class NAME<...>
 */

Ptr<ClassDecl> Parser::parse_class_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected class name").lexeme);

    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }
    
    std::vector<Ptr<ConstructorDecl>> constructors;
    std::optional<Ptr<DestructorDecl>> destructor;    
    std::vector<Ptr<ClassDecl::Field>> fields;
    std::vector<Ptr<Method>> methods;
    std::vector<Ptr<Operator>> operators;


    expect(TokenType::LBRACE, "Expected '{' after class declaration");
    enter_scope(TokenType::RBRACE);

    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        try {
            Directives member_dirs = parse_directives();

            bool is_public = false;
            bool has_visibility = false;
            bool mem_static = false;
            bool mem_mut = false;

            while (true) {
                if (peek().type == TokenType::PUB) {
                    if (has_visibility)
                        throw CompilerException("multiple visibility modifiers", get_location());
                    advance();
                    is_public = true;
                    has_visibility = true;
                }
                else if (peek().type == TokenType::STATIC) {
                    advance();
                    mem_static = true;
                }
                else if (peek().type == TokenType::MUT) {
                    advance();
                    mem_mut = true;
                }
                else {
                    break;
                }
            }

            if (mem_mut && !(peek().type == TokenType::FUNC || peek().type == TokenType::OPERATOR))
                throw CompilerException(
                    "'mut' modifier is only valid before 'func' or 'operator' in a class body", get_location());

            if (mem_static && peek().type == TokenType::OPERATOR)
                throw CompilerException(
                    "'static' modifier is not valid on operator overloads", get_location());

            
            /** Check for constructor (no return type, name matches class)
             * classname(...) { ... }
             */
            if (peek().type == TokenType::IDENTIFIER && 
                std::string_view(peek().lexeme) == name && 
                peek_next().type == TokenType::LPAREN) {
                advance();  // consume identifier
                if (mem_static || mem_mut)
                    throw CompilerException("Constructor cannot have 'static' or 'mut' modifiers", get_location());
                GenericParameters ctor_gparams;
                if (peek().type == TokenType::LT) { advance(); ctor_gparams = parse_generic_params(); }
                Parameters ctor_params = parse_parameters();
                BlockPtr ctor_body = peek().type == TokenType::LBRACE ? parse_block() : nullptr;
                constructors.push_back(std::make_unique<ConstructorDecl>(l, std::move(ctor_gparams), 
                                                                        std::move(ctor_params), 
                                                                        std::move(ctor_body), 
                                                                        std::move(member_dirs), is_public));
            }
            /** Check for destructor
             *  ~classname() { ... }
             */
            else if (peek().type == TokenType::TILDE && peek_next().type == TokenType::IDENTIFIER) {
                advance();  // consume ~
                std::string dtor_name = std::string(expect(TokenType::IDENTIFIER, "Expected class name in destructor").lexeme);
                if (dtor_name != name)
                    throw CompilerException("Destructor name must match class name", get_location());

                if (has_visibility || is_public || mem_static || mem_mut)
                    throw CompilerException("Destructor cannot have modifiers", get_location());

                expect(TokenType::LPAREN, "Expected '(' in destructor");
                expect(TokenType::RPAREN, "Expected ')' in destructor");
                BlockPtr dtor_body = peek().type == TokenType::LBRACE ? parse_block() : nullptr;
                destructor = std::make_unique<DestructorDecl>(l, std::move(dtor_body), std::move(member_dirs));
            }


            else if (accept(TokenType::FUNC)) {
                if (peek().type == TokenType::TILDE && peek_next().type == TokenType::IDENTIFIER) {
                    advance();  // consume ~
                    std::string dtor_name = std::string(expect(TokenType::IDENTIFIER, "Expected class name in destructor").lexeme);
                    if (dtor_name != name)
                        throw CompilerException("Destructor name must match class name", get_location());
                    if (mem_static || mem_mut)
                        throw CompilerException("Destructor cannot have 'static' or 'mut' modifiers", get_location());

                    expect(TokenType::LPAREN, "Expected '(' in destructor");
                    expect(TokenType::RPAREN, "Expected ')' in destructor");
                    BlockPtr dtor_body = peek().type == TokenType::LBRACE ? parse_block() : nullptr;
                    destructor = std::make_unique<DestructorDecl>(l, std::move(dtor_body), std::move(member_dirs));
                } else {
                    auto func_represnetation = parse_function_decl(mem_static, mem_mut, std::move(member_dirs));
                    methods.push_back(std::make_unique<Method>(
                        func_represnetation->location, func_represnetation->name,
                        std::move(func_represnetation->generic_params),
                        std::move(func_represnetation->params),
                        std::move(func_represnetation->return_type),
                        std::move(func_represnetation->body),
                        mem_static, mem_mut, is_public, std::move(func_represnetation->dirs)));
                }
            }

            else if (accept(TokenType::OPERATOR)) {
                SourceLocation op_loc = get_location();

                if (has_visibility || is_public || mem_static)
                    throw CompilerException("Operator overload cannot have visibility or 'static' modifiers", get_location());

                std::string op_name = parse_operator_name(op_loc);
                Parameters op_params = parse_parameters();
                OverloadableOp op_kind = resolve_overloadable_op(op_name);

                TypePtr op_return_type;
                if (accept(TokenType::ARROW)) op_return_type = parse_type();

                BlockPtr op_body = parse_block();

                operators.push_back(std::make_unique<Operator>(
                    op_loc, op_kind, std::move(op_params), std::move(op_return_type),
                    std::move(op_body), mem_mut, false, std::move(member_dirs)));
            }

            else if (accept(TokenType::VAR)) {
                auto field = parse_variable_decl(true, mem_static, std::move(member_dirs));
                fields.push_back(std::make_unique<ClassDecl::Field>(
                    field->name, std::move(field->type), std::move(field->initialiser),
                    is_public, mem_static, true));
            }

            else if (accept(TokenType::LET)) {
                auto field = parse_variable_decl(false, mem_static, std::move(member_dirs));
                fields.push_back(std::make_unique<ClassDecl::Field>(
                    field->name, std::move(field->type), std::move(field->initialiser),
                    is_public, mem_static, false));
            }

            else {
                throw CompilerException(
                    "Expected constructor, destructor, method, operator overload, or field declaration in class body", get_location());
            }

        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }

    expect(TokenType::RBRACE, "Expected '}' to close class declaration");
    leave_scope();

    return std::make_unique<ClassDecl>(l, std::move(name), std::move(gparams), 
                                       std::move(constructors), std::move(destructor), 
                                       std::move(fields), std::move(methods), std::move(operators), 
                                       std::move(dirs));

    
}


/*
Ptr<OperatorOverloadDecl> Parser::parse_operator_overload(bool is_mut, Directives dirs) {
    SourceLocation l = get_location();
    TokenType op_tok = peek().type;
    advance();

    if (op_tok == TokenType::LBRACKET)
        expect(TokenType::RBRACKET, "Expected ']' for operator[]");

    auto params = parse_parameters();

    TypePtr ret;
    if (accept(TokenType::ARROW)) ret = parse_type();

    bool is_unary = params.params.empty();
    auto op = token_to_overloadable_op(op_tok, is_unary);

    BlockPtr body;
    if (!accept(TokenType::SEMICOLON))
        body = parse_block();

    return std::make_unique<OperatorOverloadDecl>(l, op, std::move(params), std::move(ret), 
                                                    std::move(body), is_mut, std::move(dirs));
}
*/

/*
Ptr<ClassDecl> Parser::parse_class_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected class name").lexeme);

    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }

    std::vector<NamePtr> traits;
    if (accept(TokenType::IMPL)) {
        do {
            traits.push_back(parse_name());
        } while (accept(TokenType::COMMA));
    }

    expect(TokenType::LBRACE, "Expected '{' after class declaration");
    enter_scope(TokenType::RBRACE);
    
    std::vector<Ptr<ConstructorDecl>> constructors;
    std::optional<Ptr<DestructorDecl>> destructor;
    std::vector<std::pair<OverloadableOp, std::vector<OperatorOverloadDecl*>>> operators;
    std::vector<VariableDecl*> fields;
    std::vector<ClassDecl::Member> members;
    
    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        try {
            Directives member_dirs = parse_directives();

            bool is_public = false;
            bool has_visibility = false;
            bool mem_static = false;
            bool mem_mut = false;

            while (true) {
                if (peek().type == TokenType::PUB) {
                    if (has_visibility)
                        throw CompilerException("multiple visibility modifiers", get_location());
                    advance();
                    is_public = true;
                    has_visibility = true;
                }
                else if (peek().type == TokenType::PRIVATE) {
                    if (has_visibility)
                        throw CompilerException("multiple visibility modifiers", get_location());
                    advance();
                    is_public = false;
                    has_visibility = true;
                }
                else if (peek().type == TokenType::STATIC) {
                    advance();
                    mem_static = true;
                }
                else if (peek().type == TokenType::MUT) {
                    advance();
                    mem_mut = true;
                }
                else {
                    break;
                }
            }

            if (mem_mut && !(peek().type == TokenType::FUNC || peek().type == TokenType::OPERATOR))
                throw CompilerException(
                    "'mut' modifier is only valid before 'func' or 'operator' in a class body", get_location());

            if (mem_static && peek().type == TokenType::OPERATOR)
                throw CompilerException(
                    "'static' modifier is not valid on operator overloads", get_location());

            // Check for constructor (implicit - no return type, name matches class)
            if (peek().type == TokenType::IDENTIFIER && 
                std::string_view(peek().lexeme) == name && 
                peek_next().type == TokenType::LPAREN) {
                advance();  // consume identifier
                GenericParameters ctor_gparams;
                if (peek().type == TokenType::LT) { advance(); ctor_gparams = parse_generic_params(); }
                Parameters ctor_params = parse_parameters();
                BlockPtr ctor_body = peek().type == TokenType::LBRACE ? parse_block() : nullptr;
                constructors.push_back(std::make_unique<ConstructorDecl>(l, std::move(ctor_gparams), 
                                                                        std::move(ctor_params), 
                                                                        std::move(ctor_body), 
                                                                        std::move(member_dirs)));
            }
            // Check for destructor (~ClassName)
            else if (peek().type == TokenType::TILDE && 
                     peek_next().type == TokenType::IDENTIFIER) {
                advance();  // consume ~
                std::string dtor_name = std::string(expect(TokenType::IDENTIFIER, "Expected class name in destructor").lexeme);
                if (dtor_name != name)
                    throw CompilerException("Destructor name must match class name", get_location());
                expect(TokenType::LPAREN, "Expected '(' in destructor");
                expect(TokenType::RPAREN, "Expected ')' in destructor");
                BlockPtr dtor_body = peek().type == TokenType::LBRACE ? parse_block() : nullptr;
                destructor = std::make_unique<DestructorDecl>(l, std::move(dtor_body), std::move(member_dirs));
            }
            else if (accept(TokenType::LET)) {
                auto var_decl = parse_variable_decl(false, mem_static, std::move(member_dirs));
                auto var_ptr = var_decl.get();
                fields.push_back(var_ptr);
                members.emplace_back(std::move(var_decl), is_public);
            }
            else if (accept(TokenType::VAR)) {
                auto var_decl = parse_variable_decl(true, mem_static, std::move(member_dirs));
                auto var_ptr = var_decl.get();
                fields.push_back(var_ptr);
                members.emplace_back(std::move(var_decl), is_public);
            }
            else if (accept(TokenType::FUNC)) {
                DeclarationPtr func_decl = parse_function_decl(mem_static, mem_mut, std::move(member_dirs));
                members.emplace_back(std::move(func_decl), is_public);
            }
            else if (accept(TokenType::OPERATOR)) {
                auto op_decl = parse_operator_overload(mem_mut, std::move(member_dirs));
                auto op_ptr = op_decl.get();
                // Extract operator type and add to operators map
                OverloadableOp op = op_ptr->op;
                bool found = false;
                for (auto& [op_type, op_list] : operators) {
                    if (op_type == op) {
                        op_list.push_back(op_ptr);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    operators.push_back({op, {op_ptr}});
                }
                members.emplace_back(std::move(op_decl), is_public);
            }
            else
                throw CompilerException(
                    std::format("Unexpected '{}' in class body", peek().lexeme), get_location());

        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }
    
    if (peek().type == TokenType::RBRACE) {
        advance();
    }
    leave_scope();

    return std::make_unique<ClassDecl>(l, std::move(name), std::move(gparams), 
                                        std::move(traits), 
                                        std::move(constructors),
                                        std::move(destructor),
                                        std::move(operators),
                                        std::move(fields),
                                        std::move(members), 
                                        std::move(dirs));
}


Ptr<TraitDecl> Parser::parse_trait_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected trait name").lexeme);

    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }

    expect(TokenType::LBRACE, "Expected '{' after trait name");
    enter_scope(TokenType::RBRACE);

    std::vector<TraitDecl::MethodReq> method_reqs;
    std::vector<TraitDecl::OperatorReq> operator_reqs;

    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        try {
            bool req_static = false, req_mut = false;
            while (true) {
                if (peek().type == TokenType::STATIC) { advance(); req_static = true; }
                else if (peek().type == TokenType::MUT) { advance(); req_mut = true; }
                else break;
            }

            if (accept(TokenType::FUNC)) {
                TraitDecl::MethodReq req;
                req.is_static = req_static;
                req.is_mut = req_mut;
                req.name = expect(TokenType::IDENTIFIER, "Expected method name").lexeme;

                if (peek().type == TokenType::LT) {
                    advance();
                    req.generic_params = parse_generic_params();
                }

                req.param_types = parse_parameters();

                if (accept(TokenType::ARROW)) req.return_type = parse_type();
                expect_semicolon("trait method signature");
                method_reqs.push_back(std::move(req));

            } else if (accept(TokenType::OPERATOR)) {
                TraitDecl::OperatorReq req;
                TokenType op_tok = peek().type;
                advance();
                if (op_tok == TokenType::LBRACKET)
                    expect(TokenType::RBRACKET, "Expected ']' for operator[]");
                req.op = token_to_overloadable_op(op_tok, false);

                req.param_types = parse_parameters();

                if (accept(TokenType::ARROW)) req.return_type = parse_type();
                expect_semicolon("trait operator signature");
                operator_reqs.push_back(std::move(req));

            } else {
                throw CompilerException(
                    std::format("Expected 'func' or 'operator' in trait body, got '{}'", peek().lexeme),
                    get_location());
            }
        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }

    if (peek().type == TokenType::RBRACE) {
        advance();
    }
    leave_scope();

    return std::make_unique<TraitDecl>(l, std::move(name), std::move(gparams),
                                        std::vector<TraitConstraint>{},
                                        std::move(method_reqs), std::move(operator_reqs), std::move(dirs));
}
*/

Ptr<TraitDecl> Parser::parse_trait_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected trait name").lexeme);

    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }

    expect(TokenType::LBRACE, "Expected '{' after trait name");
    enter_scope(TokenType::RBRACE);

    std::vector<TraitDecl::MethodReq> method_reqs;
    std::vector<TraitDecl::OperatorReq> operator_reqs;

    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        try {
            bool req_mut = false;
            while (peek().type == TokenType::MUT) { advance(); req_mut = true; }

            if (accept(TokenType::FUNC)) {
                TraitDecl::MethodReq req;
                req.is_mut = req_mut;
                req.name = expect(TokenType::IDENTIFIER, "Expected method name").lexeme;

                if (peek().type == TokenType::LT) {
                    advance();
                    req.generic_params = parse_generic_params();
                }

                Parameters params = parse_parameters();
                for (auto& p : params.params)
                    req.param_types.push_back(std::move(p.type));

                if (accept(TokenType::ARROW)) req.return_type = parse_type();
                expect_semicolon("trait method signature");
                method_reqs.push_back(std::move(req));

            } else if (accept(TokenType::OPERATOR)) {
                SourceLocation op_loc = get_location();
                std::string op_name = parse_operator_name(op_loc);
                Parameters params = parse_parameters();

                TraitDecl::OperatorReq req;
                req.op = resolve_overloadable_op(op_name);
                for (auto& p : params.params)
                    req.param_types.push_back(std::move(p.type));

                if (accept(TokenType::ARROW)) req.return_type = parse_type();
                expect_semicolon("trait operator signature");
                operator_reqs.push_back(std::move(req));

            } else {
                throw CompilerException(
                    std::format("Expected 'func' or 'operator' in trait body, got '{}'", peek().lexeme),
                    get_location());
            }
        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }

    expect(TokenType::RBRACE, "Expected '}' to close trait declaration");
    leave_scope();

    return std::make_unique<TraitDecl>(l, std::move(name), std::move(gparams),
                                        std::move(method_reqs), std::move(operator_reqs), std::move(dirs));
}

Ptr<ImplDecl> Parser::parse_impl_decl(Directives dirs) {
    SourceLocation l = get_location();

    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }

    NamePtr first = parse_name();

    NamePtr trait_name;
    NamePtr target_type;
    if (peek().type == TokenType::IDENTIFIER && peek().lexeme == "for") {
        advance();
        trait_name = std::move(first);
        target_type = parse_name();
    } else {
        target_type = std::move(first);
    }


    expect(TokenType::LBRACE, "Expected '{' after impl header");
    enter_scope(TokenType::RBRACE);

    std::vector<Ptr<Method>> methods;
    std::vector<Ptr<Operator>> operators;

    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        try {
            Directives member_dirs = parse_directives();

            bool is_public = false;
            bool has_visibility = false;
            bool mem_static = false;
            bool mem_mut = false;

            while (true) {
                if (peek().type == TokenType::PUB) {
                    if (has_visibility)
                        throw CompilerException("multiple visibility modifiers", get_location());
                    advance();
                    is_public = true;
                    has_visibility = true;
                } else if (peek().type == TokenType::STATIC) {
                    advance();
                    mem_static = true;
                } else if (peek().type == TokenType::MUT) {
                    advance();
                    mem_mut = true;
                } else {
                    break;
                }
            }

            if (accept(TokenType::FUNC)) {
                auto func = parse_function_decl(mem_static, mem_mut, std::move(member_dirs));
                methods.push_back(std::make_unique<Method>(
                    func->location, func->name,
                    std::move(func->generic_params), std::move(func->params),
                    std::move(func->return_type), std::move(func->body),
                    mem_static, mem_mut, is_public, std::move(func->dirs)));
            } else if (accept(TokenType::OPERATOR)) {
                SourceLocation op_loc = get_location();
                if (has_visibility || is_public || mem_static)
                    throw CompilerException("Operator overload cannot have visibility or 'static' modifiers", get_location());

                std::string op_name = parse_operator_name(op_loc);
                Parameters op_params = parse_parameters();
                OverloadableOp op_kind = resolve_overloadable_op(op_name);

                TypePtr op_return_type;
                if (accept(TokenType::ARROW)) op_return_type = parse_type();

                BlockPtr op_body = parse_block();
                operators.push_back(std::make_unique<Operator>(
                    op_loc, op_kind, std::move(op_params), std::move(op_return_type),
                    std::move(op_body), mem_mut, false, std::move(member_dirs)));
            } else {
                throw CompilerException(
                    std::format("Expected 'func' or 'operator' in impl body, got '{}'", peek().lexeme),
                    get_location());
            }
        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }

    expect(TokenType::RBRACE, "Expected '}' to close impl block");
    leave_scope();

    return std::make_unique<ImplDecl>(l, std::move(trait_name), std::move(target_type),
                                      std::move(gparams), std::move(methods), std::move(operators),
                                      std::move(dirs));
}

std::string Parser::parse_operator_name(SourceLocation& op_loc) {
    op_loc = get_location();
    std::string op_name;
    while (peek().type != TokenType::LPAREN && peek().type != TokenType::LBRACE &&
           peek().type != TokenType::SEMICOLON && !is_at_end()) {
        op_name += advance().lexeme;
        if (op_name.size() > 4)
            throw CompilerException("Invalid operator overload syntax", get_location());
    }
    if (op_name.empty())
        throw CompilerException("Expected operator name after 'operator'", op_loc);
    return op_name;
}

OverloadableOp Parser::resolve_overloadable_op(const std::string& op_name) const {
    for (const auto& pair : string_to_binary_overloadable_op) {
        if (pair.first == op_name) return pair.second;
    }
    for (const auto& pair : string_to_unary_overloadable_op) {
        if (pair.first == op_name) return pair.second;
    }
    throw CompilerException(
        std::format("'{}' is not a valid overloadable operator", op_name),
        get_location());
}

Ptr<TypeAliasDecl> Parser::parse_type_alias_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string alias = std::string(expect(TokenType::IDENTIFIER, "Expected type alias name").lexeme);
    GenericParameters gparams;
    if (peek().type == TokenType::LT) { advance(); gparams = parse_generic_params(); }
    expect(TokenType::EQ, "Expected '=' in type alias");
    auto target = parse_type();
    expect_semicolon("type alias");
    return std::make_unique<TypeAliasDecl>(l, std::move(alias), std::move(gparams), std::move(target), std::move(dirs));
}

Ptr<EnumDecl> Parser::parse_enum_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected enum name").lexeme);
    expect(TokenType::LBRACE, "Expected '{'");
    std::vector<EnumDecl::Variant> variants;
    if (peek().type != TokenType::RBRACE) {
        do {
            variants.push_back({ expect(TokenType::IDENTIFIER, "Expected variant name").lexeme });
        } while (accept(TokenType::COMMA));
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return std::make_unique<EnumDecl>(l, std::move(name), std::move(variants), std::move(dirs));
}

/*
Ptr<ScopeDecl> Parser::parse_scope_decl(Directives dirs) {
    SourceLocation l = get_location();
    std::string name = std::string(expect(TokenType::IDENTIFIER, "Expected scope name").lexeme);
    expect(TokenType::LBRACE, "Expected '{'");
    enter_scope(TokenType::RBRACE);
    
    std::vector<DeclarationPtr> members;
    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        try {
            auto stmt = parse_statement();
            if (!stmt) continue;  // Skip imports/exports
            
            switch (stmt->kind) {
                case ASTNode::NodeKind::FUNCTION_DECL:
                case ASTNode::NodeKind::CLASS_DECL:
                case ASTNode::NodeKind::TRAIT_DECL:
                case ASTNode::NodeKind::TYPE_ALIAS_DECL:
                case ASTNode::NodeKind::ENUM_DECL:
                case ASTNode::NodeKind::VARIABLE_DECL:
                case ASTNode::NodeKind::SCOPE_DECL:
                    members.push_back(Ptr<Declaration>(static_cast<Declaration*>(stmt.release())));
                    break;
                default:
                    throw CompilerException("Only declarations are allowed inside a scope", get_location());
            }
        } catch (const CompilerException& e) {
            report_and_synchronise(e, TokenType::RBRACE);
        }
    }
    
    if (peek().type == TokenType::RBRACE) {
        advance();
    }
    leave_scope();
    
    return std::make_unique<ScopeDecl>(l, std::move(name), std::move(members), std::move(dirs));
}
*/

// ============================================================================
// CONTROL FLOW
// ============================================================================

ConstructPtr Parser::parse_if_stmt() {
    SourceLocation l = get_location();

    IfStmt::Branch if_branch;
    if_branch.condition = parse_expression();
    if_branch.body = parse_block();

    std::vector<IfStmt::Branch> elif_branches;
    while (accept(TokenType::ELIF)) {
        IfStmt::Branch b;
        b.condition = parse_expression();
        b.body = parse_block();
        elif_branches.push_back(std::move(b));
    }

    BlockPtr else_body;
    if (accept(TokenType::ELSE)) else_body = parse_block();

    return std::make_unique<IfStmt>(l, std::move(if_branch), std::move(elif_branches), std::move(else_body));
}

ConstructPtr Parser::parse_while_stmt() {
    SourceLocation l = get_location();
    auto cond = parse_expression();
    auto body = parse_block();
    return std::make_unique<WhileStmt>(l, std::move(cond), std::move(body));
}

ConstructPtr Parser::parse_do_while_stmt() {
    SourceLocation l = get_location();
    auto body = parse_block();
    expect(TokenType::WHILE, "Expected 'while' after do body");
    auto cond = parse_expression();
    expect_semicolon("do-while condition");
    return std::make_unique<DoWhileStmt>(l, std::move(body), std::move(cond));
}

ConstructPtr Parser::parse_foreach_stmt() {
    SourceLocation l = get_location();
    std::string iter = std::string(expect(TokenType::IDENTIFIER, "Expected iterator variable name").lexeme);
    TypePtr iter_type;
    if (accept(TokenType::COLON)) iter_type = parse_type();
    expect(TokenType::IN, "Expected 'in' after foreach variable");
    auto iterable = parse_expression();
    auto body = parse_block();
    return std::make_unique<ForeachStmt>(l, std::move(iter), std::move(iter_type), std::move(iterable), std::move(body));
}

ConstructPtr Parser::parse_match_stmt() {
    SourceLocation l = get_location();
    auto subject = parse_expression();
    expect(TokenType::LBRACE, "Expected '{' after match subject");

    std::vector<MatchStmt::Arm> arms;
    while (peek().type != TokenType::RBRACE && !is_at_end()) {
        MatchStmt::Arm arm;
        if (peek().type == TokenType::IDENTIFIER && peek().lexeme == "case")
            advance();
        if (peek().type == TokenType::IDENTIFIER && peek().lexeme == "_") {
            advance();
            arm.pattern = nullptr;
        } else {
            arm.pattern = parse_expression();
        }
        expect(TokenType::FAT_ARROW, "Expected '=>' after match pattern");
        arm.body = (peek().type == TokenType::LBRACE)
                   ? std::move(parse_block())
                   : [&]() -> BlockPtr {
                        std::vector<ConstructPtr> single_stmt;
                        single_stmt.push_back(parse_statement());
                        return std::make_unique<Block>(get_location(), std::move(single_stmt));
                   }();
        arms.push_back(std::move(arm));
    }
    expect(TokenType::RBRACE, "Expected '}' to close match");
    return std::make_unique<MatchStmt>(l, std::move(subject), std::move(arms));
}

// ============================================================================
// HEAP DEALLOCATION
// ============================================================================

ConstructPtr Parser::parse_delete_stmt() {
    SourceLocation l = get_location();
    auto target = parse_expression();
    expect_semicolon("delete statement");
    return std::make_unique<DeleteStmt>(l, std::move(target));
}

// ============================================================================
// EXCEPTION HANDLING
// ============================================================================

ConstructPtr Parser::parse_try_catch_stmt() {
    SourceLocation l = get_location();
    auto try_body = parse_block();

    std::vector<TryCatchStmt::CatchClause> catches;
    while (accept(TokenType::CATCH)) {
        expect(TokenType::LPAREN, "Expected '(' after 'catch'");
        std::string ename = std::string(expect(TokenType::IDENTIFIER, "Expected exception variable name").lexeme);
        expect(TokenType::COLON, "Expected ':' after exception variable");
        auto etype = parse_type();
        expect(TokenType::RPAREN, "Expected ')'");
        catches.push_back({ std::move(ename), std::move(etype), parse_block() });
    }
    if (catches.empty())
        throw CompilerException("try block must have at least one catch clause", get_location());

    BlockPtr finally_body;
    if (accept(TokenType::FINALLY)) finally_body = parse_block();

    return std::make_unique<TryCatchStmt>(l, std::move(try_body), std::move(catches), std::move(finally_body));
}

ConstructPtr Parser::parse_throw_stmt() {
    SourceLocation l = get_location();
    auto e = parse_expression();
    expect_semicolon("throw");
    return std::make_unique<ThrowStmt>(l, std::move(e));
}

// ============================================================================
// JUMP STATEMENTS
// ============================================================================

ConstructPtr Parser::parse_return_stmt() {
    SourceLocation l = get_location();
    ExpressionPtr value;
    if (peek().type != TokenType::SEMICOLON && !is_at_end())
        value = parse_expression();
    expect_semicolon("return");
    return std::make_unique<ReturnStmt>(l, std::move(value));
}

ConstructPtr Parser::parse_break_stmt() {
    SourceLocation l = get_location();
    expect_semicolon("break");
    return std::make_unique<BreakStmt>(l);
}

ConstructPtr Parser::parse_continue_stmt() {
    SourceLocation l = get_location();
    expect_semicolon("continue");
    return std::make_unique<ContinueStmt>(l);
}

// ============================================================================
// MODULE SYSTEM
// ============================================================================

void Parser::parse_import_decl() {
    SourceLocation l = get_location();
    std::string path;

    if (accept(TokenType::IDENTIFIER)) path = peek().lexeme; // User
    else path = expect(TokenType::STRING_LITERAL, "Expected identifier or string").lexeme;

    imports_.emplace_back(l, std::move(path));
}

void Parser::parse_export_decl() {
    SourceLocation l = get_location();
    expect(TokenType::LBRACE, "Expected '{' after 'export'");
    std::vector<NamePtr> symbols;
    if (peek().type != TokenType::RBRACE) {
        do {
            symbols.emplace_back(parse_name());
        } while (accept(TokenType::COMMA));
    }
    expect(TokenType::RBRACE, "Expected '}'");
    exports_.emplace_back(l, std::move(symbols));
}

void Parser::parse_module_decl() {
    (void)expect(TokenType::IDENTIFIER, "Expected module name after 'module'");
    expect_semicolon("module declaration");
}

// ============================================================================
// ENTRY POINT
// ============================================================================

ParserResult Parser::parse() {
    ParserResult result;
    SourceLocation l = get_location();

    if (tokens_.empty() || peek().type == TokenType::EOF_TOKEN) {
        result.ast = std::make_unique<Block>(l, std::vector<ConstructPtr>{});
        return result;
    }

    std::vector<ConstructPtr> stmts;
    while (!is_at_end()) {
        if (peek().type == TokenType::SEMICOLON) {
            advance();
            continue;
        }
        try {
            auto stmt = parse_statement();
            if (stmt) stmts.push_back(std::move(stmt));
        } catch (const CompilerException& e) {
            report_and_synchronise(e);
        }
    }

    result.ast = std::make_unique<Block>(l, std::move(stmts));
    result.imports = std::move(imports_);
    result.exports = std::move(exports_);
    return result;
}

} // namespace xenon