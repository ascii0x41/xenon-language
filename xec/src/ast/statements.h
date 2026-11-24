#pragma once

#include <vector>

#include "nodes.h"

namespace xenon::ast {

    struct BlockStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::BLOCK_STMT;
 
        std::vector<StatementPtr> statements;
 
        explicit BlockStmt(source::Span span, std::vector<StatementPtr> stmts)
            : Statement(node_kind, span), statements(std::move(stmts)) {}
    };
 
    using BlockPtr = std::unique_ptr<BlockStmt>;
 
    struct ExpressionStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::EXPRESSION_STMT;
 
        ExpressionPtr expr;
 
        explicit ExpressionStmt(source::Span span, ExpressionPtr e)
            : Statement(node_kind, span), expr(std::move(e)) {}
    };
 
 
    // -- Control Flow ---------------------------------------------------------------
 
    struct IfStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::IF_STMT;
 
        ExpressionPtr condition;
        StatementPtr  then_branch;
        StatementPtr  else_branch;  // can be nullptr
 
        IfStmt(source::Span span, ExpressionPtr cond, StatementPtr then_b, StatementPtr else_b = nullptr)
            : Statement(node_kind, span), condition(std::move(cond)),
              then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
    };
 
    struct WhileStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::WHILE_STMT;
 
        ExpressionPtr condition;
        StatementPtr  body;
 
        WhileStmt(source::Span span, ExpressionPtr cond, StatementPtr b)
            : Statement(node_kind, span), condition(std::move(cond)), body(std::move(b)) {}
    };
 
    // -- Jump Statements ---------------------------------------------------------------
 
    struct ReturnStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::RETURN_STMT;
 
        ExpressionPtr return_value;  // can be nullptr
 
        explicit ReturnStmt(source::Span span, ExpressionPtr val = nullptr)
            : Statement(node_kind, span), return_value(std::move(val)) {}
    };
 
    struct BreakStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::BREAK_STMT;
 
        explicit BreakStmt(source::Span span)
            : Statement(node_kind, span) {}
    };
 
    struct ContinueStmt : public Statement {
        static constexpr NodeKind node_kind = NodeKind::CONTINUE_STMT;
 
        explicit ContinueStmt(source::Span span)
            : Statement(node_kind, span) {}
    };

} // namespace xenon::ast
