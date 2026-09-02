#pragma once

#include <memory>
#include <vector>
#include <string>

#include "common/dataclasses.h"

namespace xenon::ast {

    using common::SourceLocation;

    struct ASTNode {
       enum class NodeKind {
            // Literals
            LITERAL_INT,
            LITERAL_FLOAT,
            LITERAL_COMPLEX,
            LITERAL_STRING,
            LITERAL_BOOL,
            LITERAL_ARRAY,
            LITERAL_NULLPTR,
            LITERAL_CLASS,

            // Names / Access
            NAME,
            MEMBER_ACCESS_EXPR,
            CALL_EXPR,
            INDEX_EXPR,

            // Type Expressiosn
            NAMED_TYPE,
            POINTER_TYPE,
            REFERENCE_TYPE,
            ARRAY_TYPE,

            // Operations
            OPERATION_EXPR,
            TERNARY_EXPR,

            // Allocation
            NEW_EXPR,
            BOX_EXPR,
            DELETE_STMT,

            // Statements
            BLOCK_STMT,
            EXPRESSION_STMT,

            // Control flow
            IF_STMT,
            WHILE_STMT,
            FOREACH_STMT,

            // Jump
            RETURN_STMT,
            BREAK_STMT,
            CONTINUE_STMT,

            // Declarations
            VARIABLE_DECL,
            FUNCTION_DECL,
            OPERATOR_OVERLOAD_DECL,
            CLASS_FIELD_DECL,
            NAMESPACE_VARIABLE_DECL,
            CLASS_METHOD_DECL,
            CLASS_STRUCTURE_DECL,
            CLASS_IMPLEMENTATION_DECL,
            
            // Special
            EOF_STMT,
        } kind;
        SourceLocation location;
        explicit ASTNode(NodeKind k, SourceLocation l) : kind(k), location(std::move(l)) {}
        virtual ~ASTNode() = default;
    };

    // Selection, Iteration, and Sequence
    struct Statement : public ASTNode {
        explicit Statement(NodeKind k, SourceLocation l)
            : ASTNode(k, l) {}
    };

    using StatementPtr = std::unique_ptr<Statement>;

    // variable, function declarations, etc.
    struct Declaration : public Statement {
        explicit Declaration(NodeKind k, SourceLocation l)
            : Statement(k, l) {}
    };

    using DeclarationPtr = std::unique_ptr<Declaration>;

    // L and R values, expressions, etc.
    struct Expression : public ASTNode {
        explicit Expression(NodeKind k, SourceLocation l)
            : ASTNode(k, l) {}
    };

    using ExpressionPtr = std::unique_ptr<Expression>;


    // -- Literals ---------------------------------------------------------------

    enum class NumericSuffix { NONE, I8, I16, I32, I64, U8, U16, U32, U64, SIZE, F32, F64 };

    inline NumericSuffix parse_integer_suffix(const std::string& suf) {
        if (suf == "i8")   return NumericSuffix::I8;
        if (suf == "i16")  return NumericSuffix::I16;
        if (suf == "i32")  return NumericSuffix::I32;
        if (suf == "i64")  return NumericSuffix::I64;
        if (suf == "u8")   return NumericSuffix::U8;
        if (suf == "u16")  return NumericSuffix::U16;
        if (suf == "u32")  return NumericSuffix::U32;
        if (suf == "u64")  return NumericSuffix::U64;
        if (suf == "size") return NumericSuffix::SIZE;
        return NumericSuffix::NONE;
    }

    inline NumericSuffix parse_float_suffix(const std::string& suf) {
        if (suf == "f32")  return NumericSuffix::F32;
        if (suf == "f64")  return NumericSuffix::F64;
        if (suf == "f")    return NumericSuffix::F32;  // f is shorthand for f32
        if (suf == "d")    return NumericSuffix::F64;  // d is shorthand for f64
        return NumericSuffix::NONE;
    }

    struct LiteralInt : public Expression {
        std::string value;      // "42", "0xFF", "0b1010"
        NumericSuffix suffix;   // NONE for default (i32)
        
        LiteralInt(SourceLocation l, std::string v, NumericSuffix suf = NumericSuffix::NONE)
            : Expression(NodeKind::LITERAL_INT, std::move(l)), value(std::move(v)), suffix(suf) {}
    };

    struct LiteralFloat : public Expression {
        std::string value;      // "3.14", "1.0e9"
        NumericSuffix suffix;   // NONE for default (f64)
        
        LiteralFloat(SourceLocation l, std::string v, NumericSuffix suf = NumericSuffix::NONE)
            : Expression(NodeKind::LITERAL_FLOAT, std::move(l)), value(std::move(v)), suffix(suf) {}
    };

    struct LiteralString : public Expression {
        std::string value;
        explicit LiteralString(SourceLocation l, std::string v)
            : Expression(NodeKind::LITERAL_STRING, std::move(l)), value(std::move(v)) {}
    };

    struct LiteralBool : public Expression {
        bool value;
        explicit LiteralBool(SourceLocation l, bool v)
            : Expression(NodeKind::LITERAL_BOOL, std::move(l)), value(v) {}
    };

    struct LiteralNullptr : public Expression {
        explicit LiteralNullptr(SourceLocation l)
            : Expression(NodeKind::LITERAL_NULLPTR, std::move(l)) {}
    };

    struct LiteralArray : public Expression {
        std::vector<ExpressionPtr> elements;
        explicit LiteralArray(SourceLocation l, std::vector<ExpressionPtr> elems)
            : Expression(NodeKind::LITERAL_ARRAY, std::move(l)), elements(std::move(elems)) {}
    };
    

    // -- Names / Access ---------------------------------------------------------------

    struct Name;

    using NamePtr = std::unique_ptr<Name>;

    struct Name : public Expression {
        std::string identifier;
        NamePtr next;   // "::" chain
        
        Name(SourceLocation l, std::string id, NamePtr nxt = nullptr)
            : Expression(NodeKind::NAME, l), identifier(std::move(id)), next(std::move(nxt)) {}
        
        bool is_qualified() const { return next != nullptr; }
        std::string to_string() const {
            if (next) return identifier + "::" + next->to_string();
            return identifier;
        }
    };


    struct MemberAccessExpr : public Expression {
        ExpressionPtr object;
        NamePtr       member;
        MemberAccessExpr(SourceLocation l, ExpressionPtr obj, NamePtr mem)
            : Expression(NodeKind::MEMBER_ACCESS_EXPR, std::move(l)), object(std::move(obj)), member(std::move(mem)) {}
    };

    struct LiteralClass : public Expression {
        NamePtr struct_name;
        std::vector<ExpressionPtr> args;
        LiteralClass(SourceLocation l, NamePtr n, std::vector<ExpressionPtr> a)
            : Expression(NodeKind::LITERAL_CLASS, std::move(l)), struct_name(std::move(n)), args(std::move(a)) {}
    };

    struct CallExpr : public Expression {
        ExpressionPtr              callee;
        std::vector<ExpressionPtr> args;
        bool is_early_return = false;  // for "callee(...)?" syntax for later!
        CallExpr(SourceLocation l, ExpressionPtr c, std::vector<ExpressionPtr> a, bool early_ret = false)
            : Expression(NodeKind::CALL_EXPR, std::move(l)), callee(std::move(c)), args(std::move(a)), is_early_return(early_ret) {}
    };

    struct IndexExpr : public Expression {
        ExpressionPtr object;
        ExpressionPtr index;
        IndexExpr(SourceLocation l, ExpressionPtr obj, ExpressionPtr idx)
            : Expression(NodeKind::INDEX_EXPR, std::move(l)), object(std::move(obj)), index(std::move(idx)) {}
    };


    // -- Type Expressions ---------------------------------------------------------------

    // Base class for all type expressions in the AST
    struct TypeExpression : public ASTNode {
        explicit TypeExpression(NodeKind k, SourceLocation l)
            : ASTNode(k, l) {}
    };

    using TypeExprPtr = std::unique_ptr<TypeExpression>;

    // One type expression node for all named types
    struct NamedTypeExpr : public TypeExpression {
        NamePtr name;
        
        NamedTypeExpr(SourceLocation l, NamePtr n)
            : TypeExpression(NodeKind::NAMED_TYPE, l), name(std::move(n)) {}
    };

    // Pointer type (ptr T)
    struct PointerTypeExpr : public TypeExpression {
        TypeExprPtr element_type;
        bool is_mut;  // mut ptr T or ptr T
        bool is_box;  // box T or ptr T
        PointerTypeExpr(SourceLocation l, TypeExprPtr elem, bool mut = false, bool boxed = false)
            : TypeExpression(NodeKind::POINTER_TYPE, l), element_type(std::move(elem)), is_mut(mut), is_box(boxed) {}
    };

    // Reference type (ref T)
    struct ReferenceTypeExpr : public TypeExpression {
        TypeExprPtr element_type;
        bool is_mut;  // mut ref T or ref T
        ReferenceTypeExpr(SourceLocation l, TypeExprPtr elem, bool mut = false)
            : TypeExpression(NodeKind::REFERENCE_TYPE, l), element_type(std::move(elem)), is_mut(mut) {}
    };

    // Array type ([T; N] or [T])
    struct ArrayTypeExpr : public TypeExpression {
        TypeExprPtr element_type;
        ExpressionPtr size_expr;  // can be nullptr for dynamic arrays
        ArrayTypeExpr(SourceLocation l, TypeExprPtr elem, ExpressionPtr size = nullptr)
            : TypeExpression(NodeKind::ARRAY_TYPE, l), element_type(std::move(elem)),
                size_expr(std::move(size)) {}
    };


    // -- Operations ---------------------------------------------------------------

    enum class OperatorKind {
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        MODULO,
        ADD_ASSIGN,
        SUBTRACT_ASSIGN,
        MULTIPLY_ASSIGN,
        DIVIDE_ASSIGN,
        MODULO_ASSIGN,
        BITWISE_AND,
        BITWISE_OR,
        BITWISE_XOR,
        SHIFT_LEFT,
        SHIFT_RIGHT,
        BITWISE_AND_ASSIGN,
        BITWISE_OR_ASSIGN,
        BITWISE_XOR_ASSIGN,
        SHIFT_LEFT_ASSIGN,
        SHIFT_RIGHT_ASSIGN,
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        LESS_EQUAL,
        GREATER_THAN,
        GREATER_EQUAL,
        LOGICAL_AND,
        LOGICAL_OR,
        LOGICAL_NOT,
        ASSIGN,                     // =
        NEGATE, BITWISE_NOT, ADDRESS_OF, DEREFERENCE,
        INDEX_READ
    };

    struct OperationExpr : public Expression {
        OperatorKind op;
        ExpressionPtr  lhs; // nullptr when unary
        ExpressionPtr  rhs;

        bool is_binary() const { return lhs != nullptr; }
        OperationExpr(SourceLocation l, OperatorKind o, ExpressionPtr left, ExpressionPtr right)
            : Expression(NodeKind::OPERATION_EXPR, std::move(l)), op(o), lhs(std::move(left)), rhs(std::move(right)) {}
    };

    struct TernaryExpr : public Expression {
        ExpressionPtr condition;
        ExpressionPtr then_branch;
        ExpressionPtr else_branch;

        TernaryExpr(SourceLocation l, ExpressionPtr cond, ExpressionPtr then_b, ExpressionPtr else_b)
            : Expression(NodeKind::TERNARY_EXPR, std::move(l)), condition(std::move(cond)),
                then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
    };


    // -- Allocation ---------------------------------------------------------------

    struct NewExpr : public Expression {
        ExpressionPtr expr;
        NewExpr(SourceLocation l, ExpressionPtr e)
            : Expression(NodeKind::NEW_EXPR, std::move(l)), expr(std::move(e)) {}
    };

    struct DeleteStmt : public Statement {
        ExpressionPtr target;
        DeleteStmt(SourceLocation l, ExpressionPtr t)
            : Statement(NodeKind::DELETE_STMT, std::move(l)), target(std::move(t)) {}
    };


    // -- Statements ---------------------------------------------------------------

    struct BlockStmt : public Statement {
        std::vector<StatementPtr> statements;
        explicit BlockStmt(SourceLocation l, std::vector<StatementPtr> stmts)
            : Statement(NodeKind::BLOCK_STMT, std::move(l)), statements(std::move(stmts)) {}
    };

    using BlockPtr = std::unique_ptr<BlockStmt>;

    struct ExpressionStmt : public Statement {
        ExpressionPtr expr;
        explicit ExpressionStmt(SourceLocation l, ExpressionPtr e)
            : Statement(NodeKind::EXPRESSION_STMT, std::move(l)), expr(std::move(e)) {}
    };


    // -- Control Flow ---------------------------------------------------------------

    struct IfStmt : public Statement {
        ExpressionPtr condition;
        StatementPtr  then_branch;
        StatementPtr  else_branch;  // can be nullptr
        IfStmt(SourceLocation l, ExpressionPtr cond, StatementPtr then_b, StatementPtr else_b = nullptr)
            : Statement(NodeKind::IF_STMT, std::move(l)), condition(std::move(cond)), then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
    };

    struct WhileStmt : public Statement {
        ExpressionPtr condition;
        StatementPtr  body;
        WhileStmt(SourceLocation l, ExpressionPtr cond, StatementPtr b)
            : Statement(NodeKind::WHILE_STMT, std::move(l)), condition(std::move(cond)), body(std::move(b)) {}
    };

    // -- Jump Statements ---------------------------------------------------------------

    struct ReturnStmt : public Statement {
        ExpressionPtr return_value;  // can be nullptr
        explicit ReturnStmt(SourceLocation l, ExpressionPtr val = nullptr)
            : Statement(NodeKind::RETURN_STMT, std::move(l)), return_value(std::move(val)) {}
    };

    struct BreakStmt : public Statement {
        explicit BreakStmt(SourceLocation l)
            : Statement(NodeKind::BREAK_STMT, std::move(l)) {}
    };

    struct ContinueStmt : public Statement {
        explicit ContinueStmt(SourceLocation l)
            : Statement(NodeKind::CONTINUE_STMT, std::move(l)) {}
    };


    // -- Declarations ---------------------------------------------------------------

    struct VariableDecl : public Declaration {
        std::string name;
        TypeExprPtr type_expr;  // can be nullptr
        ExpressionPtr init_expr;  // can be nullptr
        bool is_mut;
        bool is_public;
        VariableDecl(SourceLocation l, std::string n, TypeExprPtr t = nullptr, ExpressionPtr i = nullptr, bool m = false, bool p = false)
            : Declaration(NodeKind::VARIABLE_DECL, std::move(l)), name(std::move(n)), type_expr(std::move(t)), init_expr(std::move(i)), is_mut(m), is_public(p) {}
    };

    using VariableDeclPtr = std::unique_ptr<VariableDecl>;

    struct FunctionDecl : public Declaration {
        std::string name;
        std::vector<VariableDeclPtr> parameters;
        TypeExprPtr return_type;  // can be nullptr
        BlockPtr body;          // can be nullptr (only in header)
        bool is_public;
        FunctionDecl(SourceLocation l, std::string n, std::vector<VariableDeclPtr> params, TypeExprPtr ret_type = nullptr, BlockPtr b = nullptr, bool p = false)
            : Declaration(NodeKind::FUNCTION_DECL, std::move(l)), name(std::move(n)), parameters(std::move(params)), return_type(std::move(ret_type)), body(std::move(b)), is_public(p) {}
    };

    using FunctionDeclPtr = std::unique_ptr<FunctionDecl>;

    constexpr std::array<std::pair<const char*, OperatorKind>, 27> OPERATOR_MAP = {{
        {"+", OperatorKind::ADD},
        {"-", OperatorKind::SUBTRACT},
        {"*", OperatorKind::MULTIPLY},
        {"/", OperatorKind::DIVIDE},
        {"%", OperatorKind::MODULO},
        {"+=", OperatorKind::ADD_ASSIGN},
        {"-=", OperatorKind::SUBTRACT_ASSIGN},
        {"*=", OperatorKind::MULTIPLY_ASSIGN},
        {"/=", OperatorKind::DIVIDE_ASSIGN},
        {"%=", OperatorKind::MODULO_ASSIGN},
        {"&", OperatorKind::BITWISE_AND},
        {"|", OperatorKind::BITWISE_OR},
        {"^", OperatorKind::BITWISE_XOR},
        {"<<", OperatorKind::SHIFT_LEFT},
        {">>", OperatorKind::SHIFT_RIGHT},
        {"&=", OperatorKind::BITWISE_AND_ASSIGN},
        {"|=", OperatorKind::BITWISE_OR_ASSIGN},
        {"^=", OperatorKind::BITWISE_XOR_ASSIGN},
        {"<<=", OperatorKind::SHIFT_LEFT_ASSIGN},
        {">>=", OperatorKind::SHIFT_RIGHT_ASSIGN},
        {"==", OperatorKind::EQUAL},
        {"!=", OperatorKind::NOT_EQUAL},
        {"<",  OperatorKind::LESS_THAN},
        {"<=", OperatorKind::LESS_EQUAL},
        {">",  OperatorKind::GREATER_THAN},
        {">=", OperatorKind::GREATER_EQUAL},
        {"[]", OperatorKind::INDEX_READ},
    }};

    constexpr std::array<std::pair<const char*, OperatorKind>, 2> UNARY_OPERATOR_MAP = {{
        {"-", OperatorKind::NEGATE},
        {"~", OperatorKind::BITWISE_NOT},
    }};

    constexpr std::optional<OperatorKind> lookup_operator_overload(std::string_view op, bool is_binary) {
        if (is_binary) {
            for (const auto& [key, value] : OPERATOR_MAP) {
                if (key == op) return value;
            }
        } else {
            for (const auto& [key, value] : UNARY_OPERATOR_MAP) {
                if (key == op) return value;
            }
        }
        return std::nullopt;
    }


    struct OperatorOverloadDecl : public Declaration {
        OperatorKind op_kind;
        std::vector<VariableDeclPtr> parameters;
        TypeExprPtr return_type;
        BlockPtr body;
        bool is_public;
        OperatorOverloadDecl(SourceLocation l, OperatorKind op,
                            std::vector<VariableDeclPtr> params,
                            TypeExprPtr ret_type = nullptr,
                            BlockPtr b = nullptr,
                            bool p = false)
            : Declaration(NodeKind::OPERATOR_OVERLOAD_DECL, std::move(l)),
            op_kind(op), parameters(std::move(params)),
            return_type(std::move(ret_type)), body(std::move(b)), is_public(p) {}
    };

    using OperatorOverloadDeclPtr = std::unique_ptr<OperatorOverloadDecl>;

    struct ClassFieldDecl : public Declaration {
        std::string name;
        TypeExprPtr type_expr;
        bool is_public;
        ClassFieldDecl(SourceLocation l, std::string n,
                    TypeExprPtr t = nullptr,
                    bool p = false)
            : Declaration(NodeKind::CLASS_FIELD_DECL, std::move(l)),
            name(std::move(n)), type_expr(std::move(t)), is_public(p) {}
    };

    using ClassFieldDeclPtr = std::unique_ptr<ClassFieldDecl>;

    struct NamespaceVarDecl : public Declaration {
        std::string name;
        TypeExprPtr type_expr;
        ExpressionPtr initialiser;
        bool is_mut;
        bool is_public;
        NamespaceVarDecl(SourceLocation l, std::string n,
                        TypeExprPtr t = nullptr,
                        ExpressionPtr i = nullptr,
                        bool m = false,
                        bool p = false)
            : Declaration(NodeKind::NAMESPACE_VARIABLE_DECL, std::move(l)),
            name(std::move(n)), type_expr(std::move(t)),
            initialiser(std::move(i)), is_mut(m), is_public(p) {}
    };

    using NamespaceVarDeclPtr = std::unique_ptr<NamespaceVarDecl>;

    struct ClassMethodDecl : public Declaration {
        std::string name;
        std::vector<VariableDeclPtr> parameters;
        TypeExprPtr return_type;
        BlockPtr body;
        bool is_public;
        bool is_static;
        ClassMethodDecl(SourceLocation l, std::string n,
                        std::vector<VariableDeclPtr> params,
                        TypeExprPtr ret_type = nullptr,
                        BlockPtr b = nullptr,
                        bool p = false,
                        bool s = false)
            : Declaration(NodeKind::CLASS_METHOD_DECL, std::move(l)),
            name(std::move(n)), parameters(std::move(params)),
            return_type(std::move(ret_type)), body(std::move(b)),
            is_public(p), is_static(s) {}
    };

    using ClassMethodDeclPtr = std::unique_ptr<ClassMethodDecl>;

    struct ClassStructureDecl : public Declaration {
        std::string name;
        std::vector<ClassFieldDeclPtr> fields;
        bool is_public;
        ClassStructureDecl(SourceLocation l, std::string n,
                        std::vector<ClassFieldDeclPtr> f,
                        bool p = false)
            : Declaration(NodeKind::CLASS_STRUCTURE_DECL, std::move(l)),
            name(std::move(n)), fields(std::move(f)), is_public(p) {}
    };

    using ClassStructureDeclPtr = std::unique_ptr<ClassStructureDecl>;

    struct ClassImplementationDecl : public Declaration {
        NamePtr class_name;
        std::vector<NamespaceVarDeclPtr> static_vars;
        std::vector<ClassMethodDeclPtr> methods;
        std::vector<OperatorOverloadDeclPtr> operator_overloads;
        std::vector<ClassMethodDeclPtr> constructors;
        ClassMethodDeclPtr destructor;

        ClassImplementationDecl(SourceLocation l, NamePtr n,
                                std::vector<NamespaceVarDeclPtr> sv = {},
                                std::vector<ClassMethodDeclPtr> m = {},
                                std::vector<OperatorOverloadDeclPtr> o = {},
                                std::vector<ClassMethodDeclPtr> c = {},
                                ClassMethodDeclPtr d = nullptr)
            : Declaration(NodeKind::CLASS_IMPLEMENTATION_DECL, std::move(l)),
            class_name(std::move(n)), static_vars(std::move(sv)),
            methods(std::move(m)), operator_overloads(std::move(o)),
            constructors(std::move(c)), destructor(std::move(d)) {}
    };

    using ClassImplementationDeclPtr = std::unique_ptr<ClassImplementationDecl>;


    struct EOFStmt : public Statement {
        explicit EOFStmt(SourceLocation l)
            : Statement(NodeKind::EOF_STMT, std::move(l)) {}
    };


    struct ASTRootElement {
        std::vector<StatementPtr> declarations;
    };


    struct ModuleAST {
        std::string module_name;
        std::vector<std::string> dependencies;  // names of modules this module depends on
        ASTRootElement root;
    };


}
