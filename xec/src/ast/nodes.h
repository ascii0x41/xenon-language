#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "common/source/span.h"

namespace xenon::ast {

    struct NodeID {
        std::uint32_t value_;
        inline static std::atomic_uint32_t count_ = 0;

        [[nodiscard]]
        static constexpr NodeID invalid() noexcept {
            return {std::numeric_limits<std::uint32_t>::max()};
        }

        [[nodiscard]]
        static NodeID next() noexcept {
            return {NodeID::count_++};
        }

        constexpr bool operator==(const NodeID&) const = default;
    };

    enum class NodeKind {
        // Literals
        LITERAL_INT,
        LITERAL_FLOAT,
        LITERAL_STRING,
        LITERAL_CHAR,
        LITERAL_BOOL,
        LITERAL_ARRAY,
        LITERAL_NULLPTR,
        LITERAL_STRUCT,

        // Names / Access
        NAME,
        MEMBER_ACCESS_EXPR,
        CALL_EXPR,
        INDEX_EXPR,

        // Type Expressions
        NAMED_TYPE,
        POINTER_TYPE,
        REFERENCE_TYPE,
        ARRAY_TYPE,

        // Operations
        BINARY_OP_EXPR,
        UNARY_OP_EXPR,
        ASSIGNMENT_EXPR,
        TERNARY_EXPR,

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
        MODULE_DECL,
        IMPORT_DECL,
        VARIABLE_DECL,
        FUNCTION_DECL,
        STRUCT_FIELD_DECL,
        METHOD_DECL,
        STRUCT_DECL,
        IMPL_DECL,
        
        // Special
        EOF_STMT,
    };

    struct ASTNode {
        NodeKind kind_;
        source::Span span_;
        NodeID id_;
        explicit ASTNode(NodeKind kind, source::Span span) : kind_(kind), span_(span) { id_ = NodeID::next(); }
        ASTNode(ASTNode&&) noexcept = default;
        ASTNode& operator=(ASTNode&&) noexcept = default;
        virtual ~ASTNode() = default;
    };

    // Selection, Iteration, and Sequence
    struct Statement : public ASTNode {
        explicit Statement(NodeKind k, source::Span span)
            : ASTNode(k, span) {}
    };
 
    using StatementPtr = std::unique_ptr<Statement>;
 
    // L and R values, expressions, etc.
    struct Expression : public ASTNode {
        explicit Expression(NodeKind k, source::Span span)
            : ASTNode(k, span) {}
    };
 
    using ExpressionPtr = std::unique_ptr<Expression>;
 
    // Base class for all type expressions in the AST
    struct TypeExpression : public ASTNode {
        explicit TypeExpression(NodeKind k, source::Span span)
            : ASTNode(k, span) {}
    };
 
    using TypeExprPtr = std::unique_ptr<TypeExpression>;
 
 
    // -- Checked casts ---------------------------------------------------------------
    //
    // Every concrete node declares `static constexpr NodeKind node_kind`, which is
    // what these compare against. The categories (Expression, Statement, ...) have
    // no node_kind, so is<Expression>() deliberately does not compile.
 
    template<typename T>
    concept ConcreteNode = std::derived_from<T, ASTNode> && requires { T::node_kind; };
 
    template<ConcreteNode T>
    [[nodiscard]]
    bool is(const ASTNode& node) noexcept {
        return node.kind_ == T::node_kind;
    }
 
    // nullptr if `node` is null or of a different kind
    template<ConcreteNode T>
    [[nodiscard]]
    T* as(ASTNode* node) noexcept {
        return node != nullptr && node->kind_ == T::node_kind ? static_cast<T*>(node) : nullptr;
    }
 
    template<ConcreteNode T>
    [[nodiscard]]
    const T* as(const ASTNode* node) noexcept {
        return node != nullptr && node->kind_ == T::node_kind ? static_cast<const T*>(node) : nullptr;
    }
 
    // For when the kind is already known. Asserts in debug builds.
    template<ConcreteNode T>
    [[nodiscard]]
    T& cast(ASTNode& node) noexcept {
        assert(is<T>(node));
        return static_cast<T&>(node);
    }
 
    template<ConcreteNode T>
    [[nodiscard]]
    const T& cast(const ASTNode& node) noexcept {
        assert(is<T>(node));
        return static_cast<const T&>(node);
    }


} // namespace xenon::ast