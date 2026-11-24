#pragma once

#include <vector>

#include "nodes.h"
#include "common/source/span.h"

namespace xenon::ast {

    struct Name : public Expression {
        static constexpr NodeKind node_kind = NodeKind::NAME;
 
        std::vector<std::string_view> components;
        bool                          is_global = false;
 
        Name(source::Span span, std::vector<std::string_view> parts, bool global = false)
            : Expression(node_kind, span), components(std::move(parts)), is_global(global) {}
 
        bool is_qualified() const { return components.size() > 1 || is_global; }
 
        std::string to_string() const {
            std::string value = is_global ? "::" : "";
            for (std::size_t i = 0; i < components.size(); ++i) {
                if (i != 0) {
                    value += "::";
                }
                value += components[i];
            }
            return value;
        }
    };
 
    using NamePtr = std::unique_ptr<Name>;

    struct MemberAccessExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::MEMBER_ACCESS_EXPR;

        ExpressionPtr object;
        NamePtr member;

        MemberAccessExpr(source::Span span, ExpressionPtr object_, NamePtr member_)
            : Expression(node_kind, span), object(std::move(object_)),
              member(std::move(member_)) {}
    };

    struct CallExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::CALL_EXPR;

        ExpressionPtr callee;
        std::vector<ExpressionPtr> arguments;

        CallExpr(source::Span span, ExpressionPtr callee_, std::vector<ExpressionPtr> args)
            : Expression(node_kind, span), callee(std::move(callee_)),
              arguments(std::move(args)) {}
    };

    struct IndexExpr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::INDEX_EXPR;

        ExpressionPtr object;
        ExpressionPtr index;

        IndexExpr(source::Span span, ExpressionPtr object_, ExpressionPtr index_)
            : Expression(node_kind, span), object(std::move(object_)),
              index(std::move(index_)) {}
    };

} // namespace xenon::ast
