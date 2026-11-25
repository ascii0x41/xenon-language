#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/source/file_id.h"
#include "common/source/span.h"
#include "lexer/token_kind.h"
#include "parser_result.h"
#include "lexer/token.h"
#include "common/result/result.h"

namespace xenon::parser {

class Parser {
public:
    static ParserOutput parse(const lexer::TokenStream& tokens, source::FileID fid);

private:
    struct Metadata {
        ast::DocComments docs;
        ast::AttributeLists attributes;
    };

    Parser(const lexer::TokenStream&, source::FileID);
    ParserOutput parse();

    const lexer::TokenStream& tokens_;
    source::FileID fid_;
    std::size_t cursor_ = 0;
    std::size_t depth_ = 0;
    diagnostics::DiagnosticCollector diagnostics_;
    lexer::Token eof_;
    bool no_struct_literal_ = false;

    const lexer::Token& peek(std::size_t n = 0) const;
    const lexer::Token& advance();
    source::Span previous_span() const;
    bool check(lexer::TokenKind) const;
    bool match(lexer::TokenKind);
    result::Result<lexer::Token> expect(lexer::TokenKind, const char* message);
    bool at_eof() const;
    std::string_view text(const lexer::Token&) const;
    std::string_view ct_text(const lexer::Token&) const;
    source::Span span_from(source::Span start, source::Span end) const;
    result::Result<ast::PathPtr> parse_module_path();
    result::Result<ast::PathPtr> parse_name();
    result::Result<ast::DeclNamePtr> parse_decl_name();
    result::Result<ast::PathPtr> parse_type_path();
    result::Result<ast::TypePtr> parse_type();
    result::Result<ast::TypePtr> parse_auto_type();
    result::Result<ast::ExpressionPtr> parse_expression();
    result::Result<ast::ExpressionPtr> parse_cond();
    result::Result<ast::ExpressionPtr> parse_logical_or();
    result::Result<ast::ExpressionPtr> parse_logical_and();
    result::Result<ast::ExpressionPtr> parse_comparison();
    result::Result<ast::ExpressionPtr> parse_bitor();
    result::Result<ast::ExpressionPtr> parse_bitxor();
    result::Result<ast::ExpressionPtr> parse_bitand();
    result::Result<ast::ExpressionPtr> parse_shift();
    result::Result<ast::ExpressionPtr> parse_add();
    result::Result<ast::ExpressionPtr> parse_mul();
    result::Result<ast::ExpressionPtr> parse_unary();
    result::Result<ast::ExpressionPtr> parse_power();
    result::Result<ast::ExpressionPtr> parse_postfix();
    result::Result<ast::ExpressionPtr> parse_primary();
    result::Result<ast::ExpressionPtr> parse_intrinsic_expr();
    result::Result<std::vector<ast::ExpressionPtr>> parse_argument_list();
    result::Result<std::vector<ast::ExpressionPtr>> parse_expression_list();
    result::Result<ast::BlockPtr> parse_block();
    result::Result<ast::StatementPtr> parse_stmt();
    result::Result<ast::StatementPtr> parse_var_decl(bool module_level = false);
    result::Result<ast::StatementPtr> parse_if_stmt();
    result::Result<ast::StatementPtr> parse_while_stmt();
    result::Result<ast::StatementPtr> parse_do_stmt();
    result::Result<ast::StatementPtr> parse_ct_if_stmt();
    result::Result<ast::ItemPtr> parse_item();
    result::Result<ast::NodePtr> parse_import_decl();
    result::Result<ast::NodePtr> parse_module_decl();
    result::Result<ast::ItemPtr> parse_func_decl();
    result::Result<ast::ItemPtr> parse_type_decl();
    result::Result<ast::ItemPtr> parse_impl_decl();
    result::Result<ast::FieldDeclPtr> parse_field_decl();
    result::Result<ast::ParamPtr> parse_param();
    result::Result<ast::NodePtr> parse_method_decl();
    result::Result<ast::CtIfItemPtr> parse_ct_if_item();
    result::Result<ast::CtIfFieldPtr> parse_ct_if_field();
    result::Result<ast::CtIfMemberPtr> parse_ct_if_member();
    result::Result<Metadata> parse_metadata(bool collect_docs_first = false);
    result::Result<ast::AttributeListPtr> parse_attribute_list();
    void report(const char* message, source::Span span);
    void synchronize_statement();
    void synchronize_item();
};
} // namespace xenon::parser
