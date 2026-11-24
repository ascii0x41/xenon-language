#pragma once

#include "nodes.h"

namespace xenon::ast {

        enum class UnaryOperatorKind {
        PLUS,               // +x
        NEGATE,             // -x
        BITWISE_NOT,        // ~x
        LOGICAL_NOT,        // !x
        BORROW,             // &x
        MUT_BORROW,         // &mut x
        RAW_BORROW,         // &raw x
        MUT_RAW_BORROW,     // &raw mut x
        DEREFERENCE,        // *x
    };
 
    enum class BinaryOperatorKind {
        // Arithmetic
        ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO,
 
        // Comparison
        EQUAL, NOT_EQUAL, LESS_THAN, LESS_EQUAL, GREATER_THAN, GREATER_EQUAL,
 
        // Logical
        LOGICAL_AND, LOGICAL_OR,
 
        // Bitwise
        BITWISE_AND, BITWISE_OR, BITWISE_XOR, SHIFT_LEFT, SHIFT_RIGHT,
    };
 
    enum class AssignmentOperatorKind {
        ASSIGN,               // =
        ADD_ASSIGN,           // +=
        SUBTRACT_ASSIGN,      // -=
        MULTIPLY_ASSIGN,      // *=
        DIVIDE_ASSIGN,        // /=
        MODULO_ASSIGN,        // %=
        BITWISE_AND_ASSIGN,   // &=
        BITWISE_OR_ASSIGN,    // |=
        BITWISE_XOR_ASSIGN,   // ^=
        SHIFT_LEFT_ASSIGN,    // <<=
        SHIFT_RIGHT_ASSIGN,   // >>=
    };
 
    struct UnaryOpExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::UNARY_OP_EXPR;
 
        UnaryOperatorKind op;
        ExpressionPtr operand;
 
        UnaryOpExpr(source::Span span, UnaryOperatorKind o, ExpressionPtr operand_)
            : Expression(node_kind, span)
            , op(o), operand(std::move(operand_)) {}
    };
 
    struct BinaryOpExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::BINARY_OP_EXPR;
 
        BinaryOperatorKind op;
        ExpressionPtr lhs;
        ExpressionPtr rhs;
 
        BinaryOpExpr(source::Span span, BinaryOperatorKind o, ExpressionPtr left, ExpressionPtr right)
            : Expression(node_kind, span)
            , op(o), lhs(std::move(left)), rhs(std::move(right)) {}
    };
 
    struct AssignmentExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::ASSIGNMENT_EXPR;
 
        AssignmentOperatorKind op;
        ExpressionPtr lhs;   // Must be assignable (variable, index, deref, etc.)
        ExpressionPtr rhs;
 
        AssignmentExpr(source::Span span, AssignmentOperatorKind o, ExpressionPtr left, ExpressionPtr right)
            : Expression(node_kind, span)
            , op(o), lhs(std::move(left)), rhs(std::move(right)) {}
    };
 
    struct TernaryExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::TERNARY_EXPR;
 
        ExpressionPtr condition;
        ExpressionPtr then_branch;
        ExpressionPtr else_branch;
 
        TernaryExpr(source::Span span, ExpressionPtr cond, ExpressionPtr then_b, ExpressionPtr else_b)
            : Expression(node_kind, span), condition(std::move(cond)),
              then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
    };


} // namespace xenon::ast
