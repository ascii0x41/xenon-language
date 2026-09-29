#pragma once

#include "common/dataclasses.h"
#include "driver/compiler_driver.h"
#include "semantic/types.h"
#include "semantic/scope.h"

#include <unordered_set>

namespace xenon {
    namespace driver {
        class ModuleNamespaceTree;
        struct Module;
    }
}

namespace xenon::semantic {
    using common::SourceLocation;
    using namespace config;

    // Deliberately minimal: no CFG, no data-flow framework. A statement
    // either might fall through to whatever follows it, or every path
    // through it definitely reaches a `return`.
    enum class ControlFlowResult {
        FALLS_THROUGH,
        RETURNS
    };

    class SemanticAnalyser {
        const driver::ModuleNamespaceTree& namespace_tree_;
        const config::CompilerConfig& options_;
        TypeRegistry type_registry_;
        Scope global_scope_;
        Scope* current_scope_ = nullptr;

        // Set for the duration of validating a single function body, so
        // RETURN_STMT can check against it; restored afterwards (functions
        // don't nest, so a single field - not a stack - is enough).
        Type* current_function_return_type_ = nullptr;

        // Incremented/decremented around a while-body's validation so
        // break/continue can tell whether they're inside a loop at all.
        // This is the "temporary handling strategy" for break/continue -
        // it doesn't yet track which loop a `break` would target.
        int loop_depth_ = 0;

        // Scope only stores non-owning Symbol* (see scope.h), so whatever
        // creates a Symbol dynamically (e.g. a Variable for a `let`) has to
        // keep it alive somewhere - this is that somewhere, mirroring how
        // TypeRegistry owns the Types it hands out as raw pointers.
        std::vector<std::unique_ptr<Symbol>> symbols_;

        SemanticAnalyser(const driver::ModuleNamespaceTree& namespace_tree, const config::CompilerConfig& options);
        void warn(const std::string& message, const SourceLocation& loc) const;
        void error(const std::string& message, const SourceLocation& loc) const;
        Scope* module_scope_for_name(const std::string& name, const driver::Module& module);
        bool discover_module(const std::string& name, const driver::Module& module);
        bool validate_module(const std::string& name, const driver::Module& module);

        // Scope management. Kept deliberately minimal: current_scope_ is a
        // single pointer, not a stack, so entering a scope just has to hand
        // back whatever it overwrote for the caller to restore afterwards.
        Scope* enter_scope(const std::string& name);
        void leave_scope(Scope* previous_scope);

        // Declarations
        Type* resolve_let_binding_type(const std::string& name, const ast::TypeExprPtr& type_expr,
            const ast::ExpressionPtr& init_expr, const SourceLocation& loc, const char* what);
        bool validate_variable_decl(const ast::VariableDecl* var_decl);

        // Shared by validate_function/validate_class_method/validate_operator_overload:
        // enters a scope, registers the parameter list as Variables, validates
        // the body against return_type, then restores the previous scope.
        ControlFlowResult validate_callable_body(const std::string& scope_name,
            const std::vector<ast::VariableDeclPtr>& parameters, Type* return_type,
            const ast::BlockStmt* body, bool& parameters_ok);
        bool validate_function(const ast::FunctionDecl& function_decl);

        // Classes. Pass 1/pass 2 aware: validate_class_structure registers
        // (or reuses) its own Type before resolving fields, so a sibling
        // class's field referencing this one can find it once a future
        // module-level driver runs a registration pass before a field-
        // resolution pass. See the accompanying notes on the current
        // ordering limitation without that driver.
        bool validate_class_structure(const ast::ClassStructureDecl& class_decl);
        bool validate_class_implementation(const ast::ClassImplementationDecl& impl_decl);
        bool validate_recursive_value_layout_cycles();
        bool validate_class_static_var(Type* class_type, const ast::VariableDecl* static_var_decl);
        bool validate_class_method(Type* class_type, const ast::ClassMethodDecl* method_decl);
        bool validate_operator_overload(Type* class_type, const ast::OperatorOverloadDecl* op_decl);

        // True if 'from' recursively reaches 'target' through by-value
        // fields (a direct field of a user-defined type, or the element
        // type of a fixed-length array) - i.e. whether 'target' would end
        // up containing itself. Pointer/reference fields never count:
        // they don't embed the referent's bytes.
        bool type_depends_on_by_value(const Type* from, const Type* target, std::unordered_set<const Type*>& visited) const;

        bool validate_delete(const ast::DeleteStmt* delete_stmt);

        // Control flow
        ControlFlowResult validate_statement(const ast::Statement* stmt);
        ControlFlowResult validate_block(const ast::BlockStmt* block);
        ControlFlowResult validate_if_statement(const ast::IfStmt* if_stmt);
        ControlFlowResult validate_while_statement(const ast::WhileStmt* while_stmt);

        // Evaluation
        Type* get_operator_result_type(const Type* type, OperatorKind op_kind, const std::vector<const Type*>& arg_types, const SourceLocation& loc);
        Type* get_member_type(Type* type, const std::string& member_name, const SourceLocation& loc);

        // Resolves a call's argument list against a set of same-name
        // overloads (a free function is just a one-element list; a
        // method/static-method looks up its real overload set). When
        // skip_first_parameter is set, parameters[0] (the receiver, 'self')
        // is excluded from arity/type matching against argument_types,
        // since the receiver is never one of a CALL_EXPR's args.
        Type* resolve_call(const FunctionVariants& overloads, const std::vector<Type*>& argument_types,
            bool skip_first_parameter, const std::string& callee_description, const SourceLocation& loc);
        Type* resolve_call(const Function& overload, const std::vector<Type*>& argument_types,
            bool skip_first_parameter, const std::string& callee_description, const SourceLocation& loc);

        Type* resolve_type_expression(const ast::TypeExprPtr& type_expr);
        Type* evaluate_expression(const ast::ASTNode* ast, Type* expected_type = nullptr);

        bool is_assignable_expression(const ast::Expression* expr, Type*& out_type, bool& is_mutable);

    public:
        static bool validate(const driver::ModuleNamespaceTree& namespace_tree,
            const config::CompilerConfig& options);

        bool validate_modules();
    };

} // namespace xenon::semantic