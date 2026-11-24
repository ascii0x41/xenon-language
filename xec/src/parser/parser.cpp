#include "parser.h"
#include "ast/header.h"
#include "common/diagnostics/diagnostic.h"
#include "common/source/span.h"
#include "common/source/source_file_manager.h"
#include "lexer/token.h"
#include "lexer/token_kind.h"
#include "parser/parser_result.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace xenon::parser {
namespace {

    std::string_view token_text(const lexer::Token& token) {
        return source::g_source_file_manager.get(token.span.file).slice(token.span);
    }

    void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::string decode_string_token(std::string_view raw) {
        const bool is_raw = raw.starts_with('r');
        const std::size_t quote = is_raw ? raw.find('"') : 0;
        const std::size_t content_begin = quote + 1;
        const std::size_t content_end = raw.rfind('"');
        const std::string_view content = raw.substr(content_begin, content_end - content_begin);
        if (is_raw) return std::string(content);

        std::string value;
        for (std::size_t i = 0; i < content.size(); ++i) {
            if (content[i] != '\\') {
                value.push_back(content[i]);
                continue;
            }
            const char escape = content[++i];
            switch (escape) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case '0': value.push_back('\0'); break;
                case '\\': value.push_back('\\'); break;
                case '\'': value.push_back('\''); break;
                case '"': value.push_back('"'); break;
                case 'u': {
                    ++i;
                    if (content[i] == '{') ++i;
                    std::uint32_t codepoint = 0;
                    while (i < content.size() && content[i] != '}') {
                        const char digit = content[i++];
                        codepoint <<= 4;
                        codepoint |= static_cast<std::uint32_t>(
                            digit <= '9' ? digit - '0'
                            : (digit | 0x20) - 'a' + 10);
                    }
                    append_utf8(value, codepoint);
                    break;
                }
                default: value.push_back(escape); break;
            }
        }
        return value;
    }

    std::uint32_t first_codepoint(std::string_view text) {
        const auto first = static_cast<unsigned char>(text[0]);
        if (first < 0x80) return first;
        std::uint32_t value = first & (first < 0xE0 ? 0x1F : first < 0xF0 ? 0x0F : 0x07);
        const std::size_t count = first < 0xE0 ? 1 : first < 0xF0 ? 2 : 3;
        for (std::size_t i = 1; i <= count; ++i) {
            value = (value << 6) | (static_cast<unsigned char>(text[i]) & 0x3F);
        }
        return value;
    }

    std::pair<std::string, std::string> split_numeric_suffix(std::string_view raw, bool is_float) {
        const bool prefixed = raw.size() > 2 && raw[0] == '0'
            && (raw[1] == 'x' || raw[1] == 'b' || raw[1] == 'o');
        const std::size_t pos = raw.find_first_of(is_float ? "fd" : "ius", prefixed ? 2 : 0);
        if (pos == std::string_view::npos) return {std::string(raw), {}};
        return {std::string(raw.substr(0, pos)), std::string(raw.substr(pos))};
    }

} // namespace

    ParserOutput Parser::parse() {
        Output output;
        output.ast = std::make_unique<ast::AST>();
        if (token_stream_.empty()) {
            return output;
        }

        auto header = parse_header();
        if (!header) {
            return header.error();
        }

        output.header = std::move(header.value());
        output.ast->module_doc = std::move(module_doc_);

        diagnostics::DiagnosticCollector diagnostics;
        while (!is_at_eof()) {
            const std::size_t start_cursor = token_stream_cursor_;
            auto declaration = parse_declaration();
            if (declaration) {
                output.ast->add_statement(std::move(declaration.value()));
                continue;
            }

            for (const auto& diagnostic : declaration.diagnostics()) {
                diagnostics.report(diagnostic);
            }
            synchronize_declaration(start_cursor);
        }

        const bool success = !diagnostics.has_errors();
        return ParserOutput(std::move(output), std::move(diagnostics), success);
    }


    source::Span Parser::location() const {
        return peek().span;
    }
    bool Parser::is_at_eof() const {
        return token_stream_cursor_ >= token_stream_.size();
    }

    const lexer::Token& Parser::peek() const {
        return token_stream_cursor_ < token_stream_.size()
            ? token_stream_[token_stream_cursor_]
            : eof_token_;
    }
    const lexer::Token& Parser::peek_next() const {
        return token_stream_cursor_ + 1 < token_stream_.size()
            ? token_stream_[token_stream_cursor_ + 1]
            : eof_token_;
    }

    const lexer::Token& Parser::advance() {
        return token_stream_[token_stream_cursor_++];
    }
    const lexer::Token& Parser::previous() const {
        return token_stream_[token_stream_cursor_ - 1];
    }

    bool Parser::match(lexer::TokenKind kind) {
        if (check(kind)) {
            advance();
            return true;
        }

        return false;
    }
    bool Parser::check(lexer::TokenKind kind) const {
        if (is_at_eof()) return false;
        return peek().kind == kind;
    }
    bool Parser::accept(lexer::TokenKind kind) {
        return match(kind);
    }
    result::Result<lexer::Token> Parser::expect(lexer::TokenKind kind, const std::string &message) {
        if (accept(kind)) {
            return previous();
        }

        return diagnostics::Diagnostic{
            diagnostics::Severity::error,
            message + std::format(" (expected {}, got {})",
                lexer::escape_token_kind(kind),
                lexer::escape_token_kind(peek().kind)
            ),
            peek().span
        };
    }
    result::Result<lexer::Token> Parser::expect_semicolon() {
        return expect(
            lexer::TokenKind::SEMICOLON,
            "Expected ';' at end of statement"
        );
    }
    

    result::Result<ast::HeaderInfo> Parser::parse_header() {
        ast::HeaderInfo header_info;
        std::string module_doc;
        bool saw_header_declaration = false;
        while (true) {
            while (match(lexer::TokenKind::MODULE_DOC_COMMENT)) {
                if (saw_header_declaration) {
                    return diagnostics::Diagnostic{
                        diagnostics::Severity::error,
                        "Module documentation comments are only allowed at the top of a file",
                        previous().span
                    };
                }
                if (!module_doc.empty()) module_doc.push_back('\n');
                module_doc.append(token_text(previous()));
            }

            auto metadata_result = parse_metadata();
            if (!metadata_result) return metadata_result.error();
            Metadata metadata = std::move(metadata_result.value());

            if (check(lexer::TokenKind::MODULE)) {
                if (!metadata.attributes.empty()) {
                    return diagnostics::Diagnostic{
                        diagnostics::Severity::error,
                        "Attributes are not supported on module declarations",
                        metadata.attributes_span
                    };
                }
                if (header_info.module_decl) {
                    return diagnostics::Diagnostic{
                        diagnostics::Severity::error,
                        "Duplicate module declaration",
                        peek().span
                    };
                }

                const source::Span declaration_start = advance().span;
                auto module_path = parse_module_path();
                if (!module_path) return module_path.error();

                auto semi = expect_semicolon();
                if (!semi) return semi.error();

                header_info.module_decl.emplace(
                    source::Span{
                        .file = fid_,
                        .start = declaration_start.start,
                        .end = semi.value().span.end
                    },
                    std::move(module_path.value())
                );
                header_info.module_decl->doc = std::move(metadata.doc);
                saw_header_declaration = true;
            } else if (check(lexer::TokenKind::IMPORT)) {
                if (!metadata.attributes.empty()) {
                    return diagnostics::Diagnostic{
                        diagnostics::Severity::error,
                        "Attributes are not supported on import declarations",
                        metadata.attributes_span
                    };
                }
                const source::Span declaration_start = advance().span;
                auto module_path = parse_module_path();
                if (!module_path) return module_path.error();

                auto semi = expect_semicolon();
                if (!semi) return semi.error();

                header_info.imports.emplace_back(
                    source::Span{
                        .file = fid_,
                        .start = declaration_start.start,
                        .end = semi.value().span.end
                    },
                    std::move(module_path.value())
                );
                header_info.imports.back().doc = std::move(metadata.doc);
                saw_header_declaration = true;
            } else {
                pending_metadata_ = std::move(metadata);
                break;
            }
        }

        module_doc_ = std::move(module_doc);
        return header_info;
    }

    result::Result<ast::NamePtr> Parser::parse_module_path() {
        if (!check(lexer::TokenKind::IDENTIFIER)) {
            return diagnostics::Diagnostic{
                diagnostics::Severity::error,
                "Expected a module name",
                peek().span
            };
        }

        const auto first = advance();
        auto last = first;
        std::vector<std::string_view> components{
            source::g_source_file_manager.get(fid_).slice(first.span)
        };

        while (match(lexer::TokenKind::COLON_COLON)) {
            if (!check(lexer::TokenKind::IDENTIFIER)) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Expected a module name segment after '::'",
                    peek().span
                };
            }

            last = advance();
            components.push_back(source::g_source_file_manager.get(fid_).slice(last.span));
        }

        return std::make_unique<ast::Name>(
            source::Span{
                .file = first.span.file,
                .start = first.span.start,
                .end = last.span.end
            },
            std::move(components)
        );
    }

    result::Result<ast::NamePtr> Parser::parse_name() {
        const bool is_global = match(lexer::TokenKind::COLON_COLON);
        if (!check(lexer::TokenKind::IDENTIFIER) && !check(lexer::TokenKind::SELF)) {
            return diagnostics::Diagnostic{
                diagnostics::Severity::error,
                is_global
                    ? "Expected an identifier after leading '::'"
                    : "Expected an identifier for name",
                peek().span
            };
        }
        if (is_global && check(lexer::TokenKind::SELF)) {
            return diagnostics::Diagnostic{
                diagnostics::Severity::error,
                "'self' cannot follow a leading '::'",
                peek().span
            };
        }

        const std::size_t start = is_global ? previous().span.start : peek().span.start;
        const lexer::Token& first = advance();
        lexer::Token last = first;
        std::vector<std::string_view> components{
            source::g_source_file_manager.get(fid_).slice(first.span)
        };

        while (match(lexer::TokenKind::COLON_COLON)) {
            if (!check(lexer::TokenKind::IDENTIFIER)) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Expected an identifier after '::' in qualified name",
                    peek().span
                };
            }
            last = advance();
            components.push_back(source::g_source_file_manager.get(fid_).slice(last.span));
        }

        return std::make_unique<ast::Name>(
            source::Span{.file = fid_, .start = start, .end = last.span.end},
            std::move(components),
            is_global
        );
    }

    result::Result<ast::TypeExprPtr> Parser::parse_type_expression() {
        if (match(lexer::TokenKind::STAR)) {
            const source::Span start = previous().span;
            const bool is_mut = match(lexer::TokenKind::MUT);
            auto element_type = parse_type_expression();
            if (!element_type) return element_type.error();
            const source::Span span{
                .file = fid_, .start = start.start,
                .end = element_type.value()->span_.end
            };
            return ast::TypeExprPtr(std::make_unique<ast::PointerTypeExpr>(
                span, std::move(element_type.value()), is_mut));
        }

        if (match(lexer::TokenKind::AMP)) {
            const source::Span start = previous().span;
            const bool is_mut = match(lexer::TokenKind::MUT);
            auto element_type = parse_type_expression();
            if (!element_type) return element_type.error();
            const source::Span span{
                .file = fid_, .start = start.start,
                .end = element_type.value()->span_.end
            };
            return ast::TypeExprPtr(std::make_unique<ast::ReferenceTypeExpr>(
                span, std::move(element_type.value()), is_mut));
        }

        if (match(lexer::TokenKind::LBRACKET)) {
            const source::Span start = previous().span;
            auto size_expr = parse_expression();
            if (!size_expr) return size_expr.error();
            auto close = expect(lexer::TokenKind::RBRACKET, "Expected ']' after array length");
            if (!close) return close.error();
            auto element_type = parse_type_expression();
            if (!element_type) return element_type.error();
            const source::Span span{
                .file = fid_, .start = start.start,
                .end = element_type.value()->span_.end
            };
            return ast::TypeExprPtr(std::make_unique<ast::ArrayTypeExpr>(
                span, std::move(element_type.value()), std::move(size_expr.value())));
        }

        auto name = parse_name();
        if (!name) return name.error();
        const source::Span span = name.value()->span_;
        return ast::TypeExprPtr(std::make_unique<ast::NamedTypeExpr>(
            span, std::move(name.value())));
    }

    result::Result<ast::ExpressionPtr> Parser::parse_expression() {
        return parse_assignment();
    }

    result::Result<ast::ExpressionPtr> Parser::parse_binary(
        result::Result<ast::ExpressionPtr> (Parser::*lower)(),
        std::initializer_list<std::pair<lexer::TokenKind, ast::BinaryOperatorKind>> operators
    ) {
        auto lhs = (this->*lower)();
        if (!lhs) return lhs.error();

        while (!is_at_eof()) {
            std::optional<ast::BinaryOperatorKind> op;
            for (const auto& [kind, candidate] : operators) {
                if (peek().kind == kind) {
                    op = candidate;
                    break;
                }
            }
            if (!op) break;

            advance();
            auto rhs = (this->*lower)();
            if (!rhs) return rhs.error();
            const source::Span span{
                .file = fid_, .start = lhs.value()->span_.start,
                .end = rhs.value()->span_.end
            };
            lhs = ast::ExpressionPtr(std::make_unique<ast::BinaryOpExpr>(
                span, *op, std::move(lhs.value()), std::move(rhs.value())));
        }
        return lhs;
    }

    result::Result<ast::ExpressionPtr> Parser::parse_assignment() {
        auto lhs = parse_ternary();
        if (!lhs) return lhs.error();
        if (!is_assignment_token(peek().kind)) return lhs;

        const lexer::TokenKind kind = advance().kind;
        ast::AssignmentOperatorKind op;
        switch (kind) {
            case lexer::TokenKind::EQ: op = ast::AssignmentOperatorKind::ASSIGN; break;
            case lexer::TokenKind::PLUS_EQ: op = ast::AssignmentOperatorKind::ADD_ASSIGN; break;
            case lexer::TokenKind::MINUS_EQ: op = ast::AssignmentOperatorKind::SUBTRACT_ASSIGN; break;
            case lexer::TokenKind::STAR_EQ: op = ast::AssignmentOperatorKind::MULTIPLY_ASSIGN; break;
            case lexer::TokenKind::SLASH_EQ: op = ast::AssignmentOperatorKind::DIVIDE_ASSIGN; break;
            case lexer::TokenKind::PERCENT_EQ: op = ast::AssignmentOperatorKind::MODULO_ASSIGN; break;
            case lexer::TokenKind::AMP_EQ: op = ast::AssignmentOperatorKind::BITWISE_AND_ASSIGN; break;
            case lexer::TokenKind::PIPE_EQ: op = ast::AssignmentOperatorKind::BITWISE_OR_ASSIGN; break;
            case lexer::TokenKind::CARET_EQ: op = ast::AssignmentOperatorKind::BITWISE_XOR_ASSIGN; break;
            case lexer::TokenKind::LT_LT_EQ: op = ast::AssignmentOperatorKind::SHIFT_LEFT_ASSIGN; break;
            case lexer::TokenKind::GT_GT_EQ: op = ast::AssignmentOperatorKind::SHIFT_RIGHT_ASSIGN; break;
            default: return diagnostics::Diagnostic{diagnostics::Severity::error,
                "Invalid assignment operator", previous().span};
        }

        auto rhs = parse_assignment();
        if (!rhs) return rhs.error();
        const source::Span span{
            .file = fid_, .start = lhs.value()->span_.start,
            .end = rhs.value()->span_.end
        };
        return ast::ExpressionPtr(std::make_unique<ast::AssignmentExpr>(
            span, op, std::move(lhs.value()), std::move(rhs.value())));
    }

    result::Result<ast::ExpressionPtr> Parser::parse_ternary() {
        auto condition = parse_logical_or();
        if (!condition) return condition.error();
        if (!match(lexer::TokenKind::QUESTION)) return condition;

        auto then_branch = parse_expression();
        if (!then_branch) return then_branch.error();
        auto colon = expect(lexer::TokenKind::COLON, "Expected ':' in conditional expression");
        if (!colon) return colon.error();
        auto else_branch = parse_expression();
        if (!else_branch) return else_branch.error();
        const source::Span span{
            .file = fid_, .start = condition.value()->span_.start,
            .end = else_branch.value()->span_.end
        };
        return ast::ExpressionPtr(std::make_unique<ast::TernaryExpr>(
            span, std::move(condition.value()), std::move(then_branch.value()),
            std::move(else_branch.value())));
    }

    result::Result<ast::ExpressionPtr> Parser::parse_logical_or() {
        return parse_binary(&Parser::parse_logical_and,
            {{lexer::TokenKind::OR, ast::BinaryOperatorKind::LOGICAL_OR}});
    }

    result::Result<ast::ExpressionPtr> Parser::parse_logical_and() {
        return parse_binary(&Parser::parse_bitwise_or,
            {{lexer::TokenKind::AND, ast::BinaryOperatorKind::LOGICAL_AND}});
    }

    result::Result<ast::ExpressionPtr> Parser::parse_bitwise_or() {
        return parse_binary(&Parser::parse_bitwise_xor,
            {{lexer::TokenKind::PIPE, ast::BinaryOperatorKind::BITWISE_OR}});
    }

    result::Result<ast::ExpressionPtr> Parser::parse_bitwise_xor() {
        return parse_binary(&Parser::parse_bitwise_and,
            {{lexer::TokenKind::CARET, ast::BinaryOperatorKind::BITWISE_XOR}});
    }

    result::Result<ast::ExpressionPtr> Parser::parse_bitwise_and() {
        return parse_binary(&Parser::parse_equality,
            {{lexer::TokenKind::AMP, ast::BinaryOperatorKind::BITWISE_AND}});
    }

    result::Result<ast::ExpressionPtr> Parser::parse_equality() {
        return parse_binary(&Parser::parse_comparison, {
            {lexer::TokenKind::EQ_EQ, ast::BinaryOperatorKind::EQUAL},
            {lexer::TokenKind::BANG_EQ, ast::BinaryOperatorKind::NOT_EQUAL}
        });
    }

    result::Result<ast::ExpressionPtr> Parser::parse_comparison() {
        return parse_binary(&Parser::parse_shift, {
            {lexer::TokenKind::LT, ast::BinaryOperatorKind::LESS_THAN},
            {lexer::TokenKind::LTE, ast::BinaryOperatorKind::LESS_EQUAL},
            {lexer::TokenKind::GT, ast::BinaryOperatorKind::GREATER_THAN},
            {lexer::TokenKind::GTE, ast::BinaryOperatorKind::GREATER_EQUAL}
        });
    }

    result::Result<ast::ExpressionPtr> Parser::parse_shift() {
        return parse_binary(&Parser::parse_term, {
            {lexer::TokenKind::LT_LT, ast::BinaryOperatorKind::SHIFT_LEFT},
            {lexer::TokenKind::GT_GT, ast::BinaryOperatorKind::SHIFT_RIGHT}
        });
    }

    result::Result<ast::ExpressionPtr> Parser::parse_term() {
        return parse_binary(&Parser::parse_factor, {
            {lexer::TokenKind::PLUS, ast::BinaryOperatorKind::ADD},
            {lexer::TokenKind::MINUS, ast::BinaryOperatorKind::SUBTRACT}
        });
    }

    result::Result<ast::ExpressionPtr> Parser::parse_factor() {
        return parse_binary(&Parser::parse_unary, {
            {lexer::TokenKind::STAR, ast::BinaryOperatorKind::MULTIPLY},
            {lexer::TokenKind::SLASH, ast::BinaryOperatorKind::DIVIDE},
            {lexer::TokenKind::PERCENT, ast::BinaryOperatorKind::MODULO}
        });
    }

    result::Result<ast::ExpressionPtr> Parser::parse_unary() {
        const source::Span start = peek().span;
        if (match(lexer::TokenKind::AMP)) {
            const bool is_raw = match(lexer::TokenKind::RAW);
            const bool is_mut = match(lexer::TokenKind::MUT);
            auto operand = parse_unary();
            if (!operand) return operand.error();
            const ast::UnaryOperatorKind op = is_raw
                ? (is_mut ? ast::UnaryOperatorKind::MUT_RAW_BORROW : ast::UnaryOperatorKind::RAW_BORROW)
                : (is_mut ? ast::UnaryOperatorKind::MUT_BORROW : ast::UnaryOperatorKind::BORROW);
            const source::Span span{.file = fid_, .start = start.start,
                .end = operand.value()->span_.end};
            return ast::ExpressionPtr(std::make_unique<ast::UnaryOpExpr>(
                span, op, std::move(operand.value())));
        }

        if (is_unary_token(peek().kind)) {
            const lexer::TokenKind kind = advance().kind;
            auto operand = parse_unary();
            if (!operand) return operand.error();
            ast::UnaryOperatorKind op;
            switch (kind) {
                case lexer::TokenKind::PLUS: op = ast::UnaryOperatorKind::PLUS; break;
                case lexer::TokenKind::MINUS: op = ast::UnaryOperatorKind::NEGATE; break;
                case lexer::TokenKind::BANG: op = ast::UnaryOperatorKind::LOGICAL_NOT; break;
                case lexer::TokenKind::TILDE: op = ast::UnaryOperatorKind::BITWISE_NOT; break;
                case lexer::TokenKind::STAR: op = ast::UnaryOperatorKind::DEREFERENCE; break;
                default: return diagnostics::Diagnostic{diagnostics::Severity::error,
                    "Invalid unary operator", previous().span};
            }
            const source::Span span{.file = fid_, .start = start.start,
                .end = operand.value()->span_.end};
            return ast::ExpressionPtr(std::make_unique<ast::UnaryOpExpr>(
                span, op, std::move(operand.value())));
        }
        return parse_postfix();
    }

    bool Parser::is_assignment_token(lexer::TokenKind kind) const {
        switch (kind) {
            case lexer::TokenKind::EQ:
            case lexer::TokenKind::PLUS_EQ:
            case lexer::TokenKind::MINUS_EQ:
            case lexer::TokenKind::STAR_EQ:
            case lexer::TokenKind::SLASH_EQ:
            case lexer::TokenKind::PERCENT_EQ:
            case lexer::TokenKind::AMP_EQ:
            case lexer::TokenKind::PIPE_EQ:
            case lexer::TokenKind::CARET_EQ:
            case lexer::TokenKind::LT_LT_EQ:
            case lexer::TokenKind::GT_GT_EQ:
                return true;
            default:
                return false;
        }
    }

    bool Parser::is_unary_token(lexer::TokenKind kind) const {
        return kind == lexer::TokenKind::PLUS || kind == lexer::TokenKind::MINUS
            || kind == lexer::TokenKind::BANG || kind == lexer::TokenKind::TILDE
            || kind == lexer::TokenKind::STAR;
    }

    result::Result<ast::ExpressionPtr> Parser::parse_postfix() {
        auto expression = parse_primary();
        if (!expression) return expression.error();

        while (!is_at_eof()) {
            if (match(lexer::TokenKind::DOT)) {
                auto member_token = expect(lexer::TokenKind::IDENTIFIER,
                    "Expected member name after '.'");
                if (!member_token) return member_token.error();
                ast::NamePtr member = std::make_unique<ast::Name>(
                    member_token.value().span,
                    std::vector<std::string_view>{token_text(member_token.value())});
                const source::Span span{
                    .file = fid_, .start = expression.value()->span_.start,
                    .end = member_token.value().span.end
                };
                expression = ast::ExpressionPtr(std::make_unique<ast::MemberAccessExpr>(
                    span, std::move(expression.value()), std::move(member)));
            } else if (check(lexer::TokenKind::LPAREN)) {
                auto arguments = parse_arguments();
                if (!arguments) return arguments.error();
                const source::Span span{
                    .file = fid_, .start = expression.value()->span_.start,
                    .end = previous().span.end
                };
                expression = ast::ExpressionPtr(std::make_unique<ast::CallExpr>(
                    span, std::move(expression.value()), std::move(arguments.value())));
            } else if (match(lexer::TokenKind::LBRACKET)) {
                const bool saved_no_struct_literal = no_struct_literal_;
                no_struct_literal_ = false;
                auto index = parse_expression();
                no_struct_literal_ = saved_no_struct_literal;
                if (!index) return index.error();
                auto close = expect(lexer::TokenKind::RBRACKET, "Expected ']' after index");
                if (!close) return close.error();
                const source::Span span{
                    .file = fid_, .start = expression.value()->span_.start,
                    .end = close.value().span.end
                };
                expression = ast::ExpressionPtr(std::make_unique<ast::IndexExpr>(
                    span, std::move(expression.value()), std::move(index.value())));
            } else {
                break;
            }
        }
        return expression;
    }

    result::Result<ast::ExpressionPtr> Parser::parse_primary() {
        const lexer::Token token = peek();
        const source::Span span = token.span;

        switch (token.kind) {
            case lexer::TokenKind::INT_LITERAL: {
                advance();
                auto [value, suffix] = split_numeric_suffix(token_text(token), false);
                return ast::ExpressionPtr(std::make_unique<ast::LiteralInt>(
                    span, std::move(value), ast::parse_integer_suffix(suffix)));
            }
            case lexer::TokenKind::FLOAT_LITERAL: {
                advance();
                auto [value, suffix] = split_numeric_suffix(token_text(token), true);
                return ast::ExpressionPtr(std::make_unique<ast::LiteralFloat>(
                    span, std::move(value), ast::parse_float_suffix(suffix)));
            }
            case lexer::TokenKind::STRING_LITERAL:
                advance();
                return ast::ExpressionPtr(std::make_unique<ast::LiteralString>(
                    span, decode_string_token(token_text(token))));
            case lexer::TokenKind::CHARACTER_LITERAL: {
                advance();
                std::string_view raw = token_text(token);
                std::string decoded;
                if (raw.size() >= 2 && raw[1] == '\\') {
                    const std::string escaped = decode_string_token(
                        std::string("\"") + std::string(raw.substr(2, raw.size() - 3)) + "\"");
                    decoded = escaped;
                } else {
                    decoded.assign(raw.substr(1, raw.size() - 2));
                }
                return ast::ExpressionPtr(std::make_unique<ast::LiteralChar>(
                    span, first_codepoint(decoded)));
            }
            case lexer::TokenKind::TRUE:
                advance();
                return ast::ExpressionPtr(std::make_unique<ast::LiteralBool>(span, true));
            case lexer::TokenKind::FALSE:
                advance();
                return ast::ExpressionPtr(std::make_unique<ast::LiteralBool>(span, false));
            case lexer::TokenKind::NULLPTR:
                advance();
                return ast::ExpressionPtr(std::make_unique<ast::LiteralNullptr>(span));
            case lexer::TokenKind::LBRACKET: {
                const std::size_t saved_cursor = token_stream_cursor_;
                auto type = parse_type_expression();
                if (type && !no_struct_literal_ && check(lexer::TokenKind::LBRACE)) {
                    auto* array_type = ast::as<ast::ArrayTypeExpr>(type.value().get());
                    if (array_type) {
                        auto elements = parse_braced_expressions("array literal");
                        if (!elements) return elements.error();
                        const std::size_t end = previous().span.end;
                        auto element_type = std::move(array_type->element_type);
                        auto size_expr = std::move(array_type->size_expr);
                        const source::Span array_span{
                            .file = fid_, .start = span.start, .end = end
                        };
                        return ast::ExpressionPtr(std::make_unique<ast::LiteralArray>(
                            array_span, std::move(elements.value()),
                            std::move(element_type), std::move(size_expr)));
                    }
                }
                token_stream_cursor_ = saved_cursor;
                return parse_array_literal();
            }
            case lexer::TokenKind::COLON_COLON:
            case lexer::TokenKind::IDENTIFIER:
            case lexer::TokenKind::SELF: {
                auto name = parse_name();
                if (!name) return name.error();
                if (!no_struct_literal_ && check(lexer::TokenKind::LBRACE)) {
                    const source::Span name_span = name.value()->span_;
                    ast::TypeExprPtr type = std::make_unique<ast::NamedTypeExpr>(
                        name_span, std::move(name.value()));
                    return parse_struct_literal(name_span, std::move(type));
                }
                return ast::ExpressionPtr(std::move(name.value()));
            }
            case lexer::TokenKind::LPAREN: {
                advance();
                const bool saved_no_struct_literal = no_struct_literal_;
                no_struct_literal_ = false;
                auto expression = parse_expression();
                no_struct_literal_ = saved_no_struct_literal;
                if (!expression) return expression.error();
                auto close = expect(lexer::TokenKind::RPAREN,
                    "Expected ')' to close parenthesized expression");
                if (!close) return close.error();
                expression.value()->span_.start = span.start;
                expression.value()->span_.end = close.value().span.end;
                return expression;
            }
            default:
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    std::format("Unexpected {} in expression", lexer::escape_token_kind(token.kind)),
                    span
                };
        }
    }

    result::Result<std::vector<ast::ExpressionPtr>> Parser::parse_arguments() {
        auto open = expect(lexer::TokenKind::LPAREN, "Expected '(' to start argument list");
        if (!open) return open.error();
        const bool saved_no_struct_literal = no_struct_literal_;
        no_struct_literal_ = false;
        std::vector<ast::ExpressionPtr> arguments;
        if (!check(lexer::TokenKind::RPAREN)) {
            do {
                auto argument = parse_expression();
                if (!argument) {
                    no_struct_literal_ = saved_no_struct_literal;
                    return argument.error();
                }
                arguments.push_back(std::move(argument.value()));
            } while (match(lexer::TokenKind::COMMA));
        }
        no_struct_literal_ = saved_no_struct_literal;
        auto close = expect(lexer::TokenKind::RPAREN, "Expected ')' after arguments");
        if (!close) return close.error();
        return arguments;
    }

    result::Result<std::vector<ast::ExpressionPtr>> Parser::parse_braced_expressions(
        const std::string& what
    ) {
        auto open = expect(lexer::TokenKind::LBRACE,
            std::format("Expected '{{' to start {}", what));
        if (!open) return open.error();
        const bool saved_no_struct_literal = no_struct_literal_;
        no_struct_literal_ = false;
        std::vector<ast::ExpressionPtr> elements;
        if (!check(lexer::TokenKind::RBRACE)) {
            do {
                auto element = parse_expression();
                if (!element) {
                    no_struct_literal_ = saved_no_struct_literal;
                    return element.error();
                }
                elements.push_back(std::move(element.value()));
            } while (match(lexer::TokenKind::COMMA));
        }
        no_struct_literal_ = saved_no_struct_literal;
        auto close = expect(lexer::TokenKind::RBRACE,
            std::format("Expected '}}' to close {}", what));
        if (!close) return close.error();
        return elements;
    }

    result::Result<ast::ExpressionPtr> Parser::parse_array_literal() {
        const source::Span start = peek().span;
        auto open = expect(lexer::TokenKind::LBRACKET, "Expected '[' to start array literal");
        if (!open) return open.error();
        const bool saved_no_struct_literal = no_struct_literal_;
        no_struct_literal_ = false;
        std::vector<ast::ExpressionPtr> elements;
        if (!check(lexer::TokenKind::RBRACKET)) {
            do {
                auto element = parse_expression();
                if (!element) {
                    no_struct_literal_ = saved_no_struct_literal;
                    return element.error();
                }
                elements.push_back(std::move(element.value()));
            } while (match(lexer::TokenKind::COMMA));
        }
        no_struct_literal_ = saved_no_struct_literal;
        auto close = expect(lexer::TokenKind::RBRACKET, "Expected ']' to close array literal");
        if (!close) return close.error();
        return ast::ExpressionPtr(std::make_unique<ast::LiteralArray>(
            source::Span{.file = fid_, .start = start.start, .end = close.value().span.end},
            std::move(elements)));
    }

    result::Result<ast::ExpressionPtr> Parser::parse_struct_literal(
        source::Span span,
        ast::TypeExprPtr type_expr
    ) {
        auto open = expect(lexer::TokenKind::LBRACE, "Expected '{' to start struct literal");
        if (!open) return open.error();
        const bool saved_no_struct_literal = no_struct_literal_;
        no_struct_literal_ = false;
        std::vector<ast::FieldInitializer> fields;
        bool is_named = false;
        if (!check(lexer::TokenKind::RBRACE)) {
            do {
                ast::FieldInitializer field;
                field.span = peek().span;
                const bool entry_named = check(lexer::TokenKind::IDENTIFIER)
                    && peek_next().kind == lexer::TokenKind::COLON;
                if (fields.empty()) {
                    is_named = entry_named;
                } else if (entry_named != is_named) {
                    no_struct_literal_ = saved_no_struct_literal;
                    return diagnostics::Diagnostic{
                        diagnostics::Severity::error,
                        "Cannot mix named and positional fields in a struct literal",
                        field.span
                    };
                }
                if (entry_named) {
                    const lexer::Token name = advance();
                    field.name = std::string(token_text(name));
                    advance();
                }
                auto value = parse_expression();
                if (!value) {
                    no_struct_literal_ = saved_no_struct_literal;
                    return value.error();
                }
                field.value = std::move(value.value());
                fields.push_back(std::move(field));
            } while (match(lexer::TokenKind::COMMA));
        }
        no_struct_literal_ = saved_no_struct_literal;
        auto close = expect(lexer::TokenKind::RBRACE, "Expected '}' to close struct literal");
        if (!close) return close.error();
        span.end = close.value().span.end;
        return ast::ExpressionPtr(std::make_unique<ast::LiteralStruct>(
            span, std::move(type_expr), std::move(fields), is_named));
    }

    result::Result<ast::DeclarationPtr> Parser::parse_declaration() {
        auto metadata_result = parse_metadata();
        if (!metadata_result) return metadata_result.error();
        Metadata metadata = std::move(metadata_result.value());

        const bool is_public = match(lexer::TokenKind::PUB);
        ast::DeclarationPtr declaration;

        if (check(lexer::TokenKind::LET)) {
            auto parsed = parse_variable_declaration(is_public);
            if (!parsed) return parsed.error();
            declaration = std::move(parsed.value());
        } else if (check(lexer::TokenKind::FUNC)) {
            auto parsed = parse_function_declaration(is_public);
            if (!parsed) return parsed.error();
            declaration = std::move(parsed.value());
        } else if (check(lexer::TokenKind::TYPE)) {
            auto parsed = parse_struct_declaration(is_public);
            if (!parsed) return parsed.error();
            declaration = std::move(parsed.value());
        } else if (check(lexer::TokenKind::IMPL)) {
            if (is_public) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "'impl' blocks cannot be public; mark their members public instead",
                    previous().span
                };
            }
            if (metadata.doc_span) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Documentation comments are not allowed on impl blocks",
                    *metadata.doc_span
                };
            }
            if (!metadata.attributes.empty()) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Attributes are not allowed on impl blocks",
                    metadata.attributes_span
                };
            }
            auto parsed = parse_impl_declaration();
            if (!parsed) return parsed.error();
            declaration = std::move(parsed.value());
        } else {
            if (metadata.doc_span) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Documentation comment is not followed by a declaration",
                    *metadata.doc_span
                };
            }
            if (metadata.attributes_span) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Attribute is not followed by a declaration",
                    *metadata.attributes_span
                };
            }
            return diagnostics::Diagnostic{
                diagnostics::Severity::error,
                is_public ? "Expected declaration after 'pub'" : "Expected declaration",
                peek().span
            };
        }

        declaration->doc = std::move(metadata.doc);
        declaration->attributes = std::move(metadata.attributes);
        return declaration;
    }

    result::Result<ast::VariableDeclPtr> Parser::parse_variable_declaration(bool is_public) {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::LET, "Expected 'let' keyword");
        if (!keyword) return keyword.error();
        const bool is_mut = match(lexer::TokenKind::MUT);
        auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected variable name");
        if (!name) return name.error();
        std::string variable_name(source::g_source_file_manager.get(fid_).slice(name.value().span));

        ast::TypeExprPtr type;
        if (match(lexer::TokenKind::COLON)) {
            auto parsed = parse_type_expression();
            if (!parsed) return parsed.error();
            type = std::move(parsed.value());
        }

        ast::ExpressionPtr initializer;
        if (match(lexer::TokenKind::EQ)) {
            auto parsed = parse_expression();
            if (!parsed) return parsed.error();
            initializer = std::move(parsed.value());
        }

        auto semi = expect_semicolon();
        if (!semi) return semi.error();
        return std::make_unique<ast::VariableDecl>(
            source::Span{.file = fid_, .start = start.start, .end = semi.value().span.end},
            std::move(variable_name), std::move(type), std::move(initializer), is_mut, is_public);
    }

    result::Result<std::vector<ast::VariableDeclPtr>> Parser::parse_parameters() {
        auto open = expect(lexer::TokenKind::LPAREN, "Expected '(' to start parameter list");
        if (!open) return open.error();
        std::vector<ast::VariableDeclPtr> parameters;

        if (!check(lexer::TokenKind::RPAREN)) {
            do {
                const source::Span start = peek().span;
                const bool is_mut = match(lexer::TokenKind::MUT);
                auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected parameter name");
                if (!name) return name.error();
                std::string parameter_name(
                    source::g_source_file_manager.get(fid_).slice(name.value().span));

                ast::TypeExprPtr type;
                if (match(lexer::TokenKind::COLON)) {
                    auto parsed = parse_type_expression();
                    if (!parsed) return parsed.error();
                    type = std::move(parsed.value());
                }
                const std::size_t end = type ? type->span_.end : name.value().span.end;
                parameters.push_back(std::make_unique<ast::VariableDecl>(
                    source::Span{.file = fid_, .start = start.start, .end = end},
                    std::move(parameter_name), std::move(type), nullptr, is_mut));
            } while (match(lexer::TokenKind::COMMA));
        }

        auto close = expect(lexer::TokenKind::RPAREN, "Expected ')' after parameter list");
        if (!close) return close.error();
        return parameters;
    }

    result::Result<ast::TypeExprPtr> Parser::parse_return_type() {
        if (match(lexer::TokenKind::ARROW)) return parse_type_expression();
        const source::Span span = peek().span;
        auto name = std::make_unique<ast::Name>(
            span, std::vector<std::string_view>{"void"});
        return ast::TypeExprPtr(std::make_unique<ast::NamedTypeExpr>(span, std::move(name)));
    }

    result::Result<ast::BlockPtr> Parser::parse_optional_body() {
        if (check(lexer::TokenKind::LBRACE)) return parse_block();
        auto semi = expect(lexer::TokenKind::SEMICOLON,
            "Expected '{' or ';' after function signature");
        if (!semi) return semi.error();
        return ast::BlockPtr{};
    }

    result::Result<ast::FunctionDeclPtr> Parser::parse_function_declaration(bool is_public) {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::FUNC, "Expected 'func' keyword");
        if (!keyword) return keyword.error();
        auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected function name");
        if (!name) return name.error();
        std::string function_name(source::g_source_file_manager.get(fid_).slice(name.value().span));
        auto parameters = parse_parameters();
        if (!parameters) return parameters.error();
        auto return_type = parse_return_type();
        if (!return_type) return return_type.error();
        auto body = parse_optional_body();
        if (!body) return body.error();
        const std::size_t end = body.value()
            ? body.value()->span_.end
            : previous().span.end;
        return std::make_unique<ast::FunctionDecl>(
            source::Span{.file = fid_, .start = start.start, .end = end},
            std::move(function_name), std::move(parameters.value()),
            std::move(return_type.value()), std::move(body.value()), is_public);
    }

    result::Result<ast::BlockPtr> Parser::parse_block() {
        const source::Span start = peek().span;
        auto open = expect(lexer::TokenKind::LBRACE, "Expected '{' to start block");
        if (!open) return open.error();
        std::vector<ast::StatementPtr> statements;
        while (!check(lexer::TokenKind::RBRACE) && !is_at_eof()) {
            auto statement = parse_statement();
            if (!statement) return statement.error();
            statements.push_back(std::move(statement.value()));
        }
        auto close = expect(lexer::TokenKind::RBRACE, "Expected '}' to close block");
        if (!close) return close.error();
        return std::make_unique<ast::BlockStmt>(
            source::Span{.file = fid_, .start = start.start, .end = close.value().span.end},
            std::move(statements));
    }

    result::Result<ast::StatementPtr> Parser::parse_statement() {
        const source::Span start = peek().span;
        if (check(lexer::TokenKind::LET)) {
            auto variable = parse_variable_declaration();
            if (!variable) return variable.error();
            return ast::StatementPtr(std::move(variable.value()));
        }
        if (check(lexer::TokenKind::IF)) return parse_if_statement();
        if (check(lexer::TokenKind::WHILE)) return parse_while_statement();

        if (match(lexer::TokenKind::RETURN)) {
            ast::ExpressionPtr value;
            if (!check(lexer::TokenKind::SEMICOLON)) {
                auto expression = parse_expression();
                if (!expression) return expression.error();
                value = std::move(expression.value());
            }
            auto semi = expect_semicolon();
            if (!semi) return semi.error();
            return ast::StatementPtr(std::make_unique<ast::ReturnStmt>(
                source::Span{.file = fid_, .start = start.start, .end = semi.value().span.end},
                std::move(value)));
        }
        if (match(lexer::TokenKind::BREAK)) {
            auto semi = expect_semicolon();
            if (!semi) return semi.error();
            return ast::StatementPtr(std::make_unique<ast::BreakStmt>(
                source::Span{.file = fid_, .start = start.start, .end = semi.value().span.end}));
        }
        if (match(lexer::TokenKind::CONTINUE)) {
            auto semi = expect_semicolon();
            if (!semi) return semi.error();
            return ast::StatementPtr(std::make_unique<ast::ContinueStmt>(
                source::Span{.file = fid_, .start = start.start, .end = semi.value().span.end}));
        }

        auto expression = parse_expression();
        if (!expression) return expression.error();
        auto semi = expect_semicolon();
        if (!semi) return semi.error();
        return ast::StatementPtr(std::make_unique<ast::ExpressionStmt>(
            source::Span{.file = fid_, .start = start.start, .end = semi.value().span.end},
            std::move(expression.value())));
    }

    result::Result<ast::StatementPtr> Parser::parse_if_statement() {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::IF, "Expected 'if' keyword");
        if (!keyword) return keyword.error();

        const bool saved_no_struct_literal = no_struct_literal_;
        no_struct_literal_ = true;
        auto condition = parse_expression();
        no_struct_literal_ = saved_no_struct_literal;
        if (!condition) return condition.error();

        auto then_branch = parse_block();
        if (!then_branch) return then_branch.error();
        ast::StatementPtr else_branch;
        if (match(lexer::TokenKind::ELSE)) {
            if (check(lexer::TokenKind::IF)) {
                auto nested = parse_if_statement();
                if (!nested) return nested.error();
                else_branch = std::move(nested.value());
            } else {
                auto block = parse_block();
                if (!block) return block.error();
                else_branch = std::move(block.value());
            }
        }

        const std::size_t end = else_branch
            ? else_branch->span_.end
            : then_branch.value()->span_.end;
        return ast::StatementPtr(std::make_unique<ast::IfStmt>(
            source::Span{.file = fid_, .start = start.start, .end = end},
            std::move(condition.value()), std::move(then_branch.value()),
            std::move(else_branch)));
    }

    result::Result<ast::StatementPtr> Parser::parse_while_statement() {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::WHILE, "Expected 'while' keyword");
        if (!keyword) return keyword.error();

        const bool saved_no_struct_literal = no_struct_literal_;
        no_struct_literal_ = true;
        auto condition = parse_expression();
        no_struct_literal_ = saved_no_struct_literal;
        if (!condition) return condition.error();

        auto body = parse_block();
        if (!body) return body.error();
        return ast::StatementPtr(std::make_unique<ast::WhileStmt>(
            source::Span{.file = fid_, .start = start.start, .end = body.value()->span_.end},
            std::move(condition.value()), std::move(body.value())));
    }

    result::Result<ast::StructFieldDeclPtr> Parser::parse_struct_field_declaration(bool is_public) {
        const source::Span start = peek().span;
        auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected struct field name");
        if (!name) return name.error();
        auto colon = expect(lexer::TokenKind::COLON, "Expected ':' after struct field name");
        if (!colon) return colon.error();
        auto type = parse_type_expression();
        if (!type) return type.error();
        auto semi = expect_semicolon();
        if (!semi) return semi.error();
        return std::make_unique<ast::StructFieldDecl>(
            source::Span{.file = fid_, .start = start.start, .end = semi.value().span.end},
            std::string(token_text(name.value())), std::move(type.value()), is_public);
    }

    result::Result<ast::StructDeclPtr> Parser::parse_struct_declaration(bool is_public) {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::TYPE, "Expected 'type' keyword");
        if (!keyword) return keyword.error();
        auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected type name");
        if (!name) return name.error();
        std::string type_name(token_text(name.value()));
        auto open = expect(lexer::TokenKind::LBRACE, "Expected '{' to start type declaration");
        if (!open) return open.error();

        std::vector<ast::StructFieldDeclPtr> fields;
        while (!check(lexer::TokenKind::RBRACE) && !is_at_eof()) {
            auto metadata_result = parse_metadata();
            if (!metadata_result) return metadata_result.error();
            Metadata metadata = std::move(metadata_result.value());
            if (check(lexer::TokenKind::RBRACE) || is_at_eof()) {
                if (metadata.doc_span) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "Documentation comment is not followed by a field", *metadata.doc_span};
                }
                if (metadata.attributes_span) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "Attribute is not followed by a field", *metadata.attributes_span};
                }
                break;
            }
            const bool field_public = match(lexer::TokenKind::PUB);
            auto field = parse_struct_field_declaration(field_public);
            if (!field) return field.error();
            field.value()->doc = std::move(metadata.doc);
            field.value()->attributes = std::move(metadata.attributes);
            fields.push_back(std::move(field.value()));
        }

        auto close = expect(lexer::TokenKind::RBRACE, "Expected '}' to close type declaration");
        if (!close) return close.error();
        return std::make_unique<ast::StructDecl>(
            source::Span{.file = fid_, .start = start.start, .end = close.value().span.end},
            std::move(type_name), std::move(fields), is_public);
    }

    result::Result<std::pair<std::optional<ast::SelfReceiver>,
        std::vector<ast::VariableDeclPtr>>>
    Parser::parse_method_parameters(const ast::Name*) {
        auto open = expect(lexer::TokenKind::LPAREN, "Expected '(' to start parameter list");
        if (!open) return open.error();
        std::optional<ast::SelfReceiver> receiver;
        std::vector<ast::VariableDeclPtr> parameters;

        if (!check(lexer::TokenKind::RPAREN)) {
            bool first = true;
            do {
                const source::Span start = peek().span;
                if (first && match(lexer::TokenKind::AMP)) {
                    const bool is_mut = match(lexer::TokenKind::MUT);
                    auto self = expect(lexer::TokenKind::SELF,
                        "Expected 'self' after '&' in method receiver");
                    if (!self) return self.error();
                    receiver = ast::SelfReceiver{ast::SelfReceiverType::REFERENCE, is_mut};
                    first = false;
                    continue;
                }
                if (first && check(lexer::TokenKind::MUT)
                    && peek_next().kind == lexer::TokenKind::SELF) {
                    advance();
                    advance();
                    receiver = ast::SelfReceiver{ast::SelfReceiverType::VALUE, true};
                    first = false;
                    continue;
                }
                if (first && check(lexer::TokenKind::SELF)
                    && peek_next().kind != lexer::TokenKind::COLON) {
                    advance();
                    receiver = ast::SelfReceiver{ast::SelfReceiverType::VALUE, false};
                    first = false;
                    continue;
                }

                const bool is_mut = match(lexer::TokenKind::MUT);
                auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected parameter name");
                if (!name) return name.error();
                ast::TypeExprPtr type;
                if (match(lexer::TokenKind::COLON)) {
                    auto parsed = parse_type_expression();
                    if (!parsed) return parsed.error();
                    type = std::move(parsed.value());
                }
                const std::size_t end = type ? type->span_.end : name.value().span.end;
                parameters.push_back(std::make_unique<ast::VariableDecl>(
                    source::Span{.file = fid_, .start = start.start, .end = end},
                    std::string(token_text(name.value())), std::move(type), nullptr, is_mut));
                first = false;
            } while (match(lexer::TokenKind::COMMA));
        }

        auto close = expect(lexer::TokenKind::RPAREN, "Expected ')' after parameter list");
        if (!close) return close.error();
        return std::make_pair(std::move(receiver), std::move(parameters));
    }

    result::Result<ast::MethodDeclPtr> Parser::parse_method_declaration(
        bool is_public,
        bool is_static
    ) {
        return parse_method_declaration(is_public, is_static, nullptr);
    }

    result::Result<ast::MethodDeclPtr> Parser::parse_method_declaration(
        bool is_public,
        bool is_static,
        const ast::Name* impl_type
    ) {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::FUNC, "Expected 'func' keyword");
        if (!keyword) return keyword.error();
        auto name = expect(lexer::TokenKind::IDENTIFIER, "Expected method name");
        if (!name) return name.error();
        std::string method_name(token_text(name.value()));
        auto parameters = parse_method_parameters(impl_type);
        if (!parameters) return parameters.error();
        auto return_type = parse_return_type();
        if (!return_type) return return_type.error();
        auto body = parse_optional_body();
        if (!body) return body.error();
        const std::size_t end = body.value() ? body.value()->span_.end : previous().span.end;
        const bool method_is_static = is_static || !parameters.value().first.has_value();
        return std::make_unique<ast::MethodDecl>(
            source::Span{.file = fid_, .start = start.start, .end = end},
            std::move(method_name), std::move(parameters.value().first),
            std::move(parameters.value().second), std::move(return_type.value()),
            std::move(body.value()), is_public, method_is_static);
    }

    result::Result<ast::ImplDeclPtr> Parser::parse_impl_declaration() {
        const source::Span start = peek().span;
        auto keyword = expect(lexer::TokenKind::IMPL, "Expected 'impl' keyword");
        if (!keyword) return keyword.error();
        auto type_name = parse_name();
        if (!type_name) return type_name.error();
        auto open = expect(lexer::TokenKind::LBRACE, "Expected '{' to start impl block");
        if (!open) return open.error();

        std::vector<ast::VariableDeclPtr> static_vars;
        std::vector<ast::MethodDeclPtr> methods;
        std::vector<std::string> method_names;
        while (!check(lexer::TokenKind::RBRACE) && !is_at_eof()) {
            auto metadata_result = parse_metadata();
            if (!metadata_result) return metadata_result.error();
            Metadata metadata = std::move(metadata_result.value());
            if (check(lexer::TokenKind::RBRACE) || is_at_eof()) {
                if (metadata.doc_span) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "Documentation comment is not followed by an impl member", *metadata.doc_span};
                }
                if (metadata.attributes_span) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "Attribute is not followed by an impl member", *metadata.attributes_span};
                }
                break;
            }
            const bool member_public = match(lexer::TokenKind::PUB);
            const bool is_static = match(lexer::TokenKind::STATIC);
            if (check(lexer::TokenKind::LET)) {
                if (!is_static) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "Expected 'static' before variable declaration in impl block", peek().span};
                }
                auto variable = parse_variable_declaration(member_public);
                if (!variable) return variable.error();
                variable.value()->doc = std::move(metadata.doc);
                variable.value()->attributes = std::move(metadata.attributes);
                static_vars.push_back(std::move(variable.value()));
                continue;
            }
            if (check(lexer::TokenKind::FUNC)) {
                if (is_static) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "Methods do not use 'static'; omit the receiver to make a method static",
                        previous().span};
                }
                auto method = parse_method_declaration(member_public, false, type_name.value().get());
                if (!method) return method.error();
                if (std::find(method_names.begin(), method_names.end(), method.value()->name)
                    != method_names.end()) {
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        std::format("Method '{}' is already declared", method.value()->name),
                        method.value()->span_};
                }
                method_names.push_back(method.value()->name);
                method.value()->doc = std::move(metadata.doc);
                method.value()->attributes = std::move(metadata.attributes);
                methods.push_back(std::move(method.value()));
                continue;
            }
            return diagnostics::Diagnostic{diagnostics::Severity::error,
                "Expected impl member declaration", peek().span};
        }

        auto close = expect(lexer::TokenKind::RBRACE, "Expected '}' to close impl block");
        if (!close) return close.error();
        return std::make_unique<ast::ImplDecl>(
            source::Span{.file = fid_, .start = start.start, .end = close.value().span.end},
            std::move(type_name.value()), std::move(static_vars), std::move(methods));
    }

    result::Result<Parser::Metadata> Parser::parse_metadata() {
        Metadata metadata = std::move(pending_metadata_);
        pending_metadata_ = Metadata{};

        while (true) {
            if (check(lexer::TokenKind::DOC_COMMENT)) {
                const source::Span span = advance().span;
                if (!metadata.doc_span) metadata.doc_span = span;
                std::string_view text = source::g_source_file_manager.get(fid_).slice(span);
                if (text.starts_with("///")) text.remove_prefix(3);
                if (text.starts_with(' ')) text.remove_prefix(1);
                if (!metadata.doc.empty()) metadata.doc.push_back('\n');
                metadata.doc.append(text);
            } else if (check(lexer::TokenKind::HASH)) {
                if (!metadata.attributes_span) metadata.attributes_span = peek().span;
                auto attributes = parse_attribute_group();
                if (!attributes) return attributes.error();
                auto parsed = std::move(attributes.value());
                metadata.attributes.insert(
                    metadata.attributes.end(),
                    std::make_move_iterator(parsed.begin()),
                    std::make_move_iterator(parsed.end())
                );
            } else if (check(lexer::TokenKind::MODULE_DOC_COMMENT)) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Module documentation comments are only allowed at the top of a file",
                    peek().span
                };
            } else {
                break;
            }
        }

        return metadata;
    }

    result::Result<std::vector<ast::Attribute>> Parser::parse_attribute_group() {
        auto hash = expect(lexer::TokenKind::HASH, "Expected '#' to start an attribute");
        if (!hash) return hash.error();
        auto open = expect(lexer::TokenKind::LBRACKET, "Expected '[' after '#' to start an attribute");
        if (!open) return open.error();

        std::vector<ast::Attribute> attributes;
        do {
            if (!check(lexer::TokenKind::IDENTIFIER)
                && !check(lexer::TokenKind::COLON_COLON)) {
                return diagnostics::Diagnostic{
                    diagnostics::Severity::error,
                    "Expected attribute name",
                    peek().span
                };
            }

            ast::Attribute attribute;
            attribute.source = peek().span;
            auto name = parse_name();
            if (!name) return name.error();
            attribute.name = std::move(name.value());

            if (check(lexer::TokenKind::LPAREN)) {
                auto arguments = parse_arguments();
                if (!arguments) return arguments.error();
                attribute.args = std::move(arguments.value());
            }
            attributes.push_back(std::move(attribute));
        } while (match(lexer::TokenKind::COMMA));

        auto close = expect(lexer::TokenKind::RBRACKET, "Expected ']' to close attribute group");
        if (!close) return close.error();
        return attributes;
    }

    void Parser::synchronize_declaration(std::size_t start_cursor) {
        if (token_stream_cursor_ == start_cursor && !is_at_eof()) advance();

        std::size_t nesting = 0;
        while (!is_at_eof()) {
            const lexer::TokenKind kind = peek().kind;
            if (nesting == 0 && token_stream_cursor_ != start_cursor
                && (kind == lexer::TokenKind::LET
                    || kind == lexer::TokenKind::FUNC
                    || kind == lexer::TokenKind::TYPE
                    || kind == lexer::TokenKind::IMPL
                    || kind == lexer::TokenKind::PUB
                    || kind == lexer::TokenKind::DOC_COMMENT
                    || kind == lexer::TokenKind::HASH)) {
                return;
            }

            if (kind == lexer::TokenKind::LBRACE
                || kind == lexer::TokenKind::LPAREN
                || kind == lexer::TokenKind::LBRACKET) {
                ++nesting;
            } else if ((kind == lexer::TokenKind::RBRACE
                        || kind == lexer::TokenKind::RPAREN
                        || kind == lexer::TokenKind::RBRACKET)
                       && nesting > 0) {
                --nesting;
            } else if (kind == lexer::TokenKind::SEMICOLON && nesting == 0) {
                advance();
                return;
            }
            advance();
        }
    }

}