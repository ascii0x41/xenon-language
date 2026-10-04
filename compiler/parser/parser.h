#pragma once

#include "common/dataclasses.h"
#include "tokens/tokens.h"
#include "ast/astlib.h"
#include "common/diagnostics.h"

#include <string>
#include <vector>

namespace xenon::parser {

    using tokens::Token;
    using tokens::TokenType;
    using tokens::TokenStream;
    using common::SourceLocation;
    using namespace ast;

    class Parser {
    public:
        static ModuleAST parse(const TokenStream& tokens, std::string filepath) {
            return Parser(tokens, std::move(filepath)).parse();
        }
    private:
        explicit Parser(const TokenStream& tokens, std::string filepath)
            : tokens_(tokens), filepath_(std::move(filepath)) {}

        ModuleAST parse();

        // -- Tokens ---------------------------------------------------------------
        const TokenStream& tokens_;
        std::string filepath_;
        size_t current_ = 0;

        // While true, `Name {` is NOT parsed as a struct literal and `[T] {` is
        // not parsed as a typed array literal. Set while parsing the condition
        // of `if` / `while`, where the `{` opens the body. Reset to false inside
        // (), [] and {} so `if f(Point { 1, 2 }) { ... }` still works.
        bool no_struct_literal_ = false;

        inline SourceLocation loc() { return current_ < tokens_.size() ? tokens_[current_].location : tokens_.back().location; }
        inline bool is_at_end() const {
            return current_ >= tokens_.size() ||
                   (current_ < tokens_.size() && tokens_[current_].type == TokenType::EOF_TOKEN);
        }
        inline const Token& peek() const {
            return current_ < tokens_.size() ? tokens_[current_] : tokens_.back();
        }
        inline const Token& peek_next() const {
            return current_ + 1 < tokens_.size() ? tokens_[current_ + 1] : tokens_.back();
        }
        inline const Token& advance() { return tokens_[current_++]; }
        const Token& previous() const { return tokens_[current_ - 1]; }
        bool match(TokenType kind);
        bool check(TokenType kind) const;
        bool accept(TokenType kind);
        Token expect(TokenType kind, const std::string& msg);

        // -- Doc comments & attributes ---------------------------------------------
        //
        // `///` lines and `#[...]` groups written before a declaration. They are
        // collected here first and attached once the declaration has been parsed.

        struct Metadata {
            std::string doc;
            bool has_doc = false;
            SourceLocation doc_location;            // first `///` line
            std::vector<Attribute> attributes;
            SourceLocation attribute_location;      // first `#[`

            bool empty() const { return !has_doc && attributes.empty(); }
        };

        // Metadata read while looking for `module`/`import` that turned out to
        // belong to the first real declaration.
        Metadata pending_meta_;

        Metadata parse_metadata();                              // any mix of `///` and `#[...]`
        void parse_attribute_group(std::vector<Attribute>& out);  // one `#[a, b(1)]`
        void check_dangling_metadata(const Metadata& meta);     // error if meta has nothing to attach to
        static void attach_metadata(Declaration& decl, Metadata&& meta);

        // -- AST Construction ------------------------------------------------------
        
        NamePtr parse_name();
        TypeExprPtr parse_type_expression();
        TypeExprPtr parse_return_type();        // `-> T`, or an implicit `void`
        BlockPtr parse_optional_body();         // `{ ... }`, or `;` for a body-less declaration
        std::vector<ExpressionPtr> parse_arguments();
        std::vector<VariableDeclPtr> parse_parameters();

        // -- Expression parsing ----------------------------------------------------
        //
        // Precedence (low -> high):
        //   assignment    =  +=  -=  *=  /=  %=  &=  |=  ^=  ~=  <<=  >>=
        //   ternary       ? :
        //   logical_or    ||
        //   logical_and   &&
        //   bitwise_or    |
        //   bitwise_xor   ^
        //   bitwise_and   &
        //   equality      ==  !=
        //   comparison    <  <=  >  >=
        //   shift         <<  >>
        //   term          +  -
        //   factor        *  /  %
        //   unary         -  +  !  ~          (prefix, right-associative)
        //   postfix       .  ::  ()  []  <>   (left-associative)
        //   primary       literals  names  lambda  new

        ExpressionPtr parse_expression();
        ExpressionPtr parse_assignment();
        ExpressionPtr parse_ternary();
        ExpressionPtr parse_logical_or();
        ExpressionPtr parse_logical_and();
        ExpressionPtr parse_bitwise_or();
        ExpressionPtr parse_bitwise_xor();
        ExpressionPtr parse_bitwise_and();
        ExpressionPtr parse_equality();
        ExpressionPtr parse_comparison();
        ExpressionPtr parse_shift();
        ExpressionPtr parse_term();
        ExpressionPtr parse_factor();
        ExpressionPtr parse_unary();
        ExpressionPtr parse_postfix();
        ExpressionPtr parse_primary();

        bool is_assignment_token(TokenType type);
        bool is_unary_token(TokenType type);

        // -- Primary helpers ------------------------------------------------------

        ExpressionPtr parse_array_literal();    // [a, b]   [T] { a, b }   [T; N] { a, b }
        ExpressionPtr parse_struct_literal(SourceLocation l, TypeExprPtr type_expr);  // T { a, b }  /  T { x: a, y: b }
        std::vector<ExpressionPtr> parse_braced_expressions(const char* what);       // { a, b, c }
        bool typed_array_literal_ahead() const;   // at '[': does the matching ']' come right before a '{'?

        // -- Statements -----------------------------------------------------------

        DeclarationPtr parse_declaration();
        StatementPtr parse_statement();
        BlockPtr parse_block();

        StatementPtr parse_if_statement();
        StatementPtr parse_while_statement();
        // StatementPtr parse_foreach_statement();

        VariableDeclPtr parse_variable_declaration(bool is_public = false);
        FunctionDeclPtr parse_function_declaration(bool is_public = false);
        StructFieldDeclPtr parse_struct_field_declaration(bool is_public = false);
        MethodDeclPtr parse_method_declaration(bool is_public = false, bool is_static = false);

        StructDeclPtr parse_struct_declaration(bool is_public = false);
        ImplDeclPtr parse_impl_declaration();

        // -- Headers --------------------------------------------------------------

        std::string parse_inner_doc();  // consecutive `//!` lines at the top of the file
        ModuleName parse_module_name(); // parse the module name
        ModuleName parse_import();      // parse one `import a::b;`

        void parse_header(ModuleAST& ast); // parse the header of the module, including module name and dependencies
    };
     
} // namespace xenon
