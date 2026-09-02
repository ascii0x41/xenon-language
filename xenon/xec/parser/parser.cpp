#include "parser.h"

namespace xenon::parser {

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
        if (peek().type == type) { advance(); return true; }
        return false;
    }

    Token Parser::expect(TokenType type, const std::string& msg) {
        if (accept(type)) {
            return previous();
        }

        std::string error_msg = msg;
        error_msg += std::format(" (expected '{}', got '{}')",
                                token_type_to_string(type), escape_for_display(peek().lexeme));

        throw CompilerException(error_msg, peek().location, Severity::ERROR);
    }

    ModuleAST Parser::parse() {
        ModuleAST ast;
        parse_header(ast);
        while (!is_at_end()) {
            ast.root.declarations.push_back(parse_declaration());
        }
        return ast;
    }

    NamePtr Parser::parse_name() {
        if (!check(TokenType::IDENTIFIER)) {
            throw CompilerException("Expected identifier for name", peek().location, Severity::ERROR);
        }

        auto name = std::make_unique<Name>(loc(), peek().lexeme);
        advance();

        // Handle qualified names (e.g., Module::Submodule::Name)
        while (match(TokenType::COLON_COLON)) {
            if (!check(TokenType::IDENTIFIER)) {
                throw CompilerException("Expected identifier after '::' in qualified name", peek().location, Severity::ERROR);
            }
            auto next_name = std::make_unique<Name>(loc(), peek().lexeme);
            advance();
            next_name->next = std::move(name);
            name = std::move(next_name);
        }

        return name;
    }

    TypeExprPtr Parser::parse_type_expression() {
        SourceLocation l = loc();

        if (accept(TokenType::MUT)) {
            if (accept(TokenType::REF)) {
                return std::make_unique<ReferenceTypeExpr>(l, parse_type_expression(), true);
            } else if (accept(TokenType::PTR)) {
                return std::make_unique<PointerTypeExpr>(l, parse_type_expression(), true);
//            } else if (accept(TokenType::BOX)) {
//                return std::make_unique<PointerTypeExpr>(l, parse_type_expression(), true, true);
            } else {
                throw CompilerException(
                    "Expected 'ref', 'ptr', or 'box' after 'mut'", loc(), Severity::ERROR);
            }
        }

        if (accept(TokenType::REF)) {
            return std::make_unique<ReferenceTypeExpr>(l, parse_type_expression(), false);
        }

        if (accept(TokenType::PTR)) {
            return std::make_unique<PointerTypeExpr>(l, parse_type_expression(), false);
        }

//        if (accept(TokenType::BOX)) {
//            return std::make_unique<PointerTypeExpr>(l, parse_type_expression(), false, true);
//        }

        if (accept(TokenType::LBRACKET)) {
            auto element_type = parse_type_expression();
            ExpressionPtr size_expr = nullptr;
            if (accept(TokenType::SEMICOLON)) {
                size_expr = parse_expression();
            }
            return std::make_unique<ArrayTypeExpr>(l, std::move(element_type), std::move(size_expr));
        }

        auto name = parse_name();
        return std::make_unique<NamedTypeExpr>(l, std::move(name));
    }


    std::vector<ExpressionPtr> Parser::parse_arguments() {
        std::vector<ExpressionPtr> args;

        expect(TokenType::LPAREN, "Expected '(' to start argument list");

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
                bool mut = accept(TokenType::MUT);
                auto name = expect(TokenType::IDENTIFIER, "Expected parametr name in parameter list").lexeme;
                TypeExprPtr type_expr = nullptr;
                if (accept(TokenType::COLON)) {
                    type_expr = parse_type_expression();
                }
                params.push_back(std::make_unique<VariableDecl>(loc(), std::move(name), std::move(type_expr), nullptr, mut));
            } while (accept(TokenType::COMMA));
        }

        expect(TokenType::RPAREN, "Expected ')' after parameter list");
        return params;
    }


    OperatorKind token_to_binary_op(TokenType type) {
        switch (type) {
            case TokenType::PLUS: return OperatorKind::ADD;
            case TokenType::MINUS: return OperatorKind::SUBTRACT;
            case TokenType::STAR: return OperatorKind::MULTIPLY;
            case TokenType::SLASH: return OperatorKind::DIVIDE;
            case TokenType::PERCENT: return OperatorKind::MODULO;
            case TokenType::PLUS_EQ: return OperatorKind::ADD_ASSIGN;
            case TokenType::MINUS_EQ: return OperatorKind::SUBTRACT_ASSIGN;
            case TokenType::STAR_EQ: return OperatorKind::MULTIPLY_ASSIGN;
            case TokenType::SLASH_EQ: return OperatorKind::DIVIDE_ASSIGN;
            case TokenType::PERCENT_EQ: return OperatorKind::MODULO_ASSIGN;
            case TokenType::AMP: return OperatorKind::BITWISE_AND;
            case TokenType::PIPE: return OperatorKind::BITWISE_OR;
            case TokenType::CARET: return OperatorKind::BITWISE_XOR;
            case TokenType::LT_LT: return OperatorKind::SHIFT_LEFT;
            case TokenType::GT_GT: return OperatorKind::SHIFT_RIGHT;
            case TokenType::AMP_EQ: return OperatorKind::BITWISE_AND_ASSIGN;
            case TokenType::PIPE_EQ: return OperatorKind::BITWISE_OR_ASSIGN;
            case TokenType::CARET_EQ: return OperatorKind::BITWISE_XOR_ASSIGN;
            case TokenType::LT_LT_EQ: return OperatorKind::SHIFT_LEFT_ASSIGN;
            case TokenType::GT_GT_EQ: return OperatorKind::SHIFT_RIGHT_ASSIGN;
            case TokenType::EQ_EQ: return OperatorKind::EQUAL;
            case TokenType::BANG_EQ: return OperatorKind::NOT_EQUAL;
            case TokenType::LT: return OperatorKind::LESS_THAN;
            case TokenType::LTE: return OperatorKind::LESS_EQUAL;
            case TokenType::GT: return OperatorKind::GREATER_THAN;
            case TokenType::GTE: return OperatorKind::GREATER_EQUAL;
            case TokenType::EQ: return OperatorKind::ASSIGN;
            default:
                throw std::runtime_error("Invalid token type for binary operator");
        }
    }

    OperatorKind token_to_unary_op(TokenType type) {
        switch (type) {
            case TokenType::MINUS: return OperatorKind::NEGATE;
            case TokenType::BANG:
            case TokenType::TILDE: return OperatorKind::BITWISE_NOT;
            case TokenType::AMP: return OperatorKind::ADDRESS_OF;
            case TokenType::STAR: return OperatorKind::DEREFERENCE;
            default:
                throw std::runtime_error("Invalid token type for unary operator");
        }
    }


    ExpressionPtr Parser::parse_expression() { return parse_assignment(); }

    ExpressionPtr Parser::parse_assignment() {
        auto expr = parse_ternary();

        switch (peek().type) {
            case TokenType::EQ:
            case TokenType::PLUS_EQ:   case TokenType::MINUS_EQ:
            case TokenType::STAR_EQ:   case TokenType::SLASH_EQ:
            case TokenType::PERCENT_EQ:
            case TokenType::AMP_EQ:    case TokenType::PIPE_EQ:
            case TokenType::CARET_EQ:
            case TokenType::LT_LT_EQ:  case TokenType::GT_GT_EQ: {
                SourceLocation l = loc();
                OperatorKind op = token_to_binary_op(advance().type);
                auto right = parse_assignment();
                return std::make_unique<OperationExpr>(l, op, std::move(expr), std::move(right));
            }
            default: return expr;
        }
    }

    ExpressionPtr Parser::parse_ternary() {
        auto expr = parse_logical_or();
        if (accept(TokenType::QUESTION)) {
            SourceLocation l = loc();
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
            SourceLocation l = loc(); advance();
            expr = std::make_unique<OperationExpr>(l, OperatorKind::LOGICAL_OR, std::move(expr), parse_logical_and());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_logical_and() {
        auto expr = parse_bitwise_or();
        while (peek().type == TokenType::AND) {
            SourceLocation l = loc(); advance();
            expr = std::make_unique<OperationExpr>(l, OperatorKind::LOGICAL_AND, std::move(expr), parse_bitwise_or());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_bitwise_or() {
        auto expr = parse_bitwise_xor();
        while (peek().type == TokenType::PIPE) {
            SourceLocation l = loc(); advance();
            expr = std::make_unique<OperationExpr>(l, OperatorKind::BITWISE_OR, std::move(expr), parse_bitwise_xor());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_bitwise_xor() {
        auto expr = parse_bitwise_and();
        while (peek().type == TokenType::CARET) {
            SourceLocation l = loc(); advance();
            expr = std::make_unique<OperationExpr>(l, OperatorKind::BITWISE_XOR, std::move(expr), parse_bitwise_and());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_bitwise_and() {
        auto expr = parse_equality();
        while (peek().type == TokenType::AMP) {
            SourceLocation l = loc(); advance();
            expr = std::make_unique<OperationExpr>(l, OperatorKind::BITWISE_AND, std::move(expr), parse_equality());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_equality() {
        auto expr = parse_comparison();
        while (peek().type == TokenType::EQ_EQ || peek().type == TokenType::BANG_EQ) {
            SourceLocation l = loc();
            OperatorKind op = token_to_binary_op(advance().type);
            expr = std::make_unique<OperationExpr>(l, op, std::move(expr), parse_comparison());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_comparison() {
        auto expr = parse_shift();
        while (peek().type == TokenType::LT  || peek().type == TokenType::LTE ||
            peek().type == TokenType::GT  || peek().type == TokenType::GTE) {
            SourceLocation l = loc();
            OperatorKind op = token_to_binary_op(advance().type);
            expr = std::make_unique<OperationExpr>(l, op, std::move(expr), parse_shift());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_shift() {
        auto expr = parse_term();
        while (peek().type == TokenType::LT_LT || peek().type == TokenType::GT_GT) {
            SourceLocation l = loc();
            OperatorKind op = token_to_binary_op(advance().type);
            expr = std::make_unique<OperationExpr>(l, op, std::move(expr), parse_term());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_term() {
        auto expr = parse_factor();
        while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
            SourceLocation l = loc();
            OperatorKind op = token_to_binary_op(advance().type);
            expr = std::make_unique<OperationExpr>(l, op, std::move(expr), parse_factor());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_factor() {
        auto expr = parse_unary();
        while (peek().type == TokenType::STAR   ||
            peek().type == TokenType::SLASH  ||
            peek().type == TokenType::PERCENT) {
            SourceLocation l = loc();
            OperatorKind op = token_to_binary_op(advance().type);
            expr = std::make_unique<OperationExpr>(l, op, std::move(expr), parse_unary());
        }
        return expr;
    }

    ExpressionPtr Parser::parse_unary() {
        switch (peek().type) {
            case TokenType::MINUS:
            case TokenType::PLUS:
            case TokenType::BANG:
            case TokenType::TILDE:
            case TokenType::STAR:
            case TokenType::AMP: {
                SourceLocation l = loc();
                OperatorKind op = token_to_unary_op(advance().type);
                return std::make_unique<OperationExpr>(l, op, nullptr, parse_unary());
            }
            default:
                return parse_postfix();
        }
    }

    ExpressionPtr Parser::parse_postfix() {
        auto expr = parse_primary();

        while (true) {
            SourceLocation l = loc();

            if (peek().type == TokenType::DOT) {
                advance();
                auto member = parse_name();
                expr = std::make_unique<MemberAccessExpr>(l, std::move(expr), std::move(member));
            }
            else if (peek().type == TokenType::LPAREN) {
                auto args = parse_arguments();
                bool is_early_return = accept(TokenType::QUESTION);
                expr = std::make_unique<CallExpr>(l, std::move(expr), std::move(args), is_early_return);
            }
            else if (peek().type == TokenType::LBRACE && expr->kind == ASTNode::NodeKind::NAME) {
                auto struct_name = std::unique_ptr<Name>(
                    static_cast<Name*>(expr.release()));
                expr = parse_class_literal(l, std::move(struct_name));
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

    ExpressionPtr Parser::parse_primary() {
        SourceLocation l = loc();

        switch (peek().type) {
            case TokenType::INT_LITERAL: {
                std::string lexeme = std::string(advance().lexeme);
                NumericSuffix suffix = NumericSuffix::NONE;

                for (const auto& known_suffix : std::vector<std::string>(tokens::INTEGER_SUFFIXES.begin(), tokens::INTEGER_SUFFIXES.end())) {
                    if (lexeme.size() >= known_suffix.size() &&
                        lexeme.rfind(known_suffix) == lexeme.size() - known_suffix.size()) {
                        suffix = parse_integer_suffix(known_suffix);
                        std::cout << known_suffix << "@" << loc().format() << ": '" << lexeme << "'" << std::endl;
                        break;
                    }
                }

                return std::make_unique<LiteralInt>(l, std::move(lexeme), suffix);
            }

            case TokenType::FLOAT_LITERAL: {
                std::string lexeme = std::string(advance().lexeme);
                NumericSuffix suffix = NumericSuffix::NONE;

                for (const auto& known_suffix : tokens::FLOATING_POINT_SUFFIXES) {
                    if (lexeme.size() >= known_suffix.size() &&
                        lexeme.rfind(known_suffix) == lexeme.size() - known_suffix.size()) {
                        suffix = parse_float_suffix(known_suffix);
                        break;
                    }
                }

                return std::make_unique<LiteralFloat>(l, std::move(lexeme), suffix);
            }

            case TokenType::STRING_LITERAL:
                return std::make_unique<LiteralString>(l, advance().lexeme);

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

            case TokenType::NEW:
                advance();
                return parse_new_expr();

//            case TokenType::BOX:
//                advance();
//                return parse_box_expr();

            case TokenType::IDENTIFIER:
                return parse_name();

            case TokenType::LPAREN: {
                advance();
                auto expr = parse_expression();
                expect(TokenType::RPAREN, "Expected ')' to close parenthesized expression");
                return expr;
            }
            default:
                throw CompilerException(
                    std::format("Unexpected token '{}' in expression", escape_for_display(peek().lexeme)),
                    peek().location, Severity::ERROR);
        }
    }

    ExpressionPtr Parser::parse_array_literal() {
        SourceLocation l = loc();
        expect(TokenType::LBRACKET, "Expected '['");
        std::vector<ExpressionPtr> elems;
        if (peek().type != TokenType::RBRACKET)
            do { elems.push_back(parse_expression()); } while (accept(TokenType::COMMA));
        expect(TokenType::RBRACKET, "Expected ']' to close array literal");
        return std::make_unique<LiteralArray>(l, std::move(elems));
    }

    ExpressionPtr Parser::parse_new_expr() {
        SourceLocation l = loc();
        auto alloc = parse_primary();
        return std::make_unique<NewExpr>(l, std::move(alloc));
    }

    ExpressionPtr Parser::parse_class_literal(SourceLocation l, NamePtr struct_name) {
        expect(TokenType::LBRACE, "Expected '{' to start class literal");
        std::vector<ExpressionPtr> args;
        if (!check(TokenType::RBRACE)) {
            do {
                args.push_back(parse_expression());
            } while (accept(TokenType::COMMA));
        }
        expect(TokenType::RBRACE, "Expected '}' to close class literal");
        return std::make_unique<LiteralClass>(l, std::move(struct_name), std::move(args));
    }

    /*
    ExpressionPtr Parser::parse_box_expr() {
        SourceLocation l = loc();
        auto alloc_type = parse_type_expression();
        std::vector<ExpressionPtr> args;
        if (check(TokenType::LPAREN)) {
            args = parse_arguments();
        }
        return std::make_unique<BoxExpr>(l, std::move(alloc_type), std::move(args));
    }
    */

    DeclarationPtr Parser::parse_declaration() {
        bool is_public = false;
        if (match(TokenType::PUB)) is_public = true;
        if (check(TokenType::LET)) {
            return parse_variable_declaration(is_public);
        }
        if (check(TokenType::FUNC))  {
            return parse_function_declaration(is_public);
        }
        if (check(TokenType::CLASS)) {
            return parse_class_structure_declaration(is_public);
        }
        if (check(TokenType::IMPL)) {
            return parse_class_implementation_declaration();
        }
        throw CompilerException("Expected declaration", peek().location, Severity::ERROR);
    }

    StatementPtr Parser::parse_statement() {

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
            SourceLocation l = loc();
            ExpressionPtr ret_expr = nullptr;
            if (!check(TokenType::SEMICOLON)) {
                ret_expr = parse_expression();
            }
            expect(TokenType::SEMICOLON, "Expected ';' after return statement");
            return std::make_unique<ReturnStmt>(l, std::move(ret_expr));
        }

        if (match(TokenType::BREAK)) {
            SourceLocation l = loc();
            expect(TokenType::SEMICOLON, "Expected ';' after break statement");
            return std::make_unique<BreakStmt>(l);
        }

        if (match(TokenType::CONTINUE)) {
            SourceLocation l = loc();
            expect(TokenType::SEMICOLON, "Expected ';' after continue statement");
            return std::make_unique<ContinueStmt>(l);
        }

        // Fallback to expression statement
        auto expr = parse_expression();
        expect(TokenType::SEMICOLON, "Expected ';' after expression statement");
        return std::make_unique<ExpressionStmt>(loc(), std::move(expr));
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
        auto condition = parse_expression();
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
        auto condition = parse_expression();
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
        TypeExprPtr return_type = nullptr;
        if (accept(TokenType::ARROW)) {
            return_type = parse_type_expression();
        } else {
            return_type = std::make_unique<NamedTypeExpr>(std::move(loc()), std::make_unique<Name>(std::move(loc()), "void"));
        }
        BlockPtr body = nullptr;
        if (check(TokenType::LBRACE)) body = parse_block();
        return std::make_unique<FunctionDecl>(l, std::move(name), std::move(params), std::move(return_type), std::move(body), is_public);
    }

    // operator<op>(params...) -> ret_t { ... }
    OperatorOverloadDeclPtr Parser::parse_operator_overload_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::OPERATOR, "Expected 'operator' keyword");
        std::string op_lexeme;
        while (!check(TokenType::LPAREN) && !is_at_end()) {
            op_lexeme += advance().lexeme;
        }
        if (is_at_end() && !check(TokenType::LPAREN)) {
            throw CompilerException("Expected '(' after operator", loc());
        }
        auto params = parse_parameters();
        if (params.size() == 0) throw CompilerException("Too little arguments in operator overload", loc(), Severity::ERROR);
        if (params.size() > 2) throw CompilerException("Too many arguments in operator overload", loc(), Severity::ERROR);

        TypeExprPtr return_type = nullptr;
        if (accept(TokenType::ARROW)) {
            return_type = parse_type_expression();
        } else {
            return_type = std::make_unique<NamedTypeExpr>(std::move(loc()), std::make_unique<Name>(std::move(loc()), "void"));
        }
        BlockPtr body = nullptr;
        if (check(TokenType::LBRACE)) body = parse_block();

        std::optional<OperatorKind> op = lookup_operator_overload(op_lexeme, params.size() == 2);
        if (!op.has_value()) throw CompilerException(
            std::format("Invalid {} operator '{}'", (params.size() == 2 ? "binary" : "unary"), op_lexeme),
            l, Severity::ERROR);
        return std::make_unique<OperatorOverloadDecl>(l, op.value(), std::move(params), std::move(return_type), std::move(body), is_public);
    }

    // <name>: <type_expr>;
    ClassFieldDeclPtr Parser::parse_class_field_declaration(bool is_public) {
        SourceLocation l = loc();
        auto name = expect(TokenType::IDENTIFIER, "Expected class field name").lexeme;

        expect(TokenType::COLON, "Expected type expression for class field declaration");
        TypeExprPtr type_expr = parse_type_expression();
        expect(TokenType::SEMICOLON, "Expected ';' after class field declaration");
        return std::make_unique<ClassFieldDecl>(l, std::move(name), std::move(type_expr), is_public);
    }

    NamespaceVarDeclPtr Parser::parse_namespace_var_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::LET, "Expected 'let' keyword for namespace variable declaration");
        bool is_mutable = accept(TokenType::MUT);
        auto name = expect(TokenType::IDENTIFIER, "Expected namespace variable name after 'let'").lexeme;
        TypeExprPtr type_expr = nullptr;
        ExpressionPtr init_expr = nullptr;

        if (accept(TokenType::COLON)) {
            type_expr = parse_type_expression();
        }

        if (accept(TokenType::EQ)) {
            init_expr = parse_expression();
        }

        expect(TokenType::SEMICOLON, "Expected ';' after namespace variable declaration");
        return std::make_unique<NamespaceVarDecl>(
            l, std::move(name), std::move(type_expr), std::move(init_expr), is_mutable, is_public);
    }

    ClassMethodDeclPtr Parser::parse_class_method_declaration(bool is_public, bool is_static) {
        SourceLocation l = loc();
        expect(TokenType::FUNC, "Expected 'func' keyword");
        auto name = expect(TokenType::IDENTIFIER, "Expected method name after 'func'").lexeme;
        auto params = parse_parameters();
        TypeExprPtr return_type = nullptr;
        if (accept(TokenType::ARROW)) {
            return_type = parse_type_expression();
        } else {
            return_type = std::make_unique<NamedTypeExpr>(std::move(loc()), std::make_unique<Name>(std::move(loc()), "void"));
        }
        BlockPtr body = nullptr;
        if (check(TokenType::LBRACE)) body = parse_block();
        return std::make_unique<ClassMethodDecl>(l, std::move(name), std::move(params), std::move(return_type), std::move(body), is_public, is_static);
    }

    ClassStructureDeclPtr Parser::parse_class_structure_declaration(bool is_public) {
        SourceLocation l = loc();
        expect(TokenType::CLASS, "Expected 'class' keyword");
        auto name = expect(TokenType::IDENTIFIER, "Expected class name after 'class'").lexeme;

        expect(TokenType::LBRACE, "Expected '{' to start class body");

        std::vector<ClassFieldDeclPtr> fields;

        while (!check(TokenType::RBRACE) && !is_at_end()) {
            bool field_public = false;
            if (match(TokenType::PUB)) field_public = true;

            if (!check(TokenType::IDENTIFIER)) {
                throw CompilerException("Expected class field declaration", peek().location, Severity::ERROR);
            }

            fields.push_back(parse_class_field_declaration(field_public));
        }

        expect(TokenType::RBRACE, "Expected '}' to close class declaration");
        return std::make_unique<ClassStructureDecl>(l, std::move(name), std::move(fields), is_public);
    }

    ClassImplementationDeclPtr Parser::parse_class_implementation_declaration() {
        SourceLocation l = loc();
        expect(TokenType::IMPL, "Expected 'impl' keyword");
        auto class_name = parse_name();

        expect(TokenType::LBRACE, "Expected '{' to start impl block");

        std::vector<NamespaceVarDeclPtr> static_vars;
        std::vector<ClassMethodDeclPtr> methods;
        std::vector<OperatorOverloadDeclPtr> operator_overloads;
        std::vector<ClassMethodDeclPtr> constructors;
        ClassMethodDeclPtr destructor = nullptr;

        while (!check(TokenType::RBRACE) && !is_at_end()) {
            bool member_public = false;
            if (match(TokenType::PUB)) member_public = true;

            if (check(TokenType::OPERATOR)) {
                operator_overloads.push_back(parse_operator_overload_declaration(member_public));
                continue;
            }

            bool is_static = match(TokenType::STATIC);

            if (check(TokenType::LET)) {
                if (!is_static) {
                    throw CompilerException(
                        "Expected 'static' before namespace variable declaration in impl block",
                        peek().location, Severity::ERROR);
                }
                static_vars.push_back(parse_namespace_var_declaration(member_public));
                continue;
            }

            if (check(TokenType::FUNC)) {
                auto method = parse_class_method_declaration(member_public, is_static);
                if (method->name == "drop") {
                    if (is_static) {
                        throw CompilerException("Static destructor is not allowed", peek().location, Severity::ERROR);
                    }
                    destructor = std::move(method);
                } else {
                    methods.push_back(std::move(method));
                }
                continue;
            }

            throw CompilerException("Expected impl member declaration", peek().location, Severity::ERROR);
        }

        expect(TokenType::RBRACE, "Expected '}' to close impl block");
        return std::make_unique<ClassImplementationDecl>(
            l, std::move(class_name), std::move(static_vars), std::move(methods),
            std::move(operator_overloads), std::move(constructors), std::move(destructor));
    }


    std::string Parser::parse_module_name() {
        expect(TokenType::MODULE, "Expected 'module' keyword");
        auto module_name = (*parse_name()).to_string();
        expect(TokenType::SEMICOLON, "Expected ';' after module name declaration");
        return module_name;
    }

    std::vector<std::string> Parser::parse_dependencies() {
        std::vector<std::string> dependencies;
        while (match(TokenType::IMPORT)) {
            auto dep_name = parse_name()->to_string();
            expect(TokenType::SEMICOLON, "Expected ';' after import declaration");
            dependencies.push_back(dep_name);
        }
        return dependencies;
    }

    void Parser::parse_header(ModuleAST& ast) {
        while (!is_at_end()) {
            if (check(TokenType::MODULE)) {
                ast.module_name = parse_module_name();
            } else if (check(TokenType::IMPORT)) {
                auto deps = parse_dependencies();
                ast.dependencies.insert(ast.dependencies.end(),
                                        std::make_move_iterator(deps.begin()),
                                        std::make_move_iterator(deps.end()));
            } else {
                break;  // End of header section
            }
        }
    }
}