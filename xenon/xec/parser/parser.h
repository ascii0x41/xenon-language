#pragma once

#include "common/dataclasses.h"
#include "tokens/tokens.h"
#include "ast/astlib.h"
#include "common/diagnostics.h"

#include <string>

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

        // -- AST Construction ------------------------------------------------------
        
        NamePtr parse_name();
        TypeExprPtr parse_type_expression();

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

        // -- Primary helpers ------------------------------------------------------

        ExpressionPtr parse_array_literal();
        ExpressionPtr parse_new_expr();
        ExpressionPtr parse_class_literal(SourceLocation l, NamePtr struct_name);
//        ExpressionPtr parse_box_expr();

        // -- Statements -----------------------------------------------------------

        DeclarationPtr parse_declaration();
        StatementPtr parse_statement();
        BlockPtr parse_block();

        StatementPtr parse_if_statement();
        StatementPtr parse_while_statement();
        // StatementPtr parse_foreach_statement();

        VariableDeclPtr parse_variable_declaration(bool is_public = false);
        FunctionDeclPtr parse_function_declaration(bool is_public = false);
        OperatorOverloadDeclPtr parse_operator_overload_declaration(bool is_public = false);

        ClassFieldDeclPtr parse_class_field_declaration(bool is_public = false);
        ClassMethodDeclPtr parse_class_method_declaration(bool is_public = false, bool is_static = false);
        NamespaceVarDeclPtr parse_namespace_var_declaration(bool is_public = false);

        ClassStructureDeclPtr parse_class_structure_declaration(bool is_public = false);
        ClassImplementationDeclPtr parse_class_implementation_declaration();

        // -- Headers --------------------------------------------------------------

        std::string parse_module_name(); // parse the module name
        std::vector<std::string> parse_dependencies(); // parse the dependencies of the module
        
        void parse_header(ModuleAST& ast); // parse the header of the module, including module name and dependencies
    };
     
} // namespace xenon
