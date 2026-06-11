#pragma once

#include "common/dataclasses.h"
#include "common/diagnostics.h"
#include "tokens/tokens.h"
#include "ast/astlib.h"

#include <vector>
#include <string>
#include <unordered_map>

namespace xenon {

    class Parser {
    public:

        static ParserResult parse(const TokenStream& tokens, std::string filepath) {
            return Parser(tokens, std::move(filepath)).parse();
        }

    private:
        // -- State ----------------------------------------------------------------
        const TokenStream& tokens_;
        std::string filepath_;
        size_t idx_ = 0;
        bool   had_errors_  = false;
        size_t error_count_ = 0;
        static constexpr size_t MAX_ERRORS = 20;

        std::vector<ImportDecl> imports_;
        std::vector<ExportDecl> exports_;
        
        // Track enclosing scopes so synchronise() doesn't steal their closing delimiters
        std::vector<TokenType> expected_close_delims_;

        // Single-token buffer for splitting >> into > >
        bool  split_gt_pending_ = false;
        const Token split_gt_token_{ TokenType::EOF_TOKEN, "", {0, 0, ""}};

        explicit Parser(const TokenStream& tokens, std::string filepath)
        : tokens_(tokens), filepath_(std::move(filepath)) {}

        // Parse the full token stream into a top-level block.
        ParserResult parse();

        // -- Navigation -----------------------------------------------------------

        const Token& peek() const;
        const Token& peek_next() const;
        const Token& previous() const;
        bool is_at_end() const;
        Token advance();
        Token expect(TokenType type, const char* msg);
        bool accept(TokenType type);
        SourceLocation get_location() const;

        // -- Error recovery -------------------------------------------------------

        // Hard error: report and synchronise (for truly unexpected tokens)
        void report_and_synchronise(const CompilerException& e);
        
        // Hard error with stop delimiter: don't consume the delimiter
        void report_and_synchronise(const CompilerException& e, TokenType stop_delim);
        
        // synchronise without consuming the stop delimiter
        void synchronise(TokenType stop_delim);
        
        // Top-level synchronise (no delimiter)
        void synchronise();
        
        // Soft recovery: skip to next statement boundary without stack unwinding
        void recover_to_next_statement();
        
        // Soft expect for semicolons: report if missing, recover locally
        bool expect_semicolon(const char* context_msg);

        // -- >> disambiguation ----------------------------------------------------

        void consume_gt_gt_as_gt();
        void close_angle(const char* msg);

        // -- Directives -----------------------------------------------------------

        Directives parse_directives();

        // -- Operator helpers -----------------------------------------------------

        BinaryOp token_to_binary_op(TokenType t) const;
        UnaryOp token_to_unary_op(TokenType t) const;
//      OverloadableOp token_to_overloadable_op(TokenType t, bool is_unary) const;

        // -- Type parsing ---------------------------------------------------------

        TypePtr parse_type();
        std::vector<TypePtr> parse_type_args();

        // -- Generic parameter/argument parsing -----------------------------------

        GenericParameters parse_generic_params();
        GenericArguments parse_generic_arguments();
        std::vector<TraitConstraint> parse_trait_bounds();

        // -- Parameter parsing ----------------------------------------------------

        Parameters parse_parameters();

        // -- Name parsing ---------------------------------------------------------

        NamePtr parse_name();

        // -- Expression parsing ---------------------------------------------------
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
        ExpressionPtr parse_map_literal();
        ExpressionPtr parse_tuple_literal();
        ExpressionPtr parse_interp_string();
        ExpressionPtr parse_new_expr();       // 'new' already consumed
        ExpressionPtr parse_lambda();         // 'lambda' already consumed
        ExpressionPtr parse_group_or_tuple(); // '(' already consumed, disambiguates

        // -- Call argument parsing ------------------------------------------------

        std::vector<ExpressionPtr> parse_call_args();

        // -- Lookahead heuristics ------------------------------------------------

        bool looks_like_generic_args(const ExpressionPtr& lhs) const;

        // -- Statement parsing ----------------------------------------------------

        ConstructPtr parse_statement();
        BlockPtr parse_block();

        // -- Declaration parsing --------------------------------------------------

        Ptr<VariableDecl> parse_variable_decl(bool is_mutable, bool is_static, Directives dirs);
//      Ptr<DestructureDecl> parse_destructure_decl(bool is_mutable, Directives dirs);

        Ptr<FunctionDecl> parse_function_decl(bool is_static, bool is_mut, Directives dirs);
//      Ptr<OperatorOverloadDecl> parse_operator_overload(bool is_mut, Directives dirs);
        Ptr<ClassDecl> parse_class_decl(Directives dirs);
        Ptr<TraitDecl> parse_trait_decl(Directives dirs);
        Ptr<ImplDecl> parse_impl_decl(Directives dirs);
        Ptr<TypeAliasDecl> parse_type_alias_decl(Directives dirs);
        Ptr<EnumDecl> parse_enum_decl(Directives dirs);
//      Ptr<ScopeDecl> parse_scope_decl(Directives dirs);

        // -- Control flow ---------------------------------------------------------

        ConstructPtr parse_if_stmt();
        ConstructPtr parse_while_stmt();
        ConstructPtr parse_do_while_stmt();
        ConstructPtr parse_foreach_stmt();
        ConstructPtr parse_match_stmt();

        // -- Heap deallocation ---------------------------------------------------------

        ConstructPtr parse_delete_stmt();

        // -- Exception handling ---------------------------------------------------

        ConstructPtr parse_try_catch_stmt();
        ConstructPtr parse_throw_stmt();

        // -- Jump statements ------------------------------------------------------

        ConstructPtr parse_return_stmt();
        ConstructPtr parse_break_stmt();
        ConstructPtr parse_continue_stmt();

        // -- Module system --------------------------------------------------------

        void parse_import_decl();    // side-effects imports_ vector
        void parse_export_decl();    // side-effects exports_ vector
        void parse_module_decl();    // side-effects: consumes module declaration line

        std::string parse_operator_name(SourceLocation& op_loc);
        OverloadableOp resolve_overloadable_op(const std::string& op_name) const;
        bool looks_like_generic_args_ahead() const;
        
        // -- Scope tracking helpers -----------------------------------------------
        
        void enter_scope(TokenType close_delim);
        void leave_scope();
    };

} // namespace xenon