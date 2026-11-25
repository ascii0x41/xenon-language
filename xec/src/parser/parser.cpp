#include "parser.h"

#include "common/diagnostics/diagnostic.h"
#include "common/source/source_file_manager.h"

#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>

namespace xenon::parser {
namespace {

constexpr std::size_t MAX_DEPTH = 256;

bool builtin_type(lexer::TokenKind kind) {
    return kind == lexer::TokenKind::BuiltinType;
}

bool is_assignment(lexer::TokenKind kind) {
    switch (kind) {
        case lexer::TokenKind::Eq:
        case lexer::TokenKind::PlusEq:
        case lexer::TokenKind::MinusEq:
        case lexer::TokenKind::StarEq:
        case lexer::TokenKind::SlashEq:
        case lexer::TokenKind::DoubleStarEq:
        case lexer::TokenKind::AmpEq:
        case lexer::TokenKind::PipeEq:
        case lexer::TokenKind::CaretEq:
        case lexer::TokenKind::LessLessEq:
        case lexer::TokenKind::GreaterGreaterEq:
            return true;
        default:
            return false;
    }
}

ast::AssignOp assignment_op(lexer::TokenKind kind) {
    switch (kind) {
        case lexer::TokenKind::PlusEq: return ast::AssignOp::Add;
        case lexer::TokenKind::MinusEq: return ast::AssignOp::Subtract;
        case lexer::TokenKind::StarEq: return ast::AssignOp::Multiply;
        case lexer::TokenKind::SlashEq: return ast::AssignOp::Divide;
        case lexer::TokenKind::DoubleStarEq: return ast::AssignOp::Power;
        case lexer::TokenKind::AmpEq: return ast::AssignOp::BitwiseAnd;
        case lexer::TokenKind::PipeEq: return ast::AssignOp::BitwiseOr;
        case lexer::TokenKind::CaretEq: return ast::AssignOp::BitwiseXor;
        case lexer::TokenKind::LessLessEq: return ast::AssignOp::ShiftLeft;
        case lexer::TokenKind::GreaterGreaterEq: return ast::AssignOp::ShiftRight;
        default: return ast::AssignOp::Assign;
    }
}

bool is_place(const ast::ExpressionPtr& expr) {
    if (!expr) return false;
    switch (expr->kind) {
        case ast::Kind::NameRef:
        case ast::Kind::FieldExpr:
        case ast::Kind::IndexExpr:
        case ast::Kind::ParenExpr:
            return true;
        default:
            return false;
    }
}

} // namespace

Parser::Parser(const lexer::TokenStream& tokens, source::FileID fid)
    : tokens_(tokens), fid_(fid),
      eof_(lexer::Token{lexer::TokenKind::Eof,
                        source::Span{fid, tokens.empty() ? 0 : tokens.back().span.end,
                                     tokens.empty() ? 0 : tokens.back().span.end},
                        {}, lexer::NumericBase::Decimal, {}, {}}) {}

ParserOutput Parser::parse(const lexer::TokenStream& tokens, source::FileID fid) {
    return Parser(tokens, fid).parse();
}

ParserOutput Parser::parse() {
    const source::Span file_span{fid_, 0,
        tokens_.empty() ? 0 : tokens_.back().span.end};
    ast::DocComments leading_docs;
    ast::NodePtr module_decl;
    std::vector<ast::NodePtr> imports;
    ast::ItemPtr ast_source;

    while (check(lexer::TokenKind::DocComment)) {
        const lexer::Token& doc = advance();
        leading_docs.push_back(ast::make_DocComment(doc.span));
    }

    if (check(lexer::TokenKind::Module)) {
        auto parsed = parse_module_decl();
        if (parsed) module_decl = std::move(parsed.value());
        else report(parsed.error().message.c_str(), parsed.error().span.value_or(peek().span));
    } else if (!leading_docs.empty()) {
        report("doc comment is not followed by module or item",
               leading_docs.front()->span);
    }

    while (check(lexer::TokenKind::Import)) {
        auto parsed = parse_import_decl();
        if (parsed) imports.push_back(std::move(parsed.value()));
        else { report(parsed.error().message.c_str(), parsed.error().span.value_or(peek().span)); break; }
    }

    std::vector<ast::ItemPtr> items;
    while (!at_eof()) {
        if (check(lexer::TokenKind::RBrace)) {
            report("unexpected '}'", peek().span);
            advance();
            continue;
        }
        auto item = parse_item();
        if (item) items.push_back(std::move(item.value()));
        else {
            report(item.error().message.c_str(), item.error().span.value_or(peek().span));
            synchronize_item();
        }
    }

    ast::SourceFilePtr source = ast::make_SourceFile(file_span, std::move(module_decl),
                                                     std::move(imports), std::move(items));
    if (!leading_docs.empty() && module_decl) {
        static_cast<ast::ModuleDecl&>(*module_decl).doc_comments = std::move(leading_docs);
    }

    Output out{std::move(source)};
    const bool success = !diagnostics_.has_errors();
    return ParserOutput(std::move(out), std::move(diagnostics_), success);
}

const lexer::Token& Parser::peek(std::size_t n) const {
    const std::size_t index = cursor_ + n;
    return index < tokens_.size() ? tokens_[index] : eof_;
}

const lexer::Token& Parser::advance() {
    if (cursor_ < tokens_.size()) return tokens_[cursor_++];
    return eof_;
}

source::Span Parser::previous_span() const {
    if (cursor_ == 0) return eof_.span;
    return cursor_ <= tokens_.size() ? tokens_[cursor_ - 1].span : eof_.span;
}

bool Parser::check(lexer::TokenKind kind) const { return peek().kind == kind; }
bool Parser::match(lexer::TokenKind kind) {
    if (!check(kind)) return false;
    advance();
    return true;
}

bool Parser::at_eof() const {
    return cursor_ >= tokens_.size() || peek().kind == lexer::TokenKind::Eof;
}

result::Result<lexer::Token> Parser::expect(lexer::TokenKind kind, const char* message) {
    if (check(kind)) return advance();
    return diagnostics::Diagnostic{diagnostics::Severity::error, message, peek().span};
}

std::string_view Parser::text(const lexer::Token& token) const {
    return source::g_source_file_manager.get(fid_).slice(token.span);
}

std::string_view Parser::ct_text(const lexer::Token& token) const {
    return token.digit_text;
}

source::Span Parser::span_from(source::Span start, source::Span end) const {
    return source::Span{fid_, start.start, end.end};
}

void Parser::report(const char* message, source::Span span) {
    diagnostics_.error(message, span);
}

void Parser::synchronize_statement() {
    while (!at_eof()) {
        if (check(lexer::TokenKind::Semicolon) || check(lexer::TokenKind::RBrace)) {
            if (check(lexer::TokenKind::Semicolon)) advance();
            return;
        }
        advance();
    }
}

void Parser::synchronize_item() {
    if (!at_eof() && check(lexer::TokenKind::Semicolon)) advance();
    while (!at_eof()) {
        switch (peek().kind) {
            case lexer::TokenKind::Func:
            case lexer::TokenKind::Type:
            case lexer::TokenKind::Impl:
            case lexer::TokenKind::Static:
            case lexer::TokenKind::Const:
            case lexer::TokenKind::Import:
            case lexer::TokenKind::Module:
            case lexer::TokenKind::Pub:
            case lexer::TokenKind::AttributeStart:
            case lexer::TokenKind::DocComment:
                return;
            default:
                advance();
        }
    }
}

result::Result<ast::PathPtr> Parser::parse_module_path() {
    if (!check(lexer::TokenKind::Identifier))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected a module name", peek().span};
    std::vector<ast::PathSegment> segments;
    source::Span first = advance().span;
    segments.push_back({ast::PathSegmentKind::Identifier, first});
    source::Span last = first;
    while (match(lexer::TokenKind::Dot)) {
        if (!check(lexer::TokenKind::Identifier))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected module name after '.'", peek().span};
        last = advance().span;
        segments.push_back({ast::PathSegmentKind::Identifier, last});
    }
    return ast::make_Path(span_from(first, last), std::move(segments));
}

result::Result<ast::PathPtr> Parser::parse_name() {
    if (!check(lexer::TokenKind::Identifier) && !check(lexer::TokenKind::CtIdent) &&
        !check(lexer::TokenKind::Self)) {
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected a name", peek().span};
    }
    std::vector<ast::PathSegment> segments;
    source::Span first = peek().span;
    source::Span last = first;
    bool first_segment = true;
    while (check(lexer::TokenKind::Identifier) && peek(1).kind == lexer::TokenKind::Dot) {
        last = advance().span;
        segments.push_back({ast::PathSegmentKind::Identifier, last});
        advance();
        first_segment = false;
    }
    if (check(lexer::TokenKind::Identifier)) {
        last = advance().span;
        segments.push_back({first_segment ? ast::PathSegmentKind::Identifier :
                                            ast::PathSegmentKind::Identifier, last});
    } else if (check(lexer::TokenKind::CtIdent)) {
        last = advance().span;
        segments.push_back({ast::PathSegmentKind::CompileTimeName, last});
    } else if (check(lexer::TokenKind::Self)) {
        last = advance().span;
        segments.push_back({ast::PathSegmentKind::Identifier, last});
    } else {
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected a name", peek().span};
    }
    return ast::make_Path(span_from(first, last), std::move(segments));
}

result::Result<ast::DeclNamePtr> Parser::parse_decl_name() {
    if (check(lexer::TokenKind::Identifier)) {
        const lexer::Token& token = advance();
        return ast::make_DeclName(token.span, ast::DeclNameKind::Identifier);
    }
    if (check(lexer::TokenKind::CtIdent)) {
        const lexer::Token& token = advance();
        return ast::make_DeclName(token.span, ast::DeclNameKind::CompileTimeName);
    }
    if (check(lexer::TokenKind::Underscore)) {
        const lexer::Token& token = advance();
        return ast::make_DeclName(token.span, ast::DeclNameKind::Wildcard);
    }
    return diagnostics::Diagnostic{diagnostics::Severity::error, "expected a declaration name", peek().span};
}

result::Result<ast::PathPtr> Parser::parse_type_path() {
    std::vector<ast::PathSegment> segments;
    source::Span first = peek().span;
    while (check(lexer::TokenKind::Identifier) && peek(1).kind == lexer::TokenKind::Dot) {
        segments.push_back({ast::PathSegmentKind::Identifier, advance().span});
        advance();
    }
    if (!check(lexer::TokenKind::PctIdent))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected a type name", peek().span};
    source::Span last = advance().span;
    segments.push_back({ast::PathSegmentKind::TypeName, last});
    return ast::make_Path(span_from(first, last), std::move(segments));
}

result::Result<ast::TypePtr> Parser::parse_type() {
    if (depth_++ >= MAX_DEPTH)
        return diagnostics::Diagnostic{diagnostics::Severity::error, "maximum parser nesting depth exceeded", peek().span};
    ast::TypePtr result;
    source::Span start = peek().span;
    if (builtin_type(peek().kind)) {
        const lexer::Token& token = advance();
        ast::BuiltinTypeKind kind;
        std::string_view name = text(token);
        if (name == "str") kind = ast::BuiltinTypeKind::Str;
        else if (name == "u8") kind = ast::BuiltinTypeKind::U8;
        else if (name == "u16") kind = ast::BuiltinTypeKind::U16;
        else if (name == "u32") kind = ast::BuiltinTypeKind::U32;
        else if (name == "u64") kind = ast::BuiltinTypeKind::U64;
        else if (name == "usize") kind = ast::BuiltinTypeKind::Usize;
        else if (name == "i8") kind = ast::BuiltinTypeKind::I8;
        else if (name == "i16") kind = ast::BuiltinTypeKind::I16;
        else if (name == "i32") kind = ast::BuiltinTypeKind::I32;
        else if (name == "i64") kind = ast::BuiltinTypeKind::I64;
        else if (name == "isize") kind = ast::BuiltinTypeKind::Isize;
        else if (name == "f32") kind = ast::BuiltinTypeKind::F32;
        else if (name == "f64") kind = ast::BuiltinTypeKind::F64;
        else if (name == "bool") kind = ast::BuiltinTypeKind::Bool;
        else if (name == "rune") kind = ast::BuiltinTypeKind::Rune;
        else kind = ast::BuiltinTypeKind::Char;
        result = ast::make_BuiltinType(token.span, kind);
    } else if (check(lexer::TokenKind::Amp)) {
        advance();
        bool mut = match(lexer::TokenKind::Mut);
        auto inner = parse_type();
        if (!inner) { --depth_; return inner.error(); }
        result = ast::make_RefType(span_from(start, inner.value()->span), mut, std::move(inner.value()));
    } else if (check(lexer::TokenKind::AmpAmp)) {
        advance();
        bool mut = match(lexer::TokenKind::Mut);
        auto inner = parse_type();
        if (!inner) { --depth_; return inner.error(); }
        ast::TypePtr inner_ref = ast::make_RefType(inner.value()->span, mut, std::move(inner.value()));
        result = ast::make_RefType(span_from(start, inner_ref->span), false, std::move(inner_ref));
    } else if (check(lexer::TokenKind::Star)) {
        advance();
        bool mut = match(lexer::TokenKind::Mut);
        auto inner = parse_type();
        if (!inner) { --depth_; return inner.error(); }
        result = ast::make_PtrType(span_from(start, inner.value()->span), mut, std::move(inner.value()));
    } else if (check(lexer::TokenKind::DoubleStar)) {
        advance();
        auto inner = parse_type();
        if (!inner) { --depth_; return inner.error(); }
        result = ast::make_PtrType(span_from(start, inner.value()->span), false, std::move(inner.value()));
    } else if (check(lexer::TokenKind::LBracket)) {
        advance();
        auto size = parse_expression();
        if (!size) { --depth_; return size.error(); }
        if (!match(lexer::TokenKind::RBracket))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ']' after array length", peek().span};
        auto elem = parse_type();
        if (!elem) { --depth_; return elem.error(); }
        result = ast::make_ArrayType(span_from(start, elem.value()->span), std::move(size.value()), std::move(elem.value()));
    } else if (check(lexer::TokenKind::Func)) {
        advance();
        source::Span func_start = start;
        std::vector<ast::TypePtr> params;
        ast::TypePtr ret;
        if (!match(lexer::TokenKind::LParen))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '(' after func type", peek().span};
        if (!check(lexer::TokenKind::RParen)) {
            do {
                auto param = parse_type();
                if (!param) { --depth_; return param.error(); }
                params.push_back(std::move(param.value()));
            } while (match(lexer::TokenKind::Comma));
        }
        if (!match(lexer::TokenKind::RParen))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ')' after func type parameters", peek().span};
        if (match(lexer::TokenKind::Arrow)) {
            auto parsed = parse_type();
            if (!parsed) { --depth_; return parsed.error(); }
            ret = std::move(parsed.value());
        }
        source::Span end = ret ? ret->span : previous_span();
        result = ast::make_FuncType(span_from(func_start, end), std::move(params), std::move(ret));
    } else if (check(lexer::TokenKind::PctIdent) || check(lexer::TokenKind::Identifier)) {
        auto path = parse_type_path();
        if (!path) { --depth_; return path.error(); }
        result = ast::make_NamedType(path.value()->span, std::move(path.value()));
    } else {
        --depth_;
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected a type", peek().span};
    }
    --depth_;
    return result;
}

result::Result<ast::ExpressionPtr> Parser::parse_expression() {
    if (depth_++ >= MAX_DEPTH)
        return diagnostics::Diagnostic{diagnostics::Severity::error, "maximum parser nesting depth exceeded", peek().span};
    auto result = parse_cond();
    --depth_;
    return result;
}

result::Result<ast::ExpressionPtr> Parser::parse_cond() {
    auto condition = parse_logical_or();
    if (!condition) return condition.error();
    if (!match(lexer::TokenKind::Question)) return condition;
    auto then_expr = parse_expression();
    if (!then_expr) return then_expr.error();
    if (!match(lexer::TokenKind::Colon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ':' in conditional expression", peek().span};
    auto else_expr = parse_cond();
    if (!else_expr) return else_expr.error();
    source::Span span = span_from(condition.value()->span, else_expr.value()->span);
    return ast::ExpressionPtr(ast::make_CondExpr(span, std::move(condition.value()), std::move(then_expr.value()), std::move(else_expr.value())));
}

static ast::BinaryOp binary_op(lexer::TokenKind kind) {
    switch (kind) {
        case lexer::TokenKind::PipePipe: return ast::BinaryOp::LogicalOr;
        case lexer::TokenKind::AmpAmp: return ast::BinaryOp::LogicalAnd;
        case lexer::TokenKind::Pipe: return ast::BinaryOp::BitwiseOr;
        case lexer::TokenKind::Caret: return ast::BinaryOp::BitwiseXor;
        case lexer::TokenKind::Amp: return ast::BinaryOp::BitwiseAnd;
        case lexer::TokenKind::LessLess: return ast::BinaryOp::ShiftLeft;
        case lexer::TokenKind::GreaterGreater: return ast::BinaryOp::ShiftRight;
        case lexer::TokenKind::Plus: return ast::BinaryOp::Add;
        case lexer::TokenKind::Minus: return ast::BinaryOp::Subtract;
        case lexer::TokenKind::Star: return ast::BinaryOp::Multiply;
        case lexer::TokenKind::Slash: return ast::BinaryOp::Divide;
        case lexer::TokenKind::EqEq: return ast::BinaryOp::Equal;
        case lexer::TokenKind::BangEq: return ast::BinaryOp::NotEqual;
        case lexer::TokenKind::Less: return ast::BinaryOp::Less;
        case lexer::TokenKind::LessEq: return ast::BinaryOp::LessEqual;
        case lexer::TokenKind::Greater: return ast::BinaryOp::Greater;
        default: return ast::BinaryOp::GreaterEqual;
    }
}

result::Result<ast::ExpressionPtr> Parser::parse_logical_or() {
    auto lhs = parse_logical_and();
    if (!lhs) return lhs.error();
    while (match(lexer::TokenKind::PipePipe)) {
        auto rhs = parse_logical_and();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span),
                                   ast::BinaryOp::LogicalOr, std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}

result::Result<ast::ExpressionPtr> Parser::parse_logical_and() {
    auto lhs = parse_comparison();
    if (!lhs) return lhs.error();
    while (match(lexer::TokenKind::AmpAmp)) {
        auto rhs = parse_comparison();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span),
                                   ast::BinaryOp::LogicalAnd, std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}

result::Result<ast::ExpressionPtr> Parser::parse_comparison() {
    auto lhs = parse_bitor();
    if (!lhs) return lhs.error();
    auto op = peek().kind;
    if (op == lexer::TokenKind::EqEq || op == lexer::TokenKind::BangEq ||
        op == lexer::TokenKind::Less || op == lexer::TokenKind::LessEq ||
        op == lexer::TokenKind::Greater || op == lexer::TokenKind::GreaterEq) {
        advance();
        auto rhs = parse_bitor();
        if (!rhs) return rhs.error();
        if (peek().kind == lexer::TokenKind::EqEq || peek().kind == lexer::TokenKind::BangEq ||
            peek().kind == lexer::TokenKind::Less || peek().kind == lexer::TokenKind::LessEq ||
            peek().kind == lexer::TokenKind::Greater || peek().kind == lexer::TokenKind::GreaterEq)
            return diagnostics::Diagnostic{diagnostics::Severity::error,
                "comparison operators are not associative", peek().span};
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span),
                                   binary_op(op), std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}

result::Result<ast::ExpressionPtr> Parser::parse_bitor() {
    auto lhs = parse_bitxor();
    if (!lhs) return lhs.error();
    while (match(lexer::TokenKind::Pipe)) {
        auto rhs = parse_bitxor();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span),
                                   ast::BinaryOp::BitwiseOr, std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}
result::Result<ast::ExpressionPtr> Parser::parse_bitxor() {
    auto lhs = parse_bitand();
    if (!lhs) return lhs.error();
    while (match(lexer::TokenKind::Caret)) {
        auto rhs = parse_bitand();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span),
                                   ast::BinaryOp::BitwiseXor, std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}
result::Result<ast::ExpressionPtr> Parser::parse_bitand() {
    auto lhs = parse_shift();
    if (!lhs) return lhs.error();
    while (match(lexer::TokenKind::Amp)) {
        auto rhs = parse_shift();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span),
                                   ast::BinaryOp::BitwiseAnd, std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}
result::Result<ast::ExpressionPtr> Parser::parse_shift() {
    auto lhs = parse_add();
    if (!lhs) return lhs.error();
    while (check(lexer::TokenKind::LessLess) || check(lexer::TokenKind::GreaterGreater)) {
        ast::BinaryOp op = check(lexer::TokenKind::LessLess) ? ast::BinaryOp::ShiftLeft : ast::BinaryOp::ShiftRight;
        advance();
        auto rhs = parse_add();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span), op,
                                   std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}
result::Result<ast::ExpressionPtr> Parser::parse_add() {
    auto lhs = parse_mul();
    if (!lhs) return lhs.error();
    while (check(lexer::TokenKind::Plus) || check(lexer::TokenKind::Minus)) {
        ast::BinaryOp op = check(lexer::TokenKind::Plus) ? ast::BinaryOp::Add : ast::BinaryOp::Subtract;
        advance();
        auto rhs = parse_mul();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span), op,
                                   std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}
result::Result<ast::ExpressionPtr> Parser::parse_mul() {
    auto lhs = parse_unary();
    if (!lhs) return lhs.error();
    while (check(lexer::TokenKind::Star) || check(lexer::TokenKind::Slash)) {
        ast::BinaryOp op = check(lexer::TokenKind::Star) ? ast::BinaryOp::Multiply : ast::BinaryOp::Divide;
        advance();
        auto rhs = parse_unary();
        if (!rhs) return rhs.error();
        lhs = ast::ExpressionPtr(ast::make_BinaryExpr(span_from(lhs.value()->span, rhs.value()->span), op,
                                   std::move(lhs.value()), std::move(rhs.value())));
    }
    return lhs;
}

result::Result<ast::ExpressionPtr> Parser::parse_unary() {
    source::Span start = peek().span;
    if (check(lexer::TokenKind::Minus) || check(lexer::TokenKind::Bang) || check(lexer::TokenKind::Tilde)) {
        ast::UnaryOp op = check(lexer::TokenKind::Minus) ? ast::UnaryOp::Negate :
                          check(lexer::TokenKind::Bang) ? ast::UnaryOp::LogicalNot : ast::UnaryOp::BitwiseNot;
        advance();
        auto operand = parse_unary();
        if (!operand) return operand.error();
        if (op == ast::UnaryOp::Negate && operand.value()->kind == ast::Kind::IntLit) {
            return ast::ExpressionPtr(ast::make_UnaryExpr(span_from(start, operand.value()->span), op, std::move(operand.value())));
        }
        return ast::ExpressionPtr(ast::make_UnaryExpr(span_from(start, operand.value()->span), op, std::move(operand.value())));
    }
    return parse_power();
}

result::Result<ast::ExpressionPtr> Parser::parse_power() {
    auto left = parse_postfix();
    if (!left) return left.error();
    if (match(lexer::TokenKind::DoubleStar)) {
        auto right = parse_unary();
        if (!right) return right.error();
        return ast::ExpressionPtr(ast::make_BinaryExpr(span_from(left.value()->span, right.value()->span),
                                    ast::BinaryOp::Power, std::move(left.value()), std::move(right.value())));
    }
    return left;
}

result::Result<ast::ExpressionPtr> Parser::parse_postfix() {
    auto expr = parse_primary();
    if (!expr) return expr.error();
    while (!at_eof()) {
        if (check(lexer::TokenKind::LParen)) {
            auto args = parse_argument_list();
            if (!args) return args.error();
            expr = ast::ExpressionPtr(ast::make_CallExpr(span_from(expr.value()->span, previous_span()),
                                      std::move(expr.value()), std::move(args.value())));
        } else if (check(lexer::TokenKind::PctIdent)) {
            const lexer::Token& field = advance();
            expr = ast::ExpressionPtr(ast::make_FieldExpr(span_from(expr.value()->span, field.span),
                                       std::move(expr.value()), field.span));
        } else if (check(lexer::TokenKind::LBracket)) {
            advance();
            bool saved = no_struct_literal_;
            no_struct_literal_ = false;
            auto index = parse_expression();
            no_struct_literal_ = saved;
            if (!index) return index.error();
            auto close = expect(lexer::TokenKind::RBracket, "expected ']' after index");
            if (!close) return close.error();
            expr = ast::ExpressionPtr(ast::make_IndexExpr(span_from(expr.value()->span, close.value().span),
                                       std::move(expr.value()), std::move(index.value())));
        } else {
            break;
        }
    }
    return expr;
}

result::Result<ast::ExpressionPtr> Parser::parse_primary() {
    if (depth_++ >= MAX_DEPTH)
        return diagnostics::Diagnostic{diagnostics::Severity::error, "maximum parser nesting depth exceeded", peek().span};
    const lexer::Token& token = peek();
    ast::ExpressionPtr expr;

    switch (token.kind) {
        case lexer::TokenKind::IntLiteral: {
            advance();
            expr = ast::ExpressionPtr(ast::make_IntLit(token.span,
                                    token.numeric_base == lexer::NumericBase::Hex ? ast::NumericBase::Hexadecimal :
                                    token.numeric_base == lexer::NumericBase::Binary ? ast::NumericBase::Binary :
                                    token.numeric_base == lexer::NumericBase::Octal ? ast::NumericBase::Octal : ast::NumericBase::Decimal,
                                    token.span, token.suffix.empty() ? std::nullopt :
                                    std::optional<source::Span>{token.span},
                                    token.flags.unsuffixed));
            break;
        }
        case lexer::TokenKind::FloatLiteral: {
            advance();
            expr = ast::ExpressionPtr(ast::make_FloatLit(token.span, ast::NumericBase::Decimal,
                                      token.span, token.suffix.empty() ? std::nullopt :
                                      std::optional<source::Span>{token.span},
                                      token.flags.unsuffixed));
            break;
        }
        case lexer::TokenKind::StringLiteral:
            advance();
            expr = ast::ExpressionPtr(ast::make_StrLit(token.span, token.span));
            break;
        case lexer::TokenKind::RuneLiteral:
            advance();
            expr = ast::ExpressionPtr(ast::make_RuneLit(token.span, token.span));
            break;
        case lexer::TokenKind::True:
            advance(); expr = ast::ExpressionPtr(ast::make_BoolLit(token.span, true)); break;
        case lexer::TokenKind::False:
            advance(); expr = ast::ExpressionPtr(ast::make_BoolLit(token.span, false)); break;
        case lexer::TokenKind::Nullptr:
            advance(); expr = ast::ExpressionPtr(ast::make_NullptrLit(token.span)); break;
        case lexer::TokenKind::Self:
            advance(); expr = ast::ExpressionPtr(ast::make_SelfExpr(token.span)); break;
        case lexer::TokenKind::CtIdent:
            if (text(token) == "$sizeof" || text(token) == "$alignof" ||
                text(token) == "$cast" || text(token) == "$bitcast" ||
                text(token) == "$saturate" || text(token) == "$truncate" ||
                text(token) == "$ref" || text(token) == "$ptr") {
            {
                auto intrinsic = parse_intrinsic_expr();
                if (!intrinsic) return intrinsic.error();
                expr = std::move(intrinsic.value());
            }
            } else {
                advance();
                std::vector<ast::PathSegment> segments{{ast::PathSegmentKind::CompileTimeName, token.span}};
                expr = ast::ExpressionPtr(ast::make_NameRef(token.span, ast::make_Path(token.span, std::move(segments))));
            }
            break;
        case lexer::TokenKind::Identifier: {
            advance();
            std::vector<ast::PathSegment> segments;
            source::Span first = token.span, last = token.span;
            segments.push_back({ast::PathSegmentKind::Identifier, last});
            while (match(lexer::TokenKind::Dot)) {
                if (!check(lexer::TokenKind::Identifier))
                    return diagnostics::Diagnostic{diagnostics::Severity::error, "expected name after '.'", peek().span};
                last = advance().span;
                segments.push_back({ast::PathSegmentKind::Identifier, last});
            }
            expr = ast::ExpressionPtr(ast::make_NameRef(span_from(first, last), ast::make_Path(span_from(first, last), std::move(segments))));
            break;
        }
        case lexer::TokenKind::PctIdent: {
            advance();
            std::vector<ast::PathSegment> segments{{ast::PathSegmentKind::TypeName, token.span}};
            auto path = ast::make_Path(token.span, std::move(segments));
            if (!no_struct_literal_ && check(lexer::TokenKind::LBrace)) {
                advance();
                std::vector<ast::FieldInit> fields;
                if (!check(lexer::TokenKind::RBrace)) {
                    do {
                        if (!check(lexer::TokenKind::Identifier))
                            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected struct field name", peek().span};
                        const lexer::Token& name = advance();
                        if (!match(lexer::TokenKind::Colon))
                            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ':' after struct field name", peek().span};
                        auto value = parse_expression();
                        if (!value) return value.error();
                        fields.push_back({name.span, std::move(value.value())});
                    } while (match(lexer::TokenKind::Comma));
                }
                auto close = expect(lexer::TokenKind::RBrace, "expected '}' to close struct literal");
                if (!close) return close.error();
                expr = ast::ExpressionPtr(ast::make_StructLit(span_from(token.span, close.value().span),
                                           std::move(path), std::move(fields)));
            } else {
                expr = ast::ExpressionPtr(ast::make_NameRef(token.span, std::move(path)));
                if (no_struct_literal_ && check(lexer::TokenKind::LBrace))
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "struct literals here must be parenthesized", token.span};
            }
            break;
        }
        case lexer::TokenKind::LBracket: {
            auto size = parse_type();
            if (!size) return size.error();
            if (!no_struct_literal_ && check(lexer::TokenKind::LBrace)) {
                advance();
                std::vector<ast::ExpressionPtr> elements;
                if (!check(lexer::TokenKind::RBrace)) {
                    do {
                        auto element = parse_expression();
                        if (!element) return element.error();
                        elements.push_back(std::move(element.value()));
                    } while (match(lexer::TokenKind::Comma));
                }
                auto close = expect(lexer::TokenKind::RBrace, "expected '}' to close array literal");
                if (!close) return close.error();
                expr = ast::ExpressionPtr(ast::make_ArrayLit(span_from(token.span, close.value().span),
                                          std::move(size.value()), std::move(elements)));
            } else {
                if (no_struct_literal_ && check(lexer::TokenKind::LBrace))
                    return diagnostics::Diagnostic{diagnostics::Severity::error,
                        "struct literals here must be parenthesized", token.span};
                return diagnostics::Diagnostic{diagnostics::Severity::error, "array literal is missing '{'", peek().span};
            }
            break;
        }
        case lexer::TokenKind::LParen: {
            advance();
            bool saved = no_struct_literal_;
            no_struct_literal_ = false;
            auto inner = parse_expression();
            no_struct_literal_ = saved;
            if (!inner) return inner.error();
            auto close = expect(lexer::TokenKind::RParen, "expected ')' to close parenthesized expression");
            if (!close) return close.error();
            expr = ast::ExpressionPtr(ast::make_ParenExpr(span_from(token.span, close.value().span), std::move(inner.value())));
            break;
        }
        default:
            --depth_;
            return diagnostics::Diagnostic{diagnostics::Severity::error,
                "expected an expression", token.span};
    }
    --depth_;
    return expr;
}

result::Result<ast::ExpressionPtr> Parser::parse_intrinsic_expr() {
    const lexer::Token& intrinsic = advance();
    std::string_view name = text(intrinsic);
    if (!match(lexer::TokenKind::LParen))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '(' after intrinsic", peek().span};
    if (name == "$sizeof" || name == "$alignof") {
        auto type = parse_type();
        if (!type) return type.error();
        auto close = expect(lexer::TokenKind::RParen, "expected ')' after intrinsic type argument");
        if (!close) return close.error();
        if (name == "$sizeof") return ast::ExpressionPtr(ast::make_SizeofExpr(span_from(intrinsic.span, close.value().span), std::move(type.value())));
        return ast::ExpressionPtr(ast::make_AlignofExpr(span_from(intrinsic.span, close.value().span), std::move(type.value())));
    }
    if (name == "$cast" || name == "$bitcast" || name == "$saturate" || name == "$truncate") {
        auto type = parse_type();
        if (!type) return type.error();
        if (!match(lexer::TokenKind::Comma))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ',' after intrinsic type argument", peek().span};
        auto value = parse_expression();
        if (!value) return value.error();
        auto close = expect(lexer::TokenKind::RParen, "expected ')' after intrinsic arguments");
        if (!close) return close.error();
        ast::CastKind kind = name == "$cast" ? ast::CastKind::Cast :
                             name == "$bitcast" ? ast::CastKind::Bitcast :
                             name == "$saturate" ? ast::CastKind::Saturate : ast::CastKind::Truncate;
        return ast::ExpressionPtr(ast::make_CastExpr(span_from(intrinsic.span, close.value().span), kind,
                                  std::move(type.value()), std::move(value.value())));
    }
    bool mut = match(lexer::TokenKind::Mut);
    auto place = parse_postfix();
    if (!place) return place.error();
    auto close = expect(lexer::TokenKind::RParen, "expected ')' after intrinsic place");
    if (!close) return close.error();
    if (name == "$ref") return ast::ExpressionPtr(ast::make_RefExpr(span_from(intrinsic.span, close.value().span),
                                                  ast::RefPtrKind::Ref, mut, std::move(place.value())));
    return ast::ExpressionPtr(ast::make_PtrExpr(span_from(intrinsic.span, close.value().span),
                             ast::RefPtrKind::Ptr, mut, std::move(place.value())));
}

result::Result<std::vector<ast::ExpressionPtr>> Parser::parse_argument_list() {
    if (!match(lexer::TokenKind::LParen))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '(' to start argument list", peek().span};
    std::vector<ast::ExpressionPtr> args;
    bool saved = no_struct_literal_;
    no_struct_literal_ = false;
    if (!check(lexer::TokenKind::RParen)) {
        do {
            auto expr = parse_expression();
            if (!expr) { no_struct_literal_ = saved; return expr.error(); }
            args.push_back(std::move(expr.value()));
        } while (match(lexer::TokenKind::Comma));
    }
    no_struct_literal_ = saved;
    if (!match(lexer::TokenKind::RParen))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "unterminated parameter or argument list", peek().span};
    return args;
}

result::Result<ast::BlockPtr> Parser::parse_block() {
    source::Span open = peek().span;
    if (!match(lexer::TokenKind::LBrace))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' to start block", peek().span};
    std::vector<ast::StatementPtr> statements;
    while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
        if (depth_++ >= MAX_DEPTH) {
            --depth_;
            return diagnostics::Diagnostic{diagnostics::Severity::error, "maximum parser nesting depth exceeded", peek().span};
        }
        auto stmt = parse_stmt();
        --depth_;
        if (stmt) statements.push_back(std::move(stmt.value()));
        else {
            diagnostics_.error(stmt.error().message, stmt.error().span.value_or(peek().span));
            synchronize_statement();
        }
    }
    if (!match(lexer::TokenKind::RBrace))
        return diagnostics::Diagnostic{diagnostics::Severity::error,
            "expected '}' to close this block (opened here)", open};
    return ast::make_Block(span_from(open, previous_span()), std::move(statements));
}

static ast::StorageKind storage_for(lexer::TokenKind kind) {
    if (kind == lexer::TokenKind::Static) return ast::StorageKind::Static;
    if (kind == lexer::TokenKind::Const) return ast::StorageKind::Const;
    return ast::StorageKind::Let;
}

result::Result<ast::StatementPtr> Parser::parse_var_decl(bool module_level) {
    source::Span start = peek().span;
    lexer::TokenKind storage_token = advance().kind;
    bool mut = match(lexer::TokenKind::Mut);
    ast::TypePtr type;
    if (check(lexer::TokenKind::Auto)) {
        advance();
        type = ast::make_AutoType(previous_span());
    } else {
        auto parsed = parse_type();
        if (!parsed) return ast::StatementPtr(ast::make_ErrorStmt(start));
        type = std::move(parsed.value());
    }
    if (!match(lexer::TokenKind::ColonColon))
        return diagnostics::Diagnostic{diagnostics::Severity::error,
            "expected `::` after the type in this declaration", peek().span};
    auto name = parse_decl_name();
    if (!name) return ast::StatementPtr(ast::make_ErrorStmt(start));
    ast::ExpressionPtr init;
    if (match(lexer::TokenKind::Eq)) {
        auto parsed = parse_expression();
        if (!parsed) return ast::StatementPtr(ast::make_ErrorStmt(start));
        init = std::move(parsed.value());
    }
    if (!match(lexer::TokenKind::Semicolon))
        return diagnostics::Diagnostic{diagnostics::Severity::error,
            "expected ';' after this statement", peek().span};
    if (module_level && storage_token == lexer::TokenKind::Let)
        diagnostics_.error("let is not allowed at module level", start);
    return ast::StatementPtr(ast::make_VarDecl(span_from(start, previous_span()), storage_for(storage_token),
                             mut, std::move(type), std::move(name.value()), std::move(init)));
}

result::Result<ast::StatementPtr> Parser::parse_if_stmt() {
    source::Span start = advance().span;
    bool saved = no_struct_literal_;
    no_struct_literal_ = true;
    auto cond = parse_expression();
    no_struct_literal_ = saved;
    if (!cond) return cond.error();
    auto then_block = parse_block();
    if (!then_block) return then_block.error();
    ast::NodePtr otherwise;
    if (match(lexer::TokenKind::Else)) {
        if (check(lexer::TokenKind::If)) otherwise = std::move(parse_if_stmt().value());
        else {
            auto block = parse_block();
            if (!block) return block.error();
            otherwise = std::move(block.value());
        }
    }
    source::Span end = otherwise ? otherwise->span : then_block.value()->span;
    ast::IfStmtPtr result = ast::make_IfStmt(span_from(start, end),
                                              std::move(cond.value()), std::move(then_block.value()),
                                              std::move(otherwise));
    return ast::StatementPtr(std::move(result));
}

result::Result<ast::StatementPtr> Parser::parse_while_stmt() {
    source::Span start = advance().span;
    bool saved = no_struct_literal_;
    no_struct_literal_ = true;
    auto cond = parse_expression();
    no_struct_literal_ = saved;
    if (!cond) return cond.error();
    auto body = parse_block();
    if (!body) return body.error();
    return ast::StatementPtr(ast::make_WhileStmt(span_from(start, body.value()->span),
                               std::move(cond.value()), std::move(body.value())));
}

result::Result<ast::StatementPtr> Parser::parse_do_stmt() {
    source::Span start = advance().span;
    if (match(lexer::TokenKind::LBrace)) {
        cursor_--;
        auto body = parse_block();
        if (!body) return body.error();
        if (!match(lexer::TokenKind::While))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected 'while' after do block", peek().span};
        auto cond = parse_expression();
        if (!cond) return cond.error();
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after do-while condition", peek().span};
        return ast::StatementPtr(ast::make_DoWhileStmt(span_from(start, previous_span()),
                                     std::move(body.value()), std::move(cond.value())));
    }
    ast::TypePtr type;
    if (check(lexer::TokenKind::Auto)) {
        advance();
        type = ast::make_AutoType(previous_span());
    } else {
        auto parsed = parse_type();
        if (!parsed) return ast::StatementPtr(ast::make_ErrorStmt(start));
        type = std::move(parsed.value());
    }
    if (!match(lexer::TokenKind::ColonColon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected `::` after do-loop type", peek().span};
    auto name = parse_decl_name();
    if (!name) return ast::StatementPtr(ast::make_ErrorStmt(start));
    if (!match(lexer::TokenKind::In))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "do loop is missing 'in'", peek().span};
    bool saved = no_struct_literal_;
    no_struct_literal_ = true;
    auto range = parse_expression();
    no_struct_literal_ = saved;
    if (!range) return range.error();
    auto body = parse_block();
    if (!body) return body.error();
    return ast::StatementPtr(ast::make_DoStmt(span_from(start, body.value()->span), std::move(type),
                            std::move(name.value()), std::move(range.value()), std::move(body.value())));
}

result::Result<ast::StatementPtr> Parser::parse_ct_if_stmt() {
    source::Span start = advance().span;
    ast::CtIfStmtPtr ct = ast::make_CtIfStmt(start);
    while (true) {
        bool saved = no_struct_literal_;
        no_struct_literal_ = true;
        auto cond = parse_expression();
        no_struct_literal_ = saved;
        if (!cond) return cond.error();
        auto body = parse_block();
        if (!body) return body.error();
        ct->branches.push_back({std::move(cond.value()), {}});
        // Copy statement pointers directly by moving block contents.
        ct->branches.back().body = std::move(static_cast<ast::Block&>(*body.value()).statements);
        if (!check(lexer::TokenKind::CtIdent) || text(peek()) != "$else")
            break;
        advance();
        if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
            advance();
            continue;
        }
        auto else_block = parse_block();
        if (!else_block) return else_block.error();
        ct->else_body = std::move(static_cast<ast::Block&>(*else_block.value()).statements);
        break;
    }
    return ast::StatementPtr(std::move(ct));
}

result::Result<ast::StatementPtr> Parser::parse_stmt() {
    source::Span start = peek().span;
    if (check(lexer::TokenKind::Let) || check(lexer::TokenKind::Static) || check(lexer::TokenKind::Const))
        return parse_var_decl(false);
    if (check(lexer::TokenKind::Return)) {
        advance();
        ast::ExpressionPtr value;
        if (!check(lexer::TokenKind::Semicolon)) {
            auto parsed = parse_expression();
            if (!parsed) return ast::StatementPtr(ast::make_ErrorStmt(start));
            value = std::move(parsed.value());
        }
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after return statement", peek().span};
        return ast::StatementPtr(ast::make_ReturnStmt(span_from(start, previous_span()), std::move(value)));
    }
    if (check(lexer::TokenKind::Break)) {
        advance();
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after break", peek().span};
        return ast::StatementPtr(ast::make_BreakStmt(span_from(start, previous_span())));
    }
    if (check(lexer::TokenKind::Continue)) {
        advance();
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after continue", peek().span};
        return ast::StatementPtr(ast::make_ContinueStmt(span_from(start, previous_span())));
    }
    if (check(lexer::TokenKind::If)) return parse_if_stmt();
    if (check(lexer::TokenKind::While)) return parse_while_stmt();
    if (check(lexer::TokenKind::Do)) return parse_do_stmt();
    if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") return parse_ct_if_stmt();
    if (check(lexer::TokenKind::LBrace)) {
        auto block = parse_block();
        if (!block) return block.error();
        return ast::StatementPtr(std::move(block.value()));
    }
    if (check(lexer::TokenKind::PlusPlus) || check(lexer::TokenKind::MinusMinus))
        return diagnostics::Diagnostic{diagnostics::Severity::error,
            "increment/decrement must follow a postfix expression", peek().span};

    auto expr = parse_expression();
    if (!expr) {
        synchronize_statement();
        return ast::StatementPtr(ast::make_ErrorStmt(start));
    }
    if (is_assignment(peek().kind)) {
        if (!is_place(expr.value()))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "invalid assignment target", expr.value()->span};
        lexer::TokenKind op_token = advance().kind;
        auto rhs = parse_expression();
        if (!rhs) return ast::StatementPtr(ast::make_ErrorStmt(start));
        if (check(lexer::TokenKind::Eq) || is_assignment(peek().kind))
            return diagnostics::Diagnostic{diagnostics::Severity::error,
                "chained assignment is not valid", peek().span};
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after assignment", peek().span};
        return ast::StatementPtr(ast::make_AssignStmt(span_from(start, previous_span()), assignment_op(op_token),
                                    std::move(expr.value()), std::move(rhs.value())));
    }
    if (check(lexer::TokenKind::PlusPlus) || check(lexer::TokenKind::MinusMinus)) {
        bool increment = check(lexer::TokenKind::PlusPlus);
        advance();
        if (!is_place(expr.value()))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "invalid assignment target", expr.value()->span};
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after increment/decrement", peek().span};
        return ast::StatementPtr(ast::make_IncDecStmt(span_from(start, previous_span()),
                                    increment ? ast::IncDecKind::Increment : ast::IncDecKind::Decrement,
                                    std::move(expr.value())));
    }
    if (!match(lexer::TokenKind::Semicolon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after this statement", peek().span};
    return ast::StatementPtr(ast::make_ExprStmt(span_from(start, previous_span()), std::move(expr.value())));
}

result::Result<ast::AttributeListPtr> Parser::parse_attribute_list() {
    source::Span start = advance().span;
    std::vector<ast::AttributePtr> attrs;
    do {
        if (!check(lexer::TokenKind::Identifier))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected attribute name", peek().span};
        const lexer::Token& name = advance();
        std::vector<ast::ExpressionPtr> args;
        if (match(lexer::TokenKind::LParen)) {
            if (!check(lexer::TokenKind::RParen)) {
                do {
                    auto arg = parse_expression();
                    if (!arg) return arg.error();
                    args.push_back(std::move(arg.value()));
                } while (match(lexer::TokenKind::Comma));
            }
            if (!match(lexer::TokenKind::RParen))
                return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ')' after attribute arguments", peek().span};
        }
        attrs.push_back(ast::make_Attribute(span_from(start, name.span), name.span, std::move(args)));
    } while (match(lexer::TokenKind::Comma));
    if (!match(lexer::TokenKind::RBracket))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ']' to close attribute list", peek().span};
    return ast::make_AttributeList(span_from(start, previous_span()), std::move(attrs));
}

result::Result<Parser::Metadata> Parser::parse_metadata(bool collect_docs_first) {
    Metadata metadata;
    bool docs_first_pass = collect_docs_first;
    (void)docs_first_pass;
    while (check(lexer::TokenKind::DocComment) || check(lexer::TokenKind::AttributeStart)) {
        if (check(lexer::TokenKind::DocComment)) {
            metadata.docs.push_back(ast::make_DocComment(advance().span));
        } else {
            auto attrs = parse_attribute_list();
            if (!attrs) return attrs.error();
            metadata.attributes.push_back(std::move(attrs.value()));
        }
    }
    return metadata;
}

result::Result<ast::FieldDeclPtr> Parser::parse_field_decl() {
    source::Span start = peek().span;
    bool pub = match(lexer::TokenKind::Pub);
    auto type = parse_type();
    if (!type) return diagnostics::Diagnostic{diagnostics::Severity::error, "expected field type", start};
    if (!match(lexer::TokenKind::ColonColon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected `::` after field type", peek().span};
    if (!check(lexer::TokenKind::Identifier))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected struct field name", peek().span};
    const lexer::Token& name = advance();
    if (!match(lexer::TokenKind::Semicolon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after struct field", peek().span};
    auto result = ast::make_FieldDecl(span_from(start, previous_span()), pub,
                                      std::move(type.value()), name.span);
    return ast::FieldDeclPtr(std::move(result));
}

result::Result<ast::ParamPtr> Parser::parse_param() {
    source::Span start = peek().span;
    match(lexer::TokenKind::Mut);
    ast::TypePtr type;
    if (check(lexer::TokenKind::Amp) || check(lexer::TokenKind::AmpAmp)) {
        auto parsed = parse_type();
        if (!parsed) return ast::ParamPtr{};
        type = std::move(parsed.value());
    } else {
        auto parsed = parse_type();
        if (!parsed) return ast::ParamPtr{};
        type = std::move(parsed.value());
    }
    if (!match(lexer::TokenKind::ColonColon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected `::` after parameter type", peek().span};
    auto name = parse_decl_name();
    if (!name) return ast::ParamPtr{};
    return ast::make_Param(span_from(start, previous_span()), std::move(type), std::move(name.value()));
}

result::Result<ast::NodePtr> Parser::parse_module_decl() {
    source::Span start = advance().span;
    auto path = parse_module_path();
    if (!path) return path.error();
    if (!match(lexer::TokenKind::Semicolon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after module declaration", peek().span};
    ast::NodePtr result = ast::make_ModuleDecl(span_from(start, previous_span()), std::move(path.value()));
    return result;
}

result::Result<ast::NodePtr> Parser::parse_import_decl() {
    source::Span start = advance().span;
    auto path = parse_module_path();
    if (!path) return path.error();
    if (!match(lexer::TokenKind::Semicolon))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected ';' after import declaration", peek().span};
    ast::NodePtr result = ast::make_ImportDecl(span_from(start, previous_span()), std::move(path.value()));
    return result;
}

result::Result<ast::ItemPtr> Parser::parse_func_decl() {
    source::Span start = peek().span;
    bool pub = match(lexer::TokenKind::Pub);
    if (!match(lexer::TokenKind::Func))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected 'func'", peek().span};
    if (!check(lexer::TokenKind::Identifier))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected function name", peek().span};
    source::Span name = advance().span;
    if (!match(lexer::TokenKind::LParen))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "unterminated parameter list", peek().span};
    std::vector<ast::NodePtr> params;
    ast::NodePtr self_param;
    if (!check(lexer::TokenKind::RParen)) {
        do {
            if (check(lexer::TokenKind::Self)) {
                advance();
                self_param = ast::make_SelfParam(previous_span(), ast::SelfParamKind::Value);
            } else if (check(lexer::TokenKind::Amp)) {
                advance();
                if (match(lexer::TokenKind::Mut) && check(lexer::TokenKind::Self)) {
                    advance();
                    self_param = ast::make_SelfParam(previous_span(), ast::SelfParamKind::RefMut);
                } else if (check(lexer::TokenKind::Self)) {
                    advance();
                    self_param = ast::make_SelfParam(previous_span(), ast::SelfParamKind::Ref);
                } else {
                    cursor_--;
                    auto param = parse_param();
                    if (!param) return param.error();
                    params.push_back(std::move(param.value()));
                }
            } else {
                auto param = parse_param();
                if (!param) return param.error();
                params.push_back(std::move(param.value()));
            }
        } while (match(lexer::TokenKind::Comma));
    }
    if (!match(lexer::TokenKind::RParen))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "unterminated parameter list", peek().span};
    ast::TypePtr ret;
    if (match(lexer::TokenKind::Arrow)) {
        auto parsed = parse_type();
        if (!parsed) return parsed.error();
        ret = std::move(parsed.value());
    }
    ast::BlockPtr body;
    if (check(lexer::TokenKind::LBrace)) {
        auto parsed = parse_block();
        if (!parsed) return parsed.error();
        body = std::move(parsed.value());
    } else {
        if (!match(lexer::TokenKind::Semicolon))
            return diagnostics::Diagnostic{diagnostics::Severity::error,
                "expected '{' or ';' after function signature", peek().span};
    }
    source::Span end = body ? body->span : previous_span();
    ast::ItemPtr result;
    if (self_param) {
        result = ast::make_MethodDecl(span_from(start, end), pub, name,
                                      std::move(self_param), std::move(params),
                                      std::move(ret), std::move(body));
    } else {
        result = ast::make_FuncDecl(span_from(start, end), pub, name,
                                    std::move(self_param), std::move(params),
                                    std::move(ret), std::move(body));
    }
    return result;
}

result::Result<ast::ItemPtr> Parser::parse_type_decl() {
    source::Span start = advance().span;
    bool pub = match(lexer::TokenKind::Pub);
    if (!check(lexer::TokenKind::PctIdent))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected type name after 'type'", peek().span};
    source::Span name = advance().span;
    if (!match(lexer::TokenKind::LBrace))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' to start type declaration", peek().span};
    std::vector<ast::NodePtr> members;
    while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
        if (check(lexer::TokenKind::DocComment) || check(lexer::TokenKind::AttributeStart)) {
            auto meta = parse_metadata();
            if (!meta) return meta.error();
            ast::DocComments docs = std::move(meta.value().docs);
            ast::AttributeLists attrs = std::move(meta.value().attributes);
            if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
                auto ct = parse_ct_if_field();
                if (!ct) return ct.error();
                members.push_back(std::move(ct.value()));
                continue;
            }
            auto field = parse_field_decl();
            if (!field) return field.error();
            static_cast<ast::FieldDecl&>(*field.value()).doc_comments = std::move(docs);
            static_cast<ast::FieldDecl&>(*field.value()).attributes = std::move(attrs);
            members.push_back(std::move(field.value()));
        } else if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
            auto ct = parse_ct_if_field();
            if (!ct) return ct.error();
            members.push_back(std::move(ct.value()));
        } else {
            auto field = parse_field_decl();
            if (!field) {
                synchronize_statement();
                continue;
            }
            members.push_back(std::move(field.value()));
        }
    }
    if (!match(lexer::TokenKind::RBrace))
        return diagnostics::Diagnostic{diagnostics::Severity::error,
            "expected '}' to close type declaration", peek().span};
    ast::ItemPtr result = ast::make_TypeDecl(span_from(start, previous_span()),
                                             pub, name, ast::DocComments{}, ast::AttributeLists{}, std::move(members));
    return result;
}

result::Result<ast::ItemPtr> Parser::parse_impl_decl() {
    source::Span start = advance().span;
    auto path = parse_type_path();
    if (!path) return path.error();
    if (!match(lexer::TokenKind::LBrace))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' to start impl block", peek().span};
    std::vector<ast::NodePtr> members;
    while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
        auto meta = parse_metadata();
        if (!meta) return meta.error();
        if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
            auto ct = parse_ct_if_member();
            if (!ct) return ct.error();
            members.push_back(std::move(ct.value()));
            continue;
        }
        if (!check(lexer::TokenKind::Func)) {
            diagnostics_.error("expected impl member declaration", peek().span);
            synchronize_statement();
            continue;
        }
        advance();
        cursor_--;
        auto method = parse_func_decl();
        if (!method) return method.error();
        static_cast<ast::MethodDecl&>(*method.value()).doc_comments = std::move(meta.value().docs);
        static_cast<ast::MethodDecl&>(*method.value()).attributes = std::move(meta.value().attributes);
        members.push_back(std::move(method.value()));
    }
    if (!match(lexer::TokenKind::RBrace))
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '}' to close impl block", peek().span};
    ast::ItemPtr result = ast::make_ImplDecl(span_from(start, previous_span()),
                                             std::move(path.value()), std::move(members));
    return result;
}

result::Result<ast::CtIfItemPtr> Parser::parse_ct_if_item() {
    source::Span start = advance().span;
    ast::CtIfItemPtr ct = ast::make_CtIfItem(start);
    while (true) {
        bool saved = no_struct_literal_;
        no_struct_literal_ = true;
        auto cond = parse_expression();
        no_struct_literal_ = saved;
        if (!cond) return cond.error();
        if (!match(lexer::TokenKind::LBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' to start $if item branch", peek().span};
        std::vector<ast::ItemPtr> body;
        while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
            auto item = parse_item();
            if (item) body.push_back(std::move(item.value()));
            else { synchronize_item(); }
        }
        if (!match(lexer::TokenKind::RBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '}' to close $if item branch", peek().span};
        ct->branches.push_back({std::move(cond.value()), {}});
        ct->branches.back().body = std::move(body);
        if (!check(lexer::TokenKind::CtIdent) || text(peek()) != "$else") break;
        advance();
        if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
            advance();
            continue;
        }
        if (!match(lexer::TokenKind::LBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' after $else", peek().span};
        while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
            auto item = parse_item();
            if (item) ct->else_body.push_back(std::move(item.value()));
            else synchronize_item();
        }
        if (!match(lexer::TokenKind::RBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '}' to close $else branch", peek().span};
        break;
    }
    return ct;
}

result::Result<ast::CtIfFieldPtr> Parser::parse_ct_if_field() {
    source::Span start = advance().span;
    ast::CtIfFieldPtr ct = ast::make_CtIfField(start);
    while (true) {
        auto cond = parse_expression();
        if (!cond) return cond.error();
        if (!match(lexer::TokenKind::LBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' in $if field branch", peek().span};
        while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
            if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
                auto nested = parse_ct_if_field();
                if (!nested) return nested.error();
                ct->branches.push_back({std::move(cond.value()), {}});
                ct->branches.back().body.push_back(std::move(nested.value()));
            } else {
                auto field = parse_field_decl();
                if (!field) { synchronize_statement(); break; }
                ct->branches.push_back({std::move(cond.value()), {}});
                ct->branches.back().body.push_back(std::move(field.value()));
            }
        }
        if (!match(lexer::TokenKind::RBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '}' to close $if field branch", peek().span};
        if (!check(lexer::TokenKind::CtIdent) || text(peek()) != "$else") break;
        advance();
        if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") { advance(); continue; }
        if (!match(lexer::TokenKind::LBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' after $else", peek().span};
        while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
            auto field = parse_field_decl();
            if (field) ct->else_body.push_back(std::move(field.value()));
            else break;
        }
        match(lexer::TokenKind::RBrace);
        break;
    }
    return ct;
}

result::Result<ast::CtIfMemberPtr> Parser::parse_ct_if_member() {
    source::Span start = advance().span;
    ast::CtIfMemberPtr ct = ast::make_CtIfMember(start);
    while (true) {
        auto cond = parse_expression();
        if (!cond) return cond.error();
        if (!match(lexer::TokenKind::LBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' in $if member branch", peek().span};
        while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
            auto method = parse_func_decl();
            if (!method) { synchronize_statement(); break; }
            ct->branches.push_back({std::move(cond.value()), {}});
            ct->branches.back().body.push_back(std::move(method.value()));
        }
        if (!match(lexer::TokenKind::RBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '}' to close $if member branch", peek().span};
        if (!check(lexer::TokenKind::CtIdent) || text(peek()) != "$else") break;
        advance();
        if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") { advance(); continue; }
        if (!match(lexer::TokenKind::LBrace))
            return diagnostics::Diagnostic{diagnostics::Severity::error, "expected '{' after $else", peek().span};
        while (!check(lexer::TokenKind::RBrace) && !at_eof()) {
            auto method = parse_func_decl();
            if (method) ct->else_body.push_back(std::move(method.value()));
            else break;
        }
        match(lexer::TokenKind::RBrace);
        break;
    }
    return ct;
}

result::Result<ast::ItemPtr> Parser::parse_item() {
    auto meta = parse_metadata();
    if (!meta) return meta.error();
    ast::DocComments docs = std::move(meta.value().docs);
    ast::AttributeLists attrs = std::move(meta.value().attributes);
    bool pub = match(lexer::TokenKind::Pub);
    ast::ItemPtr item;

    if (check(lexer::TokenKind::Func)) {
        auto parsed = parse_func_decl();
        if (!parsed) return parsed.error();
        item = std::move(parsed.value());
    } else if (check(lexer::TokenKind::Type)) {
        auto parsed = parse_type_decl();
        if (!parsed) return parsed.error();
        item = std::move(parsed.value());
    } else if (check(lexer::TokenKind::Impl)) {
        auto parsed = parse_impl_decl();
        if (!parsed) return parsed.error();
        item = std::move(parsed.value());
    } else if (check(lexer::TokenKind::Static) || check(lexer::TokenKind::Const)) {
        source::Span start = peek().span;
        auto parsed = parse_var_decl(true);
        if (!parsed) return parsed.error();
        item = ast::ItemPtr(std::move(parsed.value()));
        (void)start;
    } else if (check(lexer::TokenKind::CtIdent) && text(peek()) == "$if") {
        auto parsed = parse_ct_if_item();
        if (!parsed) return parsed.error();
        item = std::move(parsed.value());
    } else if (check(lexer::TokenKind::Let)) {
        diagnostics_.error("let is not allowed at module level", peek().span);
        auto parsed = parse_var_decl(true);
        if (!parsed) return parsed.error();
        item = ast::ItemPtr(std::move(parsed.value()));
    } else if (check(lexer::TokenKind::RBrace)) {
        if (!docs.empty()) diagnostics_.error("doc comment before '}'", docs.front()->span);
        if (!attrs.empty()) diagnostics_.error("attribute before '}'", attrs.front()->span);
        return ast::ItemPtr(ast::make_ErrorItem(peek().span));
    } else {
        if (!docs.empty()) diagnostics_.error("doc comment is not followed by an item", docs.front()->span);
        if (!attrs.empty()) diagnostics_.error("attribute is not followed by an item", attrs.front()->span);
        return diagnostics::Diagnostic{diagnostics::Severity::error, "expected item", peek().span};
    }

    if (pub) {
        if (item->kind == ast::Kind::FuncDecl) static_cast<ast::FuncDecl&>(*item).is_public = true;
        if (item->kind == ast::Kind::TypeDecl) static_cast<ast::TypeDecl&>(*item).is_public = true;
    }
    if (!docs.empty()) {
        if (item->kind == ast::Kind::FuncDecl) static_cast<ast::FuncDecl&>(*item).doc_comments = std::move(docs);
        else if (item->kind == ast::Kind::TypeDecl) static_cast<ast::TypeDecl&>(*item).doc_comments = std::move(docs);
        else if (item->kind == ast::Kind::ImplDecl) static_cast<ast::ImplDecl&>(*item).doc_comments = std::move(docs);
    }
    if (!attrs.empty()) {
        if (item->kind == ast::Kind::FuncDecl) static_cast<ast::FuncDecl&>(*item).attributes = std::move(attrs);
        else if (item->kind == ast::Kind::TypeDecl) static_cast<ast::TypeDecl&>(*item).attributes = std::move(attrs);
        else if (item->kind == ast::Kind::ImplDecl) static_cast<ast::ImplDecl&>(*item).attributes = std::move(attrs);
    }
    return item;
}

} // namespace xenon::parser
