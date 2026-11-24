#pragma once

#include "name.h"
#include "nodes.h"

namespace xenon::ast {

    // One type expression node for all named types
    struct NamedTypeExpr : public TypeExpression {
        static constexpr NodeKind node_kind = NodeKind::NAMED_TYPE;
 
        NamePtr name;
 
        NamedTypeExpr(source::Span span, NamePtr n)
            : TypeExpression(node_kind, span), name(std::move(n)) {}
    };
 
    // Pointer type (*T or *mut T)
    struct PointerTypeExpr : public TypeExpression {
        static constexpr NodeKind node_kind = NodeKind::POINTER_TYPE;
 
        TypeExprPtr element_type;
        bool is_mut;
 
        PointerTypeExpr(source::Span span, TypeExprPtr elem, bool mut = false)
            : TypeExpression(node_kind, span), element_type(std::move(elem)), is_mut(mut) {}
    };
 
    // Reference type (&T or &mut T)
    struct ReferenceTypeExpr : public TypeExpression {
        static constexpr NodeKind node_kind = NodeKind::REFERENCE_TYPE;
 
        TypeExprPtr element_type;
        bool is_mut;
 
        ReferenceTypeExpr(source::Span span, TypeExprPtr elem, bool mut = false)
            : TypeExpression(node_kind, span), element_type(std::move(elem)), is_mut(mut) {}
    };
 
    // Static array type ([N]T)
    struct ArrayTypeExpr : public TypeExpression {
        static constexpr NodeKind node_kind = NodeKind::ARRAY_TYPE;
 
        TypeExprPtr element_type;
        ExpressionPtr size_expr;
 
        ArrayTypeExpr(source::Span span, TypeExprPtr elem, ExpressionPtr size = nullptr)
            : TypeExpression(node_kind, span), element_type(std::move(elem)),
              size_expr(std::move(size)) {}
    };

} // namespace xenon::ast
