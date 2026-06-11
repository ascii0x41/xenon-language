#pragma once

#include "common/dataclasses.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

namespace xenon {

    enum class BinaryOp {
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        MODULO,
        BITWISE_AND,
        BITWISE_OR,
        BITWISE_XOR,
        LEFT_SHIFT,
        RIGHT_SHIFT,
        EQUAL,
        NOT_EQUAL,
        LESS,
        LESS_EQUAL,
        GREATER,
        GREATER_EQUAL,
        ASSIGN,
        ADD_ASSIGN,
        SUBTRACT_ASSIGN,
        MULTIPLY_ASSIGN,
        DIVIDE_ASSIGN,
        MODULO_ASSIGN,
        BITWISE_AND_ASSIGN,
        BITWISE_OR_ASSIGN,
        BITWISE_XOR_ASSIGN,
        LEFT_SHIFT_ASSIGN,
        RIGHT_SHIFT_ASSIGN,
        INDEX,
        FUNCTION_CALL,
        LOGICAL_AND,
        LOGICAL_OR
    };

    enum class UnaryOp {
        UNARY_PLUS,
        UNARY_MINUS,
        LOGICAL_NOT,
        BITWISE_NOT,
        ADDRESS_OF,
        DEREFERENCE,
    };

    /**
     * Notes:
     *  - +, -, +, /, and % can return any type, but typically return the same type as their operands (e.g. int + int -> int, but int + float -> float)
     *  - ==, !=, <, <=, >, and >= must return bool, but can take any operand types (e.g. int == int -> bool, but int == string -> bool)
     *  [] is for rvalues (indexing) and []= is for lvalues (index assignment)
     */
    enum class OverloadableOp {
        // Unary (must return Self except NOT)
        UNARY_PLUS,   // operator+() -> Self
        UNARY_MINUS,  // operator-() -> Self
        BITWISE_NOT,  // operator~() -> Self
        
        // Note: LOGICAL_NOT (!) is NOT overloadable – only works on bool

        // Binary arithmetic (can return any type, but typically Self)
        ADD,        // operator+(other) -> Self
        SUBTRACT,   // operator-(other) -> Self
        MULTIPLY,   // operator*(other) -> Self
        DIVIDE,     // operator/(other) -> Self
        MODULO,     // operator%(other) -> Self
        
        // Bitwise (return Self)
        BITWISE_AND,  // operator&(other) -> Self
        BITWISE_OR,   // operator|(other) -> Self
        BITWISE_XOR,  // operator^(other) -> Self
        LEFT_SHIFT,   // operator<<(other) -> Self
        RIGHT_SHIFT,  // operator>>(other) -> Self
        
        // Comparisons (must return bool)
        EQUAL,         // operator==(other) -> bool
        NOT_EQUAL,     // operator!=(other) -> bool
        LESS,          // operator<(other) -> bool
        LESS_EQUAL,    // operator<=(other) -> bool
        GREATER,       // operator>(other) -> bool
        GREATER_EQUAL, // operator>=(other) -> bool
        
        // Compound assignment (return mut ref Self)
        ADD_ASSIGN,         // operator+=(other) -> mut ref Self
        SUBTRACT_ASSIGN,    // operator-=(other) -> mut ref Self
        MULTIPLY_ASSIGN,    // operator*=(other) -> mut ref Self
        DIVIDE_ASSIGN,      // operator/=(other) -> mut ref Self
        MODULO_ASSIGN,      // operator%=(other) -> mut ref Self
        BITWISE_AND_ASSIGN,   // operator&=(other) -> mut ref Self
        BITWISE_OR_ASSIGN,    // operator|=(other) -> mut ref Self
        BITWISE_XOR_ASSIGN,   // operator^=(other) -> mut ref Self
        LEFT_SHIFT_ASSIGN,    // operator<<=(other) -> mut ref Self
        RIGHT_SHIFT_ASSIGN,   // operator>>=(other) -> mut ref Self
        
        // Indexing (separate read/write)
        INDEX_READ,   // operator[](index) -> ref T
        INDEX_WRITE,  // operator[]=(index) -> mut ref T
    };

    constexpr std::array<std::pair<std::string_view, OverloadableOp>, 3> string_to_unary_overloadable_op = {{
        // Unary
        {"+", OverloadableOp::UNARY_PLUS},
        {"-", OverloadableOp::UNARY_MINUS},
        {"~", OverloadableOp::BITWISE_NOT},
    }};

    constexpr std::array<std::pair<std::string_view, OverloadableOp>, 30> string_to_binary_overloadable_op = {{
        // Binary arithmetic
        {"+", OverloadableOp::ADD},
        {"-", OverloadableOp::SUBTRACT},
        {"*", OverloadableOp::MULTIPLY},
        {"/", OverloadableOp::DIVIDE},
        {"%", OverloadableOp::MODULO},

        // Bitwise
        {"&", OverloadableOp::BITWISE_AND},
        {"|", OverloadableOp::BITWISE_OR},
        {"^", OverloadableOp::BITWISE_XOR},
        {"<<", OverloadableOp::LEFT_SHIFT},
        {">>", OverloadableOp::RIGHT_SHIFT},

        // Comparisons
        {"==", OverloadableOp::EQUAL},
        {"!=", OverloadableOp::NOT_EQUAL},
        {"<", OverloadableOp::LESS},
        {"<=", OverloadableOp::LESS_EQUAL},
        {">", OverloadableOp::GREATER},
        {">=", OverloadableOp::GREATER_EQUAL},

        // Compound assignment
        {"+=", OverloadableOp::ADD_ASSIGN},
        {"-=", OverloadableOp::SUBTRACT_ASSIGN},
        {"*=", OverloadableOp::MULTIPLY_ASSIGN},
        {"/=", OverloadableOp::DIVIDE_ASSIGN},
        {"%=", OverloadableOp::MODULO_ASSIGN},
        {"&=", OverloadableOp::BITWISE_AND_ASSIGN},
        {"|=", OverloadableOp::BITWISE_OR_ASSIGN},
        {"^=", OverloadableOp::BITWISE_XOR_ASSIGN},
        {"<<=", OverloadableOp::LEFT_SHIFT_ASSIGN},
        {">>=", OverloadableOp::RIGHT_SHIFT_ASSIGN},

        // Indexing
        {"[]", OverloadableOp::INDEX_READ},
        {"[]=", OverloadableOp::INDEX_WRITE},
    }};



    // Base AST node kind
    struct ASTNode {
        enum class NodeKind {
            // -- Type -----------------------------------------------------------------
            TYPE,

            // -- Literals -------------------------------------------------------------
            LITERAL_INT,
            LITERAL_FLOAT,
            LITERAL_COMPLEX,
            LITERAL_STRING,
            LITERAL_RAW_STRING,
            LITERAL_INTERP_STRING,
            LITERAL_BOOL,
            LITERAL_ARRAY,
            LITERAL_TUPLE,
            LITERAL_MAP,
            LITERAL_NULLPTR,

            // -- Names / Access -------------------------------------------------------
            NAME,
            MEMBER_ACCESS_EXPR,
            CALL_EXPR,
            INDEX_EXPR,

            // -- Operations -----------------------------------------------------------
            BINARY_EXPR,
            UNARY_EXPR,
            TERNARY_EXPR,

            // -- Allocation -----------------------------------------------------------
            NEW_EXPR,
            DELETE_STMT,

            // -- Lambda ---------------------------------------------------------------
            LAMBDA_EXPR,

            // -- Statements -----------------------------------------------------------
            BLOCK_STMT,
            EXPRESSION_STMT,

            // Control flow
            IF_STMT,
            IF_LET_STMT,        // if let binding = opt { ... }
            WHILE_STMT,
            DO_WHILE_STMT,
            FOREACH_STMT,
            MATCH_STMT,

            // Jump
            RETURN_STMT,
            BREAK_STMT,
            CONTINUE_STMT,
            THROW_STMT,

            // Exceptions
            TRY_CATCH_STMT,

            // -- Declarations ---------------------------------------------------------
            VARIABLE_DECL,
            DESTRUCTURE_DECL,   // let (a, b) = ...
            FUNCTION_DECL,
            CONSTRUCTOR_DECL,
            DESTRUCTOR_DECL,
//          OPERATOR_OVERLOAD_DECL,
            CLASS_DECL,
            IMPL_DECL,
            TRAIT_DECL,
            TYPE_ALIAS_DECL,
            ENUM_DECL,
//          SCOPE_DECL,

            // -- Module ---------------------------------------------------------------
            IMPORT_DECL,
            EXPORT_DECL,

            // -- Special --------------------------------------------------------------
            EOF_STMT,
        } kind;
        SourceLocation location;
        explicit ASTNode(NodeKind k, SourceLocation l) : kind(k), location(std::move(l)) {}
        virtual ~ASTNode() = default;
    };

    // Selection, Iteration, and Sequence
    struct Construct : public ASTNode {
        explicit Construct(NodeKind k, SourceLocation l)
            : ASTNode(k, l) {}
    };

    using ConstructPtr = Ptr<Construct>;

    // variable, function, class, enum, type alias, and trait declares
    struct Declaration : public Construct {
        explicit Declaration(NodeKind k, SourceLocation l)
            : Construct(k, l) {}
    };

    using DeclarationPtr = Ptr<Declaration>;

    struct Expression : public ASTNode {
        explicit Expression(NodeKind k, SourceLocation l)
            : ASTNode(k, l) {}
    };

    using ExpressionPtr = Ptr<Expression>;

    // --- Forward declarations for mutual references ---
    struct Name;
    struct FunctionDecl;
    struct ClassDecl;
    struct ConstructorDecl;
    struct DestructorDecl;
    struct VariableDecl;

    // ============================================================================
    // TYPE SYSTEM (separated from AST)
    // ============================================================================

    struct Type {
        enum class TypeKind {
            VALUE,
            RAW_PTR,
            BOX_PTR,
            REF,
            CALLABLE,
            STATIC_ARRAY,
            DYNAMIC_ARRAY
        } kind;
        SourceLocation location;
        explicit Type(TypeKind k, SourceLocation l): kind(k), location(std::move(l)) {}
        virtual ~Type() = default;
    };

    using TypePtr = Ptr<Type>;
    using NamePtr = Ptr<Name>;

    // T<Args...>
    struct ValueType : public Type {
        NamePtr name;
        explicit ValueType(SourceLocation l, NamePtr n_)
            : Type(TypeKind::VALUE, std::move(l)), name(std::move(n_)) {}
    };

    // (mut) ptr T
    struct RawPointerType : public Type {
        TypePtr inner_type;
        bool is_mut;
        explicit RawPointerType(SourceLocation l, TypePtr t_, bool m_ = false)
            : Type(TypeKind::RAW_PTR, std::move(l)), inner_type(std::move(t_)), is_mut(m_) {}
    };

    // (mut) box T
    struct BoxPointerType : public Type {
        TypePtr inner_type;
        bool is_mut;
        explicit BoxPointerType(SourceLocation l, TypePtr t_, bool m_ = false)
            : Type(TypeKind::BOX_PTR, std::move(l)), inner_type(std::move(t_)), is_mut(m_) {}
    };

    // (mut) ref T
    struct ReferenceType : public Type {
        TypePtr inner_type;
        bool is_mut;
        explicit ReferenceType(SourceLocation l, TypePtr t_, bool m_ = false)
            : Type(TypeKind::REF, std::move(l)), inner_type(std::move(t_)), is_mut(m_) {}
    };

    // (T1, T2) -> Ret
    struct CallableType : public Type {
        TypePtr return_t;
        std::vector<TypePtr> param_t;
        explicit CallableType(SourceLocation l, TypePtr rt_, std::vector<TypePtr> pt_)
            : Type(TypeKind::CALLABLE, std::move(l)), return_t(std::move(rt_)), param_t(std::move(pt_)) {}
    };

    // [T; N]
    struct StaticArrayType : public Type {
        TypePtr item_t;
        ExpressionPtr size_expr;
        explicit StaticArrayType(SourceLocation l, TypePtr it_, ExpressionPtr sx_)
            : Type(TypeKind::STATIC_ARRAY, std::move(l)), item_t(std::move(it_)), size_expr(std::move(sx_)) {}
    };

    // [T]
    struct DynamicArrayType : public Type {
        TypePtr item_t;
        explicit DynamicArrayType(SourceLocation l, TypePtr it_)
            : Type(TypeKind::DYNAMIC_ARRAY, std::move(l)), item_t(std::move(it_)) {}
    };

    // ============================================================================
    // GENERICS / TRAITS
    // ============================================================================

    using GenericArg = std::variant<ExpressionPtr, TypePtr>;

    struct GenericArguments {
        std::vector<GenericArg> params;
        SourceLocation open_bracket_loc;   // <
        SourceLocation close_bracket_loc;  // >

        bool empty() const { return params.empty(); }
    };

    struct Name : public Expression {
        std::string base;
        GenericArguments generics;
        NamePtr next = nullptr;   // "::" chain

        Name(SourceLocation l, std::string n_, GenericArguments g_ = {}, NamePtr nxt_ = nullptr)
            : Expression(NodeKind::NAME, l), base(std::move(n_)), generics(std::move(g_)), next(std::move(nxt_)) {}

        bool is_simple() const { return next == nullptr && generics.empty(); }
        std::string format() const {
            std::string s = base;
            if (!generics.empty()) { /* ... */ }
            if (next) s += "::" + next->format();
            return s;
        }
    };

    struct TraitConstraint {
        std::string   trait_name;
        GenericArguments   type_args;
        explicit TraitConstraint(std::string name, GenericArguments args = {})
            : trait_name(std::move(name)), type_args(std::move(args)) {}
    };

    struct GenericParam {
        std::string                  name;
        std::vector<TraitConstraint> bounds;
        explicit GenericParam(std::string n, std::vector<TraitConstraint> b = {})
            : name(std::move(n)), bounds(std::move(b)) {}
        bool has_bounds() const { return !bounds.empty(); }
    };

    struct GenericParameters {
        std::vector<GenericParam> params;
        SourceLocation open_bracket_loc;   // <
        SourceLocation close_bracket_loc;  // >

        bool empty() const { return params.empty(); }
        size_t size() const { return params.size(); }
        
        // Iterator passthrough
        auto begin() { return params.begin(); }
        auto end()   { return params.end(); }
        auto begin() const { return params.begin(); }
        auto end()   const { return params.end(); }
    };

    // ============================================================================
    // BLOCK (standalone, reused by statements/expressions)
    // ============================================================================

    struct Block {
        std::vector<ConstructPtr> statements;   // no nested blocks directly?
        SourceLocation location;
        Block(SourceLocation l, std::vector<ConstructPtr> s)
            : statements(std::move(s)), location(std::move(l)) {}
    };
    using BlockPtr = Ptr<Block>;

    // ============================================================================
    // DIRECTIVES (replaces attributes)
    // ============================================================================

    struct Directive {
        std::string                name;
        std::vector<ExpressionPtr> arguments;
        Directive(std::string n, std::vector<ExpressionPtr> args)
            : name(std::move(n)), arguments(std::move(args)) {}
    };
    using Directives = std::vector<Directive>;

    // ============================================================================
    // PARAMETERS
    // ============================================================================

    struct Param {
        std::string   name;
        TypePtr       type;
        ExpressionPtr default_value = nullptr;
        SourceLocation location;
    };

    struct Parameters {
        std::vector<Param> params;
        SourceLocation open_paren_loc; // (
        SourceLocation close_paren_loc; // )
        
        bool empty() const { return params.empty(); }
        size_t size() const { return params.size(); }
        
        // Iterator passthrough
        auto begin() { return params.begin(); }
        auto end()   { return params.end(); }
        auto begin() const { return params.begin(); }
        auto end()   const { return params.end(); }
    };

    // ============================================================================
    // EXPRESSIONS
    // ============================================================================

    struct LiteralInt : public Expression {
        std::string value;
        explicit LiteralInt(SourceLocation l, std::string v)
            : Expression(NodeKind::LITERAL_INT, std::move(l)), value(std::move(v)) {}
    };

    struct LiteralFloat : public Expression {
        std::string value;
        explicit LiteralFloat(SourceLocation l, std::string v)
            : Expression(NodeKind::LITERAL_FLOAT, std::move(l)), value(std::move(v)) {}
    };

    struct LiteralComplex : public Expression {
        std::string value;   // includes 'i'
        explicit LiteralComplex(SourceLocation l, std::string v)
            : Expression(NodeKind::LITERAL_COMPLEX, std::move(l)), value(std::move(v)) {}
    };

    struct LiteralString : public Expression {
        std::string value;
        explicit LiteralString(SourceLocation l, std::string v)
            : Expression(NodeKind::LITERAL_STRING, std::move(l)), value(std::move(v)) {}
    };

    struct LiteralRawString : public Expression {
        std::string value;
        explicit LiteralRawString(SourceLocation l, std::string v)
            : Expression(NodeKind::LITERAL_RAW_STRING, std::move(l)), value(std::move(v)) {}
    };

    struct LiteralBool : public Expression {
        bool value;
        explicit LiteralBool(SourceLocation l, bool v)
            : Expression(NodeKind::LITERAL_BOOL, std::move(l)), value(v) {}
    };

    struct LiteralArray : public Expression {
        std::vector<ExpressionPtr> elements;
        explicit LiteralArray(SourceLocation l, std::vector<ExpressionPtr> elems)
            : Expression(NodeKind::LITERAL_ARRAY, std::move(l)), elements(std::move(elems)) {}
    };

    struct LiteralMap : public Expression {
        std::vector<std::pair<ExpressionPtr, ExpressionPtr>> pairs;
        explicit LiteralMap(SourceLocation l, std::vector<std::pair<ExpressionPtr, ExpressionPtr>> p)
            : Expression(NodeKind::LITERAL_MAP, std::move(l)), pairs(std::move(p)) {}
    };

    struct LiteralTuple : public Expression {
        std::vector<ExpressionPtr> elements;
        explicit LiteralTuple(SourceLocation l, std::vector<ExpressionPtr> elems)
            : Expression(NodeKind::LITERAL_TUPLE, std::move(l)), elements(std::move(elems)) {}
    };

    struct LiteralNullptr : public Expression {
        explicit LiteralNullptr(SourceLocation l)
            : Expression(NodeKind::LITERAL_NULLPTR, std::move(l)) {}
    };

    struct LiteralInterpString : public Expression {
        struct Part {
            bool          is_expr;
            std::string   text;      // if !is_expr
            ExpressionPtr expr;      // if is_expr
        };
        std::vector<Part> parts;
        explicit LiteralInterpString(SourceLocation l, std::vector<Part> p)
            : Expression(NodeKind::LITERAL_INTERP_STRING, std::move(l)), parts(std::move(p)) {}
    };

    struct MemberAccessExpr : public Expression {
        ExpressionPtr object;
        NamePtr       member;
        MemberAccessExpr(SourceLocation l, ExpressionPtr obj, NamePtr mem)
            : Expression(NodeKind::MEMBER_ACCESS_EXPR, std::move(l)), object(std::move(obj)), member(std::move(mem)) {}
    };

    struct CallExpr : public Expression {
        ExpressionPtr              callee;
        std::vector<ExpressionPtr> args;
        bool is_early_return = false;  // for "callee(...)?" syntax
        CallExpr(SourceLocation l, ExpressionPtr c, std::vector<ExpressionPtr> a, bool early_ret = false)
            : Expression(NodeKind::CALL_EXPR, std::move(l)), callee(std::move(c)), args(std::move(a)), is_early_return(early_ret) {}
    };

    struct IndexExpr : public Expression {
        ExpressionPtr object;
        ExpressionPtr index;
        IndexExpr(SourceLocation l, ExpressionPtr obj, ExpressionPtr idx)
            : Expression(NodeKind::INDEX_EXPR, std::move(l)), object(std::move(obj)), index(std::move(idx)) {}
    };

    struct BinaryExpr : public Expression {
        BinaryOp      op;
        ExpressionPtr left;
        ExpressionPtr right;
        BinaryExpr(SourceLocation l, BinaryOp o, ExpressionPtr lexpr, ExpressionPtr rexpr)
            : Expression(NodeKind::BINARY_EXPR, std::move(l)), op(o), left(std::move(lexpr)), right(std::move(rexpr)) {}
    };

    struct UnaryExpr : public Expression {
        UnaryOp       op;
        ExpressionPtr operand;
        UnaryExpr(SourceLocation l, UnaryOp o, ExpressionPtr e)
            : Expression(NodeKind::UNARY_EXPR, std::move(l)), op(o), operand(std::move(e)) {}
    };

    struct TernaryExpr : public Expression {
        ExpressionPtr condition;
        ExpressionPtr then_expr;
        ExpressionPtr else_expr;
        TernaryExpr(SourceLocation l, ExpressionPtr cond, ExpressionPtr then_e, ExpressionPtr else_e)
            : Expression(NodeKind::TERNARY_EXPR, std::move(l)), condition(std::move(cond)), then_expr(std::move(then_e)), else_expr(std::move(else_e)) {}
    };

    struct NewExpr : public Expression {
        TypePtr                    alloc_type;
        std::vector<ExpressionPtr> ctor_args;
        NewExpr(SourceLocation l, TypePtr t, std::vector<ExpressionPtr> args)
            : Expression(NodeKind::NEW_EXPR, std::move(l)), alloc_type(std::move(t)), ctor_args(std::move(args)) {}
    };

    struct DeleteStmt : public Construct {
        ExpressionPtr expr;
        DeleteStmt(SourceLocation l, ExpressionPtr e)
            : Construct(NodeKind::DELETE_STMT, std::move(l)), expr(std::move(e)) {}
    };

    struct Lambda : public Expression {
        GenericParameters generic_params;
        Parameters    params;
        TypePtr       return_type;  // nullptr = inferred
        std::variant<ExpressionPtr, BlockPtr> body;   // expression or block
        Lambda(SourceLocation l, GenericParameters gp, Parameters ps,
               TypePtr ret, std::variant<ExpressionPtr, BlockPtr> b)
            : Expression(NodeKind::LAMBDA_EXPR, std::move(l)), generic_params(std::move(gp)),
              params(std::move(ps)), return_type(std::move(ret)), body(std::move(b)) {}
    };

    // ============================================================================
    // STATEMENTS (all derive from Construct)
    // ============================================================================

    struct ExpressionStmt : public Construct {
        ExpressionPtr expr;
        
        ExpressionStmt(SourceLocation l, ExpressionPtr e)
            : Construct(NodeKind::EXPRESSION_STMT, std::move(l)), expr(std::move(e)) {}
    };

    struct IfStmt : public Construct {
        struct Branch {
            ExpressionPtr condition;
            BlockPtr      body;
        };
        Branch              if_branch;
        std::vector<Branch> elif_branches;
        BlockPtr            else_body;   // nullptr if absent
        IfStmt(SourceLocation l, Branch if_b, std::vector<Branch> elif_b = {}, BlockPtr else_b = nullptr)
            : Construct(NodeKind::IF_STMT, std::move(l)), if_branch(std::move(if_b)), elif_branches(std::move(elif_b)), else_body(std::move(else_b)) {}
    };

    struct IfLetStmt : public Construct {
        std::string    binding;
        ExpressionPtr  value;
        BlockPtr       body;
        BlockPtr       else_body;
        IfLetStmt(SourceLocation l, std::string b, ExpressionPtr val, BlockPtr body_arg, BlockPtr else_b = nullptr)
            : Construct(NodeKind::IF_LET_STMT, std::move(l)), binding(std::move(b)), value(std::move(val)), body(std::move(body_arg)), else_body(std::move(else_b)) {}
    };

    struct WhileStmt : public Construct {
        ExpressionPtr condition;
        BlockPtr      body;
        WhileStmt(SourceLocation l, ExpressionPtr cond, BlockPtr b)
            : Construct(NodeKind::WHILE_STMT, std::move(l)), condition(std::move(cond)), body(std::move(b)) {}
    };

    struct DoWhileStmt : public Construct {
        BlockPtr      body;
        ExpressionPtr condition;
        DoWhileStmt(SourceLocation l, BlockPtr b, ExpressionPtr cond)
            : Construct(NodeKind::DO_WHILE_STMT, std::move(l)), body(std::move(b)), condition(std::move(cond)) {}
    };

    struct ForeachStmt : public Construct {
        std::string   iter_name;
        TypePtr       iter_type;    // nullptr = inferred
        ExpressionPtr iterable;
        BlockPtr      body;
        ForeachStmt(SourceLocation l, std::string name, TypePtr type, ExpressionPtr iter, BlockPtr b)
            : Construct(NodeKind::FOREACH_STMT, std::move(l)), iter_name(std::move(name)), iter_type(std::move(type)), iterable(std::move(iter)), body(std::move(b)) {}
    };

    struct MatchStmt : public Construct {
        struct Arm {
            ExpressionPtr pattern;   // nullptr = wildcard
            BlockPtr  body;      // usually a block, but can be a single stmt?
        };
        ExpressionPtr    subject;
        std::vector<Arm> arms;
        MatchStmt(SourceLocation l, ExpressionPtr subj, std::vector<Arm> a)
            : Construct(NodeKind::MATCH_STMT, std::move(l)), subject(std::move(subj)), arms(std::move(a)) {}
    };

    struct ReturnStmt : public Construct {
        ExpressionPtr value;   // nullptr = bare return
        explicit ReturnStmt(SourceLocation l, ExpressionPtr v = nullptr)
            : Construct(NodeKind::RETURN_STMT, std::move(l)), value(std::move(v)) {}
    };

    struct BreakStmt : public Construct {
        explicit BreakStmt(SourceLocation l)
            : Construct(NodeKind::BREAK_STMT, std::move(l)) {}
    };

    struct ContinueStmt : public Construct {
        explicit ContinueStmt(SourceLocation l)
            : Construct(NodeKind::CONTINUE_STMT, std::move(l)) {}
    };

    struct ThrowStmt : public Construct {
        ExpressionPtr exception;
        explicit ThrowStmt(SourceLocation l, ExpressionPtr e)
            : Construct(NodeKind::THROW_STMT, std::move(l)), exception(std::move(e)) {}
    };

    struct TryCatchStmt : public Construct {
        struct CatchClause {
            std::string binding;
            TypePtr     exception_type;
            BlockPtr    body;
        };
        BlockPtr                 try_body;
        std::vector<CatchClause> catches;
        BlockPtr                 finally_body;   // nullptr = no finally
        TryCatchStmt(SourceLocation l, BlockPtr try_b, std::vector<CatchClause> c, BlockPtr finally_b = nullptr)
            : Construct(NodeKind::TRY_CATCH_STMT, std::move(l)), try_body(std::move(try_b)), catches(std::move(c)), finally_body(std::move(finally_b)) {}
    };

    // ============================================================================
    // DECLARATIONS (derive from Declaration)
    // ============================================================================

    struct VariableDecl : public Declaration {
        std::string   name;
        TypePtr       type;          // nullptr = inferred
        ExpressionPtr initialiser;
        bool          is_mutable = false;
        bool          is_static = false;
        bool         is_public = false;
        Directives    dirs;
        VariableDecl(SourceLocation l, std::string n, TypePtr t, ExpressionPtr i, bool mut = false, bool stat=false, Directives d = {})
            : Declaration(NodeKind::VARIABLE_DECL, std::move(l)), name(std::move(n)), type(std::move(t)), initialiser(std::move(i)), is_mutable(mut), is_static(stat), dirs(std::move(d)) {}
    };

    struct FunctionDecl : public Declaration {
        std::string   name;
        GenericParameters generic_params;
        Parameters    params;
        TypePtr       return_type;   // nullptr = void/inferred
        BlockPtr      body;          // nullptr = prototype
        bool          is_static = false;
        bool          is_mut    = false;
        Directives    dirs;
        FunctionDecl(SourceLocation l, std::string n, GenericParameters gp, Parameters ps,
                     TypePtr ret, BlockPtr b, bool stat = false, bool mut = false, Directives d = {})
            : Declaration(NodeKind::FUNCTION_DECL, std::move(l)), name(std::move(n)), generic_params(std::move(gp)),
              params(std::move(ps)), return_type(std::move(ret)), body(std::move(b)), is_static(stat), is_mut(mut), dirs(std::move(d)) {}
    };

    struct ConstructorDecl : public Declaration {
        GenericParameters generic_params;
        Parameters    params;
        BlockPtr      body;
        Directives    dirs;
        SourceLocation location;  // for error reporting
        bool is_public = false;
        ConstructorDecl(SourceLocation l, GenericParameters gp, Parameters ps, BlockPtr b, Directives d = {}, bool pub_ = false)
            : Declaration(NodeKind::CONSTRUCTOR_DECL, std::move(l)), generic_params(std::move(gp)),
              params(std::move(ps)), body(std::move(b)), dirs(std::move(d)), location(std::move(l)), is_public(pub_) {}
    };

    struct DestructorDecl : public Declaration {
        BlockPtr      body;
        Directives    dirs;
        SourceLocation location;  // for error reporting
        DestructorDecl(SourceLocation l, BlockPtr b, Directives d = {})
            : Declaration(NodeKind::DESTRUCTOR_DECL, std::move(l)), body(std::move(b)), dirs(std::move(d)), location(std::move(l)) {}
    };

    struct Method {
        std::string name;  // "new", "~", "operator+", or normal name
        GenericParameters generic_params;
        Parameters params;
        TypePtr return_type;
        BlockPtr body;
        bool is_static = false;
        bool is_mut = false;  // for &mut self methods
        bool is_public = false;
        SourceLocation location;  // for error reporting
        Directives dirs;
        Method(SourceLocation l, std::string n, GenericParameters gp, Parameters ps,
               TypePtr ret, BlockPtr b, bool stat = false, bool mut = false, bool pub_ = false, Directives d = {})
            : name(std::move(n)), generic_params(std::move(gp)), params(std::move(ps)), return_type(std::move(ret)),
              body(std::move(b)), is_static(stat), is_mut(mut), is_public(pub_), location(std::move(l)), dirs(std::move(d)) {}
    };

    struct Operator {
        OverloadableOp op;
        Parameters     params;
        TypePtr        return_type;
        BlockPtr       body;
        bool           is_mut = false;
        bool           is_public = false;
        SourceLocation location;  // for error reporting
        Directives dirs;
        Operator(SourceLocation l, OverloadableOp o, Parameters ps, TypePtr ret, BlockPtr b,
                 bool mut = false, bool pub = false, Directives d = {})
            : op(o), params(std::move(ps)), return_type(std::move(ret)), body(std::move(b)),
              is_mut(mut), is_public(pub), location(std::move(l)), dirs(std::move(d)) {}
    };

    struct ClassDecl : public Declaration {
        std::string name;
        GenericParameters generic_params;

        std::vector<Ptr<ConstructorDecl>> constructors;     // multiple constructors allowed
        std::optional<Ptr<DestructorDecl>> destructor;      // at most one destructor
        
        struct Field {
            std::string name;
            TypePtr type;
            ExpressionPtr default_value = nullptr;
            bool is_public = false;
            bool is_static = false;
            bool is_mut = true;  // fields are mutable by default
            Field(std::string n, TypePtr t, ExpressionPtr init = nullptr,
                  bool pub = false, bool stat = false, bool mut = true)
                : name(std::move(n)), type(std::move(t)), default_value(std::move(init)),
                  is_public(pub), is_static(stat), is_mut(mut) {}
        };
            
        std::vector<Ptr<Field>> fields;
        
        // Methods live directly in the class
        std::vector<Ptr<Method>> methods;

        // Operators also live directly in the class
        std::vector<Ptr<Operator>> operators;
        
        Directives dirs;

        ClassDecl(SourceLocation l, std::string n, GenericParameters gp = {},
                  std::vector<Ptr<ConstructorDecl>> ctors = {},
                  std::optional<Ptr<DestructorDecl>> dtor = std::nullopt,
                  std::vector<Ptr<Field>> flds = {}, std::vector<Ptr<Method>> meths = {},
                  std::vector<Ptr<Operator>> ops = {}, Directives d = {})
            : Declaration(NodeKind::CLASS_DECL, std::move(l)), name(std::move(n)), generic_params(std::move(gp)),
              constructors(std::move(ctors)), destructor(std::move(dtor)),
              fields(std::move(flds)), methods(std::move(meths)), operators(std::move(ops)), dirs(std::move(d)) {}
    };

    // "impl MyType { ... }" (inherent impl, no trait)
    // "impl<T: MyTrait> for MyType<T> { ... } " 

    struct ImplDecl : public Declaration {
        NamePtr         trait_name;                     // trait name for trait impls (nullopt for inherent impls)   
        NamePtr         target_type;                    // the type being implemented
        GenericParameters generic_params;               // e.g. <T> in impl<T> ...

        std::vector<Ptr<Method>> methods;                  // methods and associated functions
        std::vector<Ptr<Operator>> operators;              // operator overloads

        Directives dirs;

        ImplDecl(SourceLocation l, NamePtr trait, NamePtr target,
                GenericParameters gp = {},
                std::vector<Ptr<Method>> meths = {}, std::vector<Ptr<Operator>> ops = {},
                Directives d = {})
            : Declaration(NodeKind::IMPL_DECL, std::move(l)), 
            trait_name(std::move(trait)), target_type(std::move(target)), generic_params(std::move(gp)),
            methods(std::move(meths)), operators(std::move(ops)),
            dirs(std::move(d)) {}
    };


    struct TraitDecl : public Declaration {
        struct MethodReq {
            std::string name;
            GenericParameters generic_params;
            std::vector<TypePtr> param_types;
            TypePtr return_type;
            bool is_mut = false;
        };
        struct OperatorReq {
            OverloadableOp op;
            std::vector<TypePtr> param_types;
            TypePtr return_type;
        };
        std::string name;
        GenericParameters generic_params;
        std::vector<MethodReq> method_reqs;
        std::vector<OperatorReq> operator_reqs;
        Directives dirs;
        TraitDecl(SourceLocation l, std::string n, GenericParameters gp = {}, std::vector<MethodReq> methods = {}, std::vector<OperatorReq> ops = {}, Directives d = {})
            : Declaration(NodeKind::TRAIT_DECL, std::move(l)), name(std::move(n)), generic_params(std::move(gp)), method_reqs(std::move(methods)), operator_reqs(std::move(ops)), dirs(std::move(d)) {}
    };

    struct TypeAliasDecl : public Declaration {
        std::string   alias_name;
        GenericParameters generic_params;
        TypePtr       target_type;
        Directives    dirs;
        TypeAliasDecl(SourceLocation l, std::string name, GenericParameters gp, TypePtr target, Directives d = {})
            : Declaration(NodeKind::TYPE_ALIAS_DECL, std::move(l)), alias_name(std::move(name)), generic_params(std::move(gp)), target_type(std::move(target)), dirs(std::move(d)) {}
    };

    struct EnumDecl : public Declaration {
        struct Variant {
            std::string name;
        };
        std::string          name;
        std::vector<Variant> variants;
        Directives           dirs;
        EnumDecl(SourceLocation l, std::string n, std::vector<Variant> vs, Directives d = {})
            : Declaration(NodeKind::ENUM_DECL, std::move(l)), name(std::move(n)), variants(std::move(vs)), dirs(std::move(d)) {}
    };


    // ============================================================================
    // MODULE SYSTEM
    // ============================================================================

    struct ImportDecl {
        SourceLocation location;
        std::string                module_path;
        std::optional<std::string> module_alias;
        explicit ImportDecl(SourceLocation l, std::string path, std::optional<std::string> alias = std::nullopt)
            : location(std::move(l)), module_path(std::move(path)), module_alias(std::move(alias)) {}
    };

    struct ExportDecl {
        SourceLocation location;
        std::vector<NamePtr> symbols;
        ExportDecl(SourceLocation l, std::vector<NamePtr> syms)
            : location(std::move(l)), symbols(std::move(syms)) {}
    };

} // namespace xenon