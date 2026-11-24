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
        static ParserOutput parse(const lexer::TokenStream& token_stream, source::FileID fid) {
            return Parser(token_stream, fid).parse();
        }

    private:
        struct Metadata {
            std::string doc;
            std::optional<source::Span> doc_span;
            std::vector<ast::Attribute> attributes;
            std::optional<source::Span> attributes_span;
        };

        explicit Parser(const lexer::TokenStream& token_stream, source::FileID fid)
            : token_stream_(token_stream), fid_(fid), token_stream_cursor_(0),
              eof_token_{
                  .kind = lexer::TokenKind::EOF_TOKEN,
                  .span = source::Span{
                      .file = fid,
                      .start = token_stream.empty() ? 0 : token_stream.back().span.end,
                      .end = token_stream.empty() ? 0 : token_stream.back().span.end
                  }
              } {}

        ParserOutput parse();

        const lexer::TokenStream& token_stream_;
        source::FileID fid_;
        std::size_t token_stream_cursor_;
        lexer::Token eof_token_;
        Metadata pending_metadata_;
        std::string module_doc_;
        bool no_struct_literal_ = false;

        source::Span location() const;
        bool is_at_eof() const;

        const lexer::Token& peek() const;
        const lexer::Token& peek_next() const;

        const lexer::Token& advance();
        const lexer::Token& previous() const;

        bool match(lexer::TokenKind kind);
        bool check(lexer::TokenKind kind) const;
        bool accept(lexer::TokenKind kind);
        result::Result<lexer::Token> expect(lexer::TokenKind kind, const std::string& message);
        result::Result<lexer::Token> expect_semicolon();

        result::Result<ast::HeaderInfo> parse_header();
        result::Result<ast::NamePtr> parse_module_path();

        result::Result<ast::NamePtr> parse_name();
        result::Result<Metadata> parse_metadata();
        result::Result<std::vector<ast::Attribute>> parse_attribute_group();
        result::Result<ast::TypeExprPtr> parse_type_expression();
        result::Result<ast::TypeExprPtr> parse_return_type();
        result::Result<ast::BlockPtr> parse_optional_body();
        result::Result<std::vector<ast::ExpressionPtr>> parse_arguments();
        result::Result<std::vector<ast::VariableDeclPtr>> parse_parameters();

        result::Result<ast::ExpressionPtr> parse_expression();
        result::Result<ast::ExpressionPtr> parse_binary(
            result::Result<ast::ExpressionPtr> (Parser::*lower)(),
            std::initializer_list<std::pair<lexer::TokenKind, ast::BinaryOperatorKind>> operators
        );
        result::Result<ast::ExpressionPtr> parse_assignment();
        result::Result<ast::ExpressionPtr> parse_ternary();
        result::Result<ast::ExpressionPtr> parse_logical_or();
        result::Result<ast::ExpressionPtr> parse_logical_and();
        result::Result<ast::ExpressionPtr> parse_bitwise_or();
        result::Result<ast::ExpressionPtr> parse_bitwise_xor();
        result::Result<ast::ExpressionPtr> parse_bitwise_and();
        result::Result<ast::ExpressionPtr> parse_equality();
        result::Result<ast::ExpressionPtr> parse_comparison();
        result::Result<ast::ExpressionPtr> parse_shift();
        result::Result<ast::ExpressionPtr> parse_term();
        result::Result<ast::ExpressionPtr> parse_factor();
        result::Result<ast::ExpressionPtr> parse_unary();
        result::Result<ast::ExpressionPtr> parse_postfix();
        result::Result<ast::ExpressionPtr> parse_primary();

        bool is_assignment_token(lexer::TokenKind kind) const;
        bool is_unary_token(lexer::TokenKind kind) const;

        result::Result<ast::ExpressionPtr> parse_array_literal();
        result::Result<ast::ExpressionPtr> parse_struct_literal(source::Span span, ast::TypeExprPtr type_expr);
        result::Result<std::vector<ast::ExpressionPtr>> parse_braced_expressions(const std::string& what);

        result::Result<ast::DeclarationPtr> parse_declaration();
        void synchronize_declaration(std::size_t start_cursor);
        result::Result<ast::StatementPtr> parse_statement();
        result::Result<ast::BlockPtr> parse_block();
        result::Result<ast::StatementPtr> parse_if_statement();
        result::Result<ast::StatementPtr> parse_while_statement();

        result::Result<ast::VariableDeclPtr> parse_variable_declaration(bool is_public = false);
        result::Result<ast::FunctionDeclPtr> parse_function_declaration(bool is_public = false);
        result::Result<ast::StructFieldDeclPtr> parse_struct_field_declaration(bool is_public = false);
        result::Result<ast::MethodDeclPtr> parse_method_declaration(bool is_public = false, bool is_static = false);
        result::Result<ast::MethodDeclPtr> parse_method_declaration(
            bool is_public,
            bool is_static,
            const ast::Name* impl_type
        );
        result::Result<std::pair<std::optional<ast::SelfReceiver>, std::vector<ast::VariableDeclPtr>>>
        parse_method_parameters(const ast::Name* impl_type);
        result::Result<ast::StructDeclPtr> parse_struct_declaration(bool is_public = false);
        result::Result<ast::ImplDeclPtr> parse_impl_declaration();
    };
} // namespace xenon::parser
