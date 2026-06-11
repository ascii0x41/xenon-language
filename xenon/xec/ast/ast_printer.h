#pragma once

#include "astlib.h"
#include <iostream>
#include <sstream>

namespace xenon {

class ASTPrinter {
public:
    static void print_ast(const ParserResult& result, std::ostream& out = std::cout, int indent_size = 4) {
        ASTPrinter(out, indent_size).print(result);
    }

private:
    explicit ASTPrinter(std::ostream& out, int indent_size)
    : out_(out), indent_size_(indent_size) {}
    
    void print(const ParserResult& result) {
        auto& block = *result.ast;
        for (size_t i = 0; i < block.statements.size(); ++i) {
            writeln("stmt[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            visit(*block.statements[i]);
            indent_ -= indent_size_;
        }

        auto& imps = result.imports;
        auto& exps = result.exports;

        if (imps.empty()) {
            writeln("[No Imports]");
        } else {
            writeln("[Imports]");
            for (const auto& imp : imps) {
                visit_import_decl(imp);                
            }
        }

        if (exps.empty()) {
            writeln("[No Exports]");
        } else {
            writeln("[Exports]");
            for (const auto& exp : exps) {
                visit_export_decl(exp);
            }
        }
    }

    std::ostream& out_;
    int indent_size_;
    int indent_ = 0;

    void write_indent() {
        for (int i = 0; i < indent_; ++i)
            out_ << ' ';
    }

    void writeln(const std::string& line) {
        write_indent();
        out_ << line << '\n';
    }

    std::string format_location(const SourceLocation& loc) const {
        return loc.file + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column);
    }

    // ---- Visitor dispatch ----
    void visit(const ASTNode& node) {

        switch (node.kind) {
            // Literals
            case ASTNode::NodeKind::LITERAL_INT:          visit_literal_int(static_cast<const LiteralInt&>(node)); break;
            case ASTNode::NodeKind::LITERAL_FLOAT:        visit_literal_float(static_cast<const LiteralFloat&>(node)); break;
            case ASTNode::NodeKind::LITERAL_COMPLEX:      visit_literal_complex(static_cast<const LiteralComplex&>(node)); break;
            case ASTNode::NodeKind::LITERAL_STRING:       visit_literal_string(static_cast<const LiteralString&>(node)); break;
            case ASTNode::NodeKind::LITERAL_RAW_STRING:   visit_literal_raw_string(static_cast<const LiteralRawString&>(node)); break;
            case ASTNode::NodeKind::LITERAL_INTERP_STRING: visit_literal_interp_string(static_cast<const LiteralInterpString&>(node)); break;
            case ASTNode::NodeKind::LITERAL_BOOL:         visit_literal_bool(static_cast<const LiteralBool&>(node)); break;
            case ASTNode::NodeKind::LITERAL_ARRAY:        visit_literal_array(static_cast<const LiteralArray&>(node)); break;
            case ASTNode::NodeKind::LITERAL_TUPLE:        visit_literal_tuple(static_cast<const LiteralTuple&>(node)); break;
            case ASTNode::NodeKind::LITERAL_MAP:          visit_literal_map(static_cast<const LiteralMap&>(node)); break;
            case ASTNode::NodeKind::LITERAL_NULLPTR:      visit_literal_nullptr(static_cast<const LiteralNullptr&>(node)); break;

            // Names / Access
            case ASTNode::NodeKind::NAME:                visit_name(static_cast<const Name&>(node)); break;
            case ASTNode::NodeKind::MEMBER_ACCESS_EXPR:  visit_member_access(static_cast<const MemberAccessExpr&>(node)); break;
            case ASTNode::NodeKind::CALL_EXPR:           visit_call_expr(static_cast<const CallExpr&>(node)); break;
            case ASTNode::NodeKind::INDEX_EXPR:          visit_index_expr(static_cast<const IndexExpr&>(node)); break;

            // Operations
            case ASTNode::NodeKind::BINARY_EXPR:    visit_binary_expr(static_cast<const BinaryExpr&>(node)); break;
            case ASTNode::NodeKind::UNARY_EXPR:     visit_unary_expr(static_cast<const UnaryExpr&>(node)); break;
            case ASTNode::NodeKind::TERNARY_EXPR:   visit_ternary_expr(static_cast<const TernaryExpr&>(node)); break;

            // Allocation
            case ASTNode::NodeKind::NEW_EXPR:       visit_new_expr(static_cast<const NewExpr&>(node)); break;
            case ASTNode::NodeKind::DELETE_STMT:    visit_delete_stmt(static_cast<const DeleteStmt&>(node)); break;

            // Lambda
            case ASTNode::NodeKind::LAMBDA_EXPR:    visit_lambda(static_cast<const Lambda&>(node)); break;

            // Statements
            case ASTNode::NodeKind::EXPRESSION_STMT:   visit_expression_stmt(static_cast<const ExpressionStmt&>(node)); break;
            case ASTNode::NodeKind::IF_STMT:           visit_if_stmt(static_cast<const IfStmt&>(node)); break;
            case ASTNode::NodeKind::IF_LET_STMT:       visit_if_let_stmt(static_cast<const IfLetStmt&>(node)); break;
            case ASTNode::NodeKind::WHILE_STMT:        visit_while_stmt(static_cast<const WhileStmt&>(node)); break;
            case ASTNode::NodeKind::DO_WHILE_STMT:     visit_do_while_stmt(static_cast<const DoWhileStmt&>(node)); break;
            case ASTNode::NodeKind::FOREACH_STMT:      visit_foreach_stmt(static_cast<const ForeachStmt&>(node)); break;
            case ASTNode::NodeKind::MATCH_STMT:        visit_match_stmt(static_cast<const MatchStmt&>(node)); break;
            case ASTNode::NodeKind::RETURN_STMT:       visit_return_stmt(static_cast<const ReturnStmt&>(node)); break;
            case ASTNode::NodeKind::BREAK_STMT:        visit_break_stmt(static_cast<const BreakStmt&>(node)); break;
            case ASTNode::NodeKind::CONTINUE_STMT:     visit_continue_stmt(static_cast<const ContinueStmt&>(node)); break;
            case ASTNode::NodeKind::THROW_STMT:        visit_throw_stmt(static_cast<const ThrowStmt&>(node)); break;
            case ASTNode::NodeKind::TRY_CATCH_STMT:    visit_try_catch_stmt(static_cast<const TryCatchStmt&>(node)); break;

            // Declarations
            case ASTNode::NodeKind::VARIABLE_DECL:          visit_variable_decl(static_cast<const VariableDecl&>(node)); break;
            case ASTNode::NodeKind::FUNCTION_DECL:          visit_function_decl(static_cast<const FunctionDecl&>(node)); break;
            case ASTNode::NodeKind::CONSTRUCTOR_DECL:       visit_constructor_decl(static_cast<const ConstructorDecl&>(node)); break;
            case ASTNode::NodeKind::DESTRUCTOR_DECL:        visit_destructor_decl(static_cast<const DestructorDecl&>(node)); break;
//          case ASTNode::NodeKind::OPERATOR_OVERLOAD_DECL: visit_operator_overload(static_cast<const OperatorOverloadDecl&>(node)); break;
            case ASTNode::NodeKind::CLASS_DECL:             visit_class_decl(static_cast<const ClassDecl&>(node)); break;
            case ASTNode::NodeKind::IMPL_DECL:              visit_impl_decl(static_cast<const ImplDecl&>(node)); break;
            case ASTNode::NodeKind::TRAIT_DECL:             visit_trait_decl(static_cast<const TraitDecl&>(node)); break;
            case ASTNode::NodeKind::TYPE_ALIAS_DECL:        visit_type_alias_decl(static_cast<const TypeAliasDecl&>(node)); break;
            case ASTNode::NodeKind::ENUM_DECL:              visit_enum_decl(static_cast<const EnumDecl&>(node)); break;
//          case ASTNode::NodeKind::SCOPE_DECL:             visit_scope_decl(static_cast<const ScopeDecl&>(node)); break;
            default: writeln("<<unknown node>>"); break;
        }
    }

    // ---- Type visitors ----
    void visit_type(const Type& type) {
        switch (type.kind) {
            case Type::TypeKind::VALUE: {
                auto& vt = static_cast<const ValueType&>(type);
                writeln("ValueType:");
                indent_ += indent_size_;
                writeln("name: " + vt.name->format());
                indent_ -= indent_size_;
                break;
            }
            case Type::TypeKind::RAW_PTR: {
                auto& pt = static_cast<const RawPointerType&>(type);
                writeln("RawPointerType:");
                indent_ += indent_size_;
                writeln(std::string("mut: ") + (pt.is_mut ? "true" : "false"));
                visit_type(*pt.inner_type);
                indent_ -= indent_size_;
                break;
            }
            case Type::TypeKind::BOX_PTR: {
                auto& bpt = static_cast<const BoxPointerType&>(type);
                writeln("BoxPointerType:");
                indent_ += indent_size_;
                writeln(std::string("mut: ") + (bpt.is_mut ? "true" : "false"));
                visit_type(*bpt.inner_type);
                indent_ -= indent_size_;
                break;
            }
            case Type::TypeKind::REF: {
                auto& rt = static_cast<const ReferenceType&>(type);
                writeln("ReferenceType:");
                indent_ += indent_size_;
                writeln(std::string("mut: ") + (rt.is_mut ? "true" : "false"));
                visit_type(*rt.inner_type);
                indent_ -= indent_size_;
                break;
            }
            case Type::TypeKind::CALLABLE: {
                auto& ct = static_cast<const CallableType&>(type);
                writeln("CallableType:");
                indent_ += indent_size_;
                writeln("params:");
                indent_ += indent_size_;
                for (size_t i = 0; i < ct.param_t.size(); ++i) {
                    writeln("[" + std::to_string(i) + "]:");
                    indent_ += indent_size_;
                    visit_type(*ct.param_t[i]);
                    indent_ -= indent_size_;
                }
                indent_ -= indent_size_;
                writeln("return:");
                indent_ += indent_size_;
                visit_type(*ct.return_t);
                indent_ -= indent_size_;
                indent_ -= indent_size_;
                break;
            }
            case Type::TypeKind::STATIC_ARRAY: {
                auto& sa = static_cast<const StaticArrayType&>(type);
                writeln("StaticArrayType:");
                indent_ += indent_size_;
                writeln("item_type:");
                indent_ += indent_size_;
                visit_type(*sa.item_t);
                indent_ -= indent_size_;
                writeln("size:");
                indent_ += indent_size_;
                visit(*sa.size_expr);
                indent_ -= indent_size_;
                indent_ -= indent_size_;
                break;
            }
            case Type::TypeKind::DYNAMIC_ARRAY: {
                auto& da = static_cast<const DynamicArrayType&>(type);
                writeln("DynamicArrayType:");
                indent_ += indent_size_;
                writeln("item_type:");
                indent_ += indent_size_;
                visit_type(*da.item_t);
                indent_ -= indent_size_;
                indent_ -= indent_size_;
                break;
            }
            default: {
                writeln("<<unknown type>>");
                break;
            }
        }
    }

    // ---- Literal visitors ----
    void visit_literal_int(const LiteralInt& lit) {
        writeln("LiteralInt: " + lit.value + "  [" + format_location(lit.location) + "]");
    }

    void visit_literal_float(const LiteralFloat& lit) {
        writeln("LiteralFloat: " + lit.value + "  [" + format_location(lit.location) + "]");
    }

    void visit_literal_complex(const LiteralComplex& lit) {
        writeln("LiteralComplex: " + lit.value + "  [" + format_location(lit.location) + "]");
    }

    void visit_literal_string(const LiteralString& lit) {
        writeln("LiteralString: \"" + lit.value + "\"  [" + format_location(lit.location) + "]");
    }

    void visit_literal_raw_string(const LiteralRawString& lit) {
        writeln("LiteralRawString: r\"" + lit.value + "\"  [" + format_location(lit.location) + "]");
    }

    void visit_literal_interp_string(const LiteralInterpString& lit) {
        writeln("LiteralInterpString: [" + format_location(lit.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < lit.parts.size(); ++i) {
            const auto& part = lit.parts[i];
            if (part.is_expr) {
                writeln("expr[" + std::to_string(i) + "]:");
                indent_ += indent_size_;
                visit(*part.expr);
                indent_ -= indent_size_;
            } else {
                writeln("text[" + std::to_string(i) + "]: \"" + part.text + "\"");
            }
        }
        indent_ -= indent_size_;
    }

    void visit_literal_bool(const LiteralBool& lit) {
        writeln(std::string("LiteralBool: ") + (lit.value ? "true" : "false") + "  [" + format_location(lit.location) + "]");
    }

    void visit_literal_array(const LiteralArray& lit) {
        writeln("LiteralArray: [" + format_location(lit.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < lit.elements.size(); ++i) {
            writeln("element[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            visit(*lit.elements[i]);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_literal_tuple(const LiteralTuple& lit) {
        writeln("LiteralTuple: [" + format_location(lit.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < lit.elements.size(); ++i) {
            writeln("element[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            visit(*lit.elements[i]);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_literal_map(const LiteralMap& lit) {
        writeln("LiteralMap: [" + format_location(lit.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < lit.pairs.size(); ++i) {
            writeln("pair[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            writeln("key:");
            indent_ += indent_size_;
            visit(*lit.pairs[i].first);
            indent_ -= indent_size_;
            writeln("value:");
            indent_ += indent_size_;
            visit(*lit.pairs[i].second);
            indent_ -= indent_size_;
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_literal_nullptr(const LiteralNullptr& lit) {
        writeln("LiteralNullptr [" + format_location(lit.location) + "]");
    }

    // ---- Name / Access visitors ----
    void visit_name(const Name& name) {
        writeln("Name: " + name.base + "  [" + format_location(name.location) + "]");
        if (!name.generics.empty()) {
            indent_ += indent_size_;
            writeln("generics:");
            indent_ += indent_size_;
            for (size_t i = 0; i < name.generics.params.size(); ++i) {
                writeln("arg[" + std::to_string(i) + "]:");
                indent_ += indent_size_;
                visit_generic_arg(name.generics.params[i]);
                indent_ -= indent_size_;
            }
            indent_ -= indent_size_;
            indent_ -= indent_size_;
        }
        if (name.next) {
            indent_ += indent_size_;
            writeln("next:");
            indent_ += indent_size_;
            visit(*name.next);
            indent_ -= indent_size_;
            indent_ -= indent_size_;
        }
    }

    void visit_generic_arg(const GenericArg& arg) {
        if (std::holds_alternative<ExpressionPtr>(arg)) {
            visit(*std::get<ExpressionPtr>(arg));
        } else {
            visit_type(*std::get<TypePtr>(arg));
        }
    }

    void visit_member_access(const MemberAccessExpr& expr) {
        writeln("MemberAccessExpr: [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("object:");
        indent_ += indent_size_;
        visit(*expr.object);
        indent_ -= indent_size_;
        writeln("member:");
        indent_ += indent_size_;
        visit(*expr.member);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_call_expr(const CallExpr& expr) {
        writeln("CallExpr: [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("callee:");
        indent_ += indent_size_;
        visit(*expr.callee);
        indent_ -= indent_size_;
        writeln("args:");
        indent_ += indent_size_;
        for (size_t i = 0; i < expr.args.size(); ++i) {
            writeln("arg[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            visit(*expr.args[i]);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_index_expr(const IndexExpr& expr) {
        writeln("IndexExpr: [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("object:");
        indent_ += indent_size_;
        visit(*expr.object);
        indent_ -= indent_size_;
        writeln("index:");
        indent_ += indent_size_;
        visit(*expr.index);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    // ---- Operation visitors ----
    void visit_binary_expr(const BinaryExpr& expr) {
        writeln("BinaryExpr: " + binary_op_to_string(expr.op) + " [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("left:");
        indent_ += indent_size_;
        visit(*expr.left);
        indent_ -= indent_size_;
        writeln("right:");
        indent_ += indent_size_;
        visit(*expr.right);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_unary_expr(const UnaryExpr& expr) {
        writeln("UnaryExpr: " + unary_op_to_string(expr.op) + " [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("operand:");
        indent_ += indent_size_;
        visit(*expr.operand);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_ternary_expr(const TernaryExpr& expr) {
        writeln("TernaryExpr: [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("condition:");
        indent_ += indent_size_;
        visit(*expr.condition);
        indent_ -= indent_size_;
        writeln("then:");
        indent_ += indent_size_;
        visit(*expr.then_expr);
        indent_ -= indent_size_;
        writeln("else:");
        indent_ += indent_size_;
        visit(*expr.else_expr);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_new_expr(const NewExpr& expr) {
        writeln("NewExpr: [" + format_location(expr.location) + "]");
        indent_ += indent_size_;
        writeln("alloc_type:");
        indent_ += indent_size_;
        visit_type(*expr.alloc_type);
        indent_ -= indent_size_;
        if (!expr.ctor_args.empty()) {
            writeln("ctor_args:");
            indent_ += indent_size_;
            for (size_t i = 0; i < expr.ctor_args.size(); ++i) {
                writeln("arg[" + std::to_string(i) + "]:");
                indent_ += indent_size_;
                visit(*expr.ctor_args[i]);
                indent_ -= indent_size_;
            }
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_delete_stmt(const DeleteStmt& stmt) {
        writeln("DeleteStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("expr:");
        indent_ += indent_size_;
        visit(*stmt.expr);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_lambda(const Lambda& lambda) {
        writeln("Lambda: [" + format_location(lambda.location) + "]");
        indent_ += indent_size_;
        if (!lambda.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : lambda.generic_params) {
                writeln("- " + gp.name + (gp.has_bounds() ? " (bounded)" : ""));
            }
            indent_ -= indent_size_;
        }
        writeln("params:");
        indent_ += indent_size_;
        for (size_t i = 0; i < lambda.params.size(); ++i) {
            const auto& p = lambda.params.params[i];
            writeln("param[" + std::to_string(i) + "]: " + p.name);
            if (p.type) {
                indent_ += indent_size_;
                visit_type(*p.type);
                indent_ -= indent_size_;
            }
        }
        indent_ -= indent_size_;
        if (lambda.return_type) {
            writeln("return_type:");
            indent_ += indent_size_;
            visit_type(*lambda.return_type);
            indent_ -= indent_size_;
        }
        writeln("body:");
        indent_ += indent_size_;
        if (std::holds_alternative<ExpressionPtr>(lambda.body)) {
            visit(*std::get<ExpressionPtr>(lambda.body));
        } else {
            visit_block(*std::get<BlockPtr>(lambda.body));
        }
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    // ---- Statement visitors ----
    void visit_block(const Block& block) {
        writeln("Block: [" + format_location(block.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < block.statements.size(); ++i) {
            writeln("stmt[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            visit(*block.statements[i]);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_expression_stmt(const ExpressionStmt& stmt) {
        writeln("ExpressionStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        visit(*stmt.expr);
        indent_ -= indent_size_;
    }

    void visit_if_stmt(const IfStmt& stmt) {
        writeln("IfStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("if:");
        indent_ += indent_size_;
        writeln("condition:");
        indent_ += indent_size_;
        visit(*stmt.if_branch.condition);
        indent_ -= indent_size_;
        writeln("body:");
        indent_ += indent_size_;
        visit_block(*stmt.if_branch.body);
        indent_ -= indent_size_;
        indent_ -= indent_size_;

        for (size_t i = 0; i < stmt.elif_branches.size(); ++i) {
            writeln("elif[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            writeln("condition:");
            indent_ += indent_size_;
            visit(*stmt.elif_branches[i].condition);
            indent_ -= indent_size_;
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*stmt.elif_branches[i].body);
            indent_ -= indent_size_;
            indent_ -= indent_size_;
        }

        if (stmt.else_body) {
            writeln("else:");
            indent_ += indent_size_;
            visit_block(*stmt.else_body);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_if_let_stmt(const IfLetStmt& stmt) {
        writeln("IfLetStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("binding: " + stmt.binding);
        writeln("value:");
        indent_ += indent_size_;
        visit(*stmt.value);
        indent_ -= indent_size_;
        writeln("body:");
        indent_ += indent_size_;
        visit_block(*stmt.body);
        indent_ -= indent_size_;
        if (stmt.else_body) {
            writeln("else:");
            indent_ += indent_size_;
            visit_block(*stmt.else_body);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_while_stmt(const WhileStmt& stmt) {
        writeln("WhileStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("condition:");
        indent_ += indent_size_;
        visit(*stmt.condition);
        indent_ -= indent_size_;
        writeln("body:");
        indent_ += indent_size_;
        visit_block(*stmt.body);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_do_while_stmt(const DoWhileStmt& stmt) {
        writeln("DoWhileStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("body:");
        indent_ += indent_size_;
        visit_block(*stmt.body);
        indent_ -= indent_size_;
        writeln("condition:");
        indent_ += indent_size_;
        visit(*stmt.condition);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_foreach_stmt(const ForeachStmt& stmt) {
        writeln("ForeachStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("iter_name: " + stmt.iter_name);
        if (stmt.iter_type) {
            writeln("iter_type:");
            indent_ += indent_size_;
            visit_type(*stmt.iter_type);
            indent_ -= indent_size_;
        }
        writeln("iterable:");
        indent_ += indent_size_;
        visit(*stmt.iterable);
        indent_ -= indent_size_;
        writeln("body:");
        indent_ += indent_size_;
        visit_block(*stmt.body);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_match_stmt(const MatchStmt& stmt) {
        writeln("MatchStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("subject:");
        indent_ += indent_size_;
        visit(*stmt.subject);
        indent_ -= indent_size_;
        writeln("arms:");
        indent_ += indent_size_;
        for (size_t i = 0; i < stmt.arms.size(); ++i) {
            writeln("arm[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            if (stmt.arms[i].pattern) {
                writeln("pattern:");
                indent_ += indent_size_;
                visit(*stmt.arms[i].pattern);
                indent_ -= indent_size_;
            } else {
                writeln("pattern: _");
            }
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*stmt.arms[i].body);
            indent_ -= indent_size_;
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_return_stmt(const ReturnStmt& stmt) {
        writeln("ReturnStmt: [" + format_location(stmt.location) + "]");
        if (stmt.value) {
            indent_ += indent_size_;
            visit(*stmt.value);
            indent_ -= indent_size_;
        }
    }

    void visit_break_stmt(const BreakStmt& stmt) {
        writeln("BreakStmt: [" + format_location(stmt.location) + "]");
    }

    void visit_continue_stmt(const ContinueStmt& stmt) {
        writeln("ContinueStmt: [" + format_location(stmt.location) + "]");
    }

    void visit_throw_stmt(const ThrowStmt& stmt) {
        writeln("ThrowStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        visit(*stmt.exception);
        indent_ -= indent_size_;
    }

    void visit_try_catch_stmt(const TryCatchStmt& stmt) {
        writeln("TryCatchStmt: [" + format_location(stmt.location) + "]");
        indent_ += indent_size_;
        writeln("try:");
        indent_ += indent_size_;
        visit_block(*stmt.try_body);
        indent_ -= indent_size_;
        for (size_t i = 0; i < stmt.catches.size(); ++i) {
            writeln("catch[" + std::to_string(i) + "]: " + stmt.catches[i].binding);
            if (stmt.catches[i].exception_type) {
                indent_ += indent_size_;
                visit_type(*stmt.catches[i].exception_type);
                indent_ -= indent_size_;
            }
            indent_ += indent_size_;
            visit_block(*stmt.catches[i].body);
            indent_ -= indent_size_;
        }
        if (stmt.finally_body) {
            writeln("finally:");
            indent_ += indent_size_;
            visit_block(*stmt.finally_body);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    // ---- Declaration visitors ----
    void visit_variable_decl(const VariableDecl& decl) {
        writeln("VariableDecl: " + decl.name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        writeln(std::string("mutable: ") + (decl.is_mutable ? "true" : "false"));
        writeln(std::string("static: ") + (decl.is_static ? "true" : "false"));
        if (decl.type) {
            writeln("type:");
            indent_ += indent_size_;
            visit_type(*decl.type);
            indent_ -= indent_size_;
        }
        if (decl.initialiser) {
            writeln("initialiser:");
            indent_ += indent_size_;
            visit(*decl.initialiser);
            indent_ -= indent_size_;
        }
        if (!decl.dirs.empty()) {
            print_directives(decl.dirs);
        }
        indent_ -= indent_size_;
    }

    void visit_function_decl(const FunctionDecl& decl) {
        writeln("FunctionDecl: " + decl.name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        writeln(std::string("static: ") + (decl.is_static ? "true" : "false"));
        writeln(std::string("mut: ") + (decl.is_mut ? "true" : "false"));
        if (!decl.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : decl.generic_params) {
                std::string s = gp.name;
                if (gp.has_bounds()) {
                    s += " : ";
                    for (size_t i = 0; i < gp.bounds.size(); ++i) {
                        if (i > 0) s += " + ";
                        s += gp.bounds[i].trait_name;
                    }
                }
                writeln("- " + s);
            }
            indent_ -= indent_size_;
        }
        writeln("params:");
        indent_ += indent_size_;
        for (size_t i = 0; i < decl.params.size(); ++i) {
            const auto& p = decl.params.params[i];
            writeln("param[" + std::to_string(i) + "]: " + p.name);
            if (p.type) {
                indent_ += indent_size_;
                visit_type(*p.type);
                indent_ -= indent_size_;
            }
            if (p.default_value) {
                indent_ += indent_size_;
                writeln("default:");
                indent_ += indent_size_;
                visit(*p.default_value);
                indent_ -= indent_size_;
                indent_ -= indent_size_;
            }
        }
        indent_ -= indent_size_;
        if (decl.return_type) {
            writeln("return_type:");
            indent_ += indent_size_;
            visit_type(*decl.return_type);
            indent_ -= indent_size_;
        }
        if (decl.body) {
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*decl.body);
            indent_ -= indent_size_;
        }
        if (!decl.dirs.empty()) {
            print_directives(decl.dirs);
        }
        indent_ -= indent_size_;
    }

    /*
    void visit_operator_overload(const OperatorOverloadDecl& decl) {
        writeln("OperatorOverloadDecl: " + overloadable_op_to_string(decl.op) + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        writeln(std::string("mut: ") + (decl.is_mut ? "true" : "false"));
        writeln("params:");
        indent_ += indent_size_;
        for (size_t i = 0; i < decl.params.size(); ++i) {
            writeln("param[" + std::to_string(i) + "]: " + decl.params.params[i].name);
            if (decl.params.params[i].type) {
                indent_ += indent_size_;
                visit_type(*decl.params.params[i].type);
                indent_ -= indent_size_;
            }
        }
        indent_ -= indent_size_;
        if (decl.return_type) {
            writeln("return_type:");
            indent_ += indent_size_;
            visit_type(*decl.return_type);
            indent_ -= indent_size_;
        }
        if (decl.body) {
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*decl.body);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }
    */
   
    void visit_class_decl(const ClassDecl& decl) {
        writeln("ClassDecl: " + decl.name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        if (!decl.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : decl.generic_params) {
                writeln("- " + gp.name);
            }
            indent_ -= indent_size_;
        }
        if (!decl.fields.empty()) {
            writeln("fields:");
            indent_ += indent_size_;
            for (const auto& f : decl.fields) {
                std::string line = f->name;
                if (f->is_public) line = "pub " + line;
                writeln(line);
                if (f->type) {
                    indent_ += indent_size_;
                    visit_type(*f->type);
                    indent_ -= indent_size_;
                }
            }
            indent_ -= indent_size_;
        }
        if (!decl.constructors.empty()) {
            writeln("constructors:");
            indent_ += indent_size_;
            for (const auto& ctor : decl.constructors) {
                visit_constructor_decl(*ctor);
            }
            indent_ -= indent_size_;
        }
        if (decl.destructor.has_value()) {
            writeln("destructor:");
            indent_ += indent_size_;
            visit_destructor_decl(*decl.destructor.value());
            indent_ -= indent_size_;
        }
        if (!decl.methods.empty()) {
            writeln("methods:");
            indent_ += indent_size_;
            for (const auto& m : decl.methods) {
                visit_method(*m);
            }
            indent_ -= indent_size_;
        }
        if (!decl.dirs.empty()) {
            print_directives(decl.dirs);
        }
        indent_ -= indent_size_;
    }

    void visit_constructor_decl(const ConstructorDecl& decl) {
        writeln("ConstructorDecl [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        writeln(std::string("public: ") + (decl.is_public ? "true" : "false"));
        if (!decl.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : decl.generic_params) {
                std::string s = gp.name;
                if (gp.has_bounds()) {
                    s += " : ";
                    for (size_t i = 0; i < gp.bounds.size(); ++i) {
                        if (i > 0) s += " + ";
                        s += gp.bounds[i].trait_name;
                    }
                }
                writeln("- " + s);
            }
            indent_ -= indent_size_;
        }
        writeln("params:");
        indent_ += indent_size_;
        for (size_t i = 0; i < decl.params.size(); ++i) {
            const auto& p = decl.params.params[i];
            writeln("param[" + std::to_string(i) + "]: " + p.name);
            if (p.type) {
                indent_ += indent_size_;
                visit_type(*p.type);
                indent_ -= indent_size_;
            }
        }
        indent_ -= indent_size_;
        if (decl.body) {
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*decl.body);
            indent_ -= indent_size_;
        }
        if (!decl.dirs.empty()) {
            print_directives(decl.dirs);
        }
        indent_ -= indent_size_;
    }

    void visit_destructor_decl(const DestructorDecl& decl) {
        writeln("DestructorDecl [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        if (decl.body) {
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*decl.body);
            indent_ -= indent_size_;
        }
        if (!decl.dirs.empty()) {
            print_directives(decl.dirs);
        }
        indent_ -= indent_size_;
    }

    void visit_method(const Method& method) {
        writeln("Method: " + method.name + " [" + format_location(method.location) + "]");
        indent_ += indent_size_;
        writeln(std::string("public: ") + (method.is_public ? "true" : "false"));
        writeln(std::string("static: ") + (method.is_static ? "true" : "false"));
        writeln(std::string("mut: ") + (method.is_mut ? "true" : "false"));
        if (!method.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : method.generic_params) {
                std::string s = gp.name;
                if (gp.has_bounds()) {
                    s += " : ";
                    for (size_t i = 0; i < gp.bounds.size(); ++i) {
                        if (i > 0) s += " + ";
                        s += gp.bounds[i].trait_name;
                    }
                }
                writeln("- " + s);
            }
            indent_ -= indent_size_;
        }
        writeln("params:");
        indent_ += indent_size_;
        for (size_t i = 0; i < method.params.size(); ++i) {
            const auto& p = method.params.params[i];
            writeln("param[" + std::to_string(i) + "]: " + p.name);
            if (p.type) {
                indent_ += indent_size_;
                visit_type(*p.type);
                indent_ -= indent_size_;
            }
            if (p.default_value) {
                indent_ += indent_size_;
                writeln("default:");
                indent_ += indent_size_;
                visit(*p.default_value);
                indent_ -= indent_size_;
                indent_ -= indent_size_;
            }
        }
        indent_ -= indent_size_;
        if (method.return_type) {
            writeln("return_type:");
            indent_ += indent_size_;
            visit_type(*method.return_type);
            indent_ -= indent_size_;
        }
        if (method.body) {
            writeln("body:");
            indent_ += indent_size_;
            visit_block(*method.body);
            indent_ -= indent_size_;
        }
        if (!method.dirs.empty()) {
            print_directives(method.dirs);
        }
        indent_ -= indent_size_;
    }

    void visit_impl_decl(const ImplDecl& decl) {
        writeln("ImplDecl [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        
        // Target type
        writeln("implementing trait" + decl.trait_name->format() + " for type " + decl.target_type->format());

        // Generic parameters
        if (!decl.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : decl.generic_params) {
                std::string s = gp.name;
                if (gp.has_bounds()) {
                    s += " : ";
                    for (size_t i = 0; i < gp.bounds.size(); ++i) {
                        if (i > 0) s += " + ";
                        s += gp.bounds[i].trait_name;
                    }
                }
                writeln("- " + s);
            }
            indent_ -= indent_size_;
        }

        // Methods
        if (!decl.methods.empty()) {
            writeln("methods:");
            indent_ += indent_size_;
            for (size_t i = 0; i < decl.methods.size(); ++i) {
                const auto& m = decl.methods[i];
                writeln("method[" + std::to_string(i) + "]: " + m->name +
                        (m->is_public ? " (pub)" : "") +
                        (m->is_static ? " (static)" : "") +
                        (m->is_mut ? " (mut)" : ""));
                if (!m->generic_params.empty()) {
                    indent_ += indent_size_;
                    writeln("generic_params:");
                    indent_ += indent_size_;
                    for (const auto& gp : m->generic_params) {
                        writeln("- " + gp.name);
                    }
                    indent_ -= indent_size_;
                    indent_ -= indent_size_;
                }
                indent_ += indent_size_;
                writeln("params:");
                indent_ += indent_size_;
                for (size_t j = 0; j < m->params.size(); ++j) {
                    const auto& p = m->params.params[j];
                    writeln("param[" + std::to_string(j) + "]: " + p.name);
                    if (p.type) {
                        indent_ += indent_size_;
                        visit_type(*p.type);
                        indent_ -= indent_size_;
                    }
                }
                indent_ -= indent_size_;
                if (m->return_type) {
                    writeln("return_type:");
                    indent_ += indent_size_;
                    visit_type(*m->return_type);
                    indent_ -= indent_size_;
                }
                if (m->body) {
                    writeln("body:");
                    indent_ += indent_size_;
                    visit_block(*m->body);
                    indent_ -= indent_size_;
                }
                indent_ -= indent_size_;
            }
            indent_ -= indent_size_;
        }

        // Operators
        if (!decl.operators.empty()) {
            writeln("operators:");
            indent_ += indent_size_;
            for (size_t i = 0; i < decl.operators.size(); ++i) {
                const auto& op = decl.operators[i];
                writeln("operator[" + std::to_string(i) + "]: " +
                        overloadable_op_to_string(op->op) +
                        (op->is_public ? " (pub)" : "") +
                        (op->is_mut ? " (mut)" : ""));
                indent_ += indent_size_;
                writeln("params:");
                indent_ += indent_size_;
                for (size_t j = 0; j < op->params.size(); ++j) {
                    const auto& p = op->params.params[j];
                    writeln("param[" + std::to_string(j) + "]: " + p.name);
                    if (p.type) {
                        indent_ += indent_size_;
                        visit_type(*p.type);
                        indent_ -= indent_size_;
                    }
                }
                indent_ -= indent_size_;
                if (op->return_type) {
                    writeln("return_type:");
                    indent_ += indent_size_;
                    visit_type(*op->return_type);
                    indent_ -= indent_size_;
                }
                if (op->body) {
                    writeln("body:");
                    indent_ += indent_size_;
                    visit_block(*op->body);
                    indent_ -= indent_size_;
                }
                indent_ -= indent_size_;
            }
            indent_ -= indent_size_;
        }

        if (!decl.dirs.empty()) {
            print_directives(decl.dirs);
        }
        indent_ -= indent_size_;
    }

    void visit_trait_decl(const TraitDecl& decl) {
        writeln("TraitDecl: " + decl.name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        if (!decl.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : decl.generic_params) {
                writeln("- " + gp.name);
            }
            indent_ -= indent_size_;
        }
        if (!decl.method_reqs.empty()) {
            writeln("methods:");
            indent_ += indent_size_;
            for (const auto& m : decl.method_reqs) {
                std::string s = m.name;
                if (m.is_mut) s = "mut " + s;
                writeln("- " + s);
            }
            indent_ -= indent_size_;
        }
        if (!decl.operator_reqs.empty()) {
            writeln("operators:");
            indent_ += indent_size_;
            for (const auto& op : decl.operator_reqs) {
                writeln("- " + overloadable_op_to_string(op.op));
            }
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }

    void visit_type_alias_decl(const TypeAliasDecl& decl) {
        writeln("TypeAliasDecl: " + decl.alias_name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        if (!decl.generic_params.empty()) {
            writeln("generic_params:");
            indent_ += indent_size_;
            for (const auto& gp : decl.generic_params) {
                writeln("- " + gp.name);
            }
            indent_ -= indent_size_;
        }
        writeln("target:");
        indent_ += indent_size_;
        visit_type(*decl.target_type);
        indent_ -= indent_size_;
        indent_ -= indent_size_;
    }

    void visit_enum_decl(const EnumDecl& decl) {
        writeln("EnumDecl: " + decl.name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        for (const auto& v : decl.variants) {
            writeln("- " + v.name);
        }
        indent_ -= indent_size_;
    }

    /*
    void visit_scope_decl(const ScopeDecl& decl) {
        writeln("ScopeDecl: " + decl.name + " [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < decl.members.size(); ++i) {
            writeln("member[" + std::to_string(i) + "]:");
            indent_ += indent_size_;
            visit(*decl.members[i]);
            indent_ -= indent_size_;
        }
        indent_ -= indent_size_;
    }
    */

    // ---- Module visitors ----
    void visit_import_decl(const ImportDecl& decl) {
        writeln("ImportDecl: " + decl.module_path + (decl.module_alias ? " as " + *decl.module_alias : "") +
                " [" + format_location(decl.location) + "]");
    }

    void visit_export_decl(const ExportDecl& decl) {
        writeln("ExportDecl: [" + format_location(decl.location) + "]");
        indent_ += indent_size_;
        for (size_t i = 0; i < decl.symbols.size(); ++i) {
            writeln("symbol[" + std::to_string(i) + "]: " + decl.symbols[i]->format());
        }
        indent_ -= indent_size_;
    }

    // ---- Helpers ----
    void print_directives(const Directives& dirs) {
        writeln("directives:");
        indent_ += indent_size_;
        for (const auto& d : dirs) {
            std::string s = "@" + d.name;
            if (!d.arguments.empty()) {
                s += "(...)";  // simplified
            }
            writeln(s);
        }
        indent_ -= indent_size_;
    }

    // ---- Operator name helpers ----
    static std::string binary_op_to_string(BinaryOp op) {
        switch (op) {
            case BinaryOp::ADD: return "+";
            case BinaryOp::SUBTRACT: return "-";
            case BinaryOp::MULTIPLY: return "*";
            case BinaryOp::DIVIDE: return "/";
            case BinaryOp::MODULO: return "%";
            case BinaryOp::BITWISE_AND: return "&";
            case BinaryOp::BITWISE_OR: return "|";
            case BinaryOp::BITWISE_XOR: return "^";
            case BinaryOp::LEFT_SHIFT: return "<<";
            case BinaryOp::RIGHT_SHIFT: return ">>";
            case BinaryOp::EQUAL: return "==";
            case BinaryOp::NOT_EQUAL: return "!=";
            case BinaryOp::LESS: return "<";
            case BinaryOp::LESS_EQUAL: return "<=";
            case BinaryOp::GREATER: return ">";
            case BinaryOp::GREATER_EQUAL: return ">=";
            case BinaryOp::ASSIGN: return "=";
            case BinaryOp::ADD_ASSIGN: return "+=";
            case BinaryOp::SUBTRACT_ASSIGN: return "-=";
            case BinaryOp::MULTIPLY_ASSIGN: return "*=";
            case BinaryOp::DIVIDE_ASSIGN: return "/=";
            case BinaryOp::MODULO_ASSIGN: return "%=";
            case BinaryOp::BITWISE_AND_ASSIGN: return "&=";
            case BinaryOp::BITWISE_OR_ASSIGN: return "|=";
            case BinaryOp::BITWISE_XOR_ASSIGN: return "^=";
            case BinaryOp::LEFT_SHIFT_ASSIGN: return "<<=";
            case BinaryOp::RIGHT_SHIFT_ASSIGN: return ">>=";
            case BinaryOp::INDEX: return "[]";
            case BinaryOp::FUNCTION_CALL: return "()";
            case BinaryOp::LOGICAL_AND: return "&&";
            case BinaryOp::LOGICAL_OR: return "||";
        }
        return "?";
    }

    static std::string unary_op_to_string(UnaryOp op) {
        switch (op) {
            case UnaryOp::UNARY_PLUS: return "+";
            case UnaryOp::UNARY_MINUS: return "-";
            case UnaryOp::LOGICAL_NOT: return "!";
            case UnaryOp::BITWISE_NOT: return "~";
            case UnaryOp::ADDRESS_OF: return "&";
            case UnaryOp::DEREFERENCE: return "*";
        }
        return "?";
    }

    static std::string overloadable_op_to_string(OverloadableOp op) {
        switch (op) {
            case OverloadableOp::UNARY_PLUS: return "unary+";
            case OverloadableOp::UNARY_MINUS: return "unary-";
            case OverloadableOp::BITWISE_NOT: return "~";
            case OverloadableOp::ADD: return "+";
            case OverloadableOp::SUBTRACT: return "-";
            case OverloadableOp::MULTIPLY: return "*";
            case OverloadableOp::DIVIDE: return "/";
            case OverloadableOp::MODULO: return "%";
            case OverloadableOp::BITWISE_AND: return "&";
            case OverloadableOp::BITWISE_OR: return "|";
            case OverloadableOp::BITWISE_XOR: return "^";
            case OverloadableOp::LEFT_SHIFT: return "<<";
            case OverloadableOp::RIGHT_SHIFT: return ">>";
            case OverloadableOp::EQUAL: return "==";
            case OverloadableOp::NOT_EQUAL: return "!=";
            case OverloadableOp::LESS: return "<";
            case OverloadableOp::LESS_EQUAL: return "<=";
            case OverloadableOp::GREATER: return ">";
            case OverloadableOp::GREATER_EQUAL: return ">=";
            case OverloadableOp::ADD_ASSIGN: return "+=";
            case OverloadableOp::SUBTRACT_ASSIGN: return "-=";
            case OverloadableOp::MULTIPLY_ASSIGN: return "*=";
            case OverloadableOp::DIVIDE_ASSIGN: return "/=";
            case OverloadableOp::MODULO_ASSIGN: return "%=";
            case OverloadableOp::BITWISE_AND_ASSIGN: return "&=";
            case OverloadableOp::BITWISE_OR_ASSIGN: return "|=";
            case OverloadableOp::BITWISE_XOR_ASSIGN: return "^=";
            case OverloadableOp::LEFT_SHIFT_ASSIGN: return "<<=";
            case OverloadableOp::RIGHT_SHIFT_ASSIGN: return ">>=";
            case OverloadableOp::INDEX_READ: return "[]";
            case OverloadableOp::INDEX_WRITE: return "[]=";
        }
        return "?";
    }
};

} // namespace xenon