#pragma once

#include <vector>

#include "suffix.h"
#include "nodes.h"

namespace xenon::ast {

    struct LiteralInt : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_INT;
 
        std::string   lexeme;   // digits as written, suffix excluded: "42", "0xFF", "0b1010", "0o17"
        NumericSuffix suffix;   // NONE => unconstrained {integer}: sema infers from context, else i32
 
        LiteralInt(source::Span span, std::string lex, NumericSuffix suf = NumericSuffix::NONE)
            : Expression(node_kind, span), lexeme(std::move(lex)), suffix(suf) {}
    };
 
    struct LiteralFloat : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_FLOAT;
 
        std::string   lexeme;   // "3.14", "1.0e9" (suffix excluded)
        NumericSuffix suffix;   // NONE => unconstrained {float}: sema infers from context, else f32
 
        LiteralFloat(source::Span span, std::string lex, NumericSuffix suf = NumericSuffix::NONE)
            : Expression(node_kind, span), lexeme(std::move(lex)), suffix(suf) {}
    };
 
    struct LiteralString : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_STRING;
 
        std::string value;      // UTF-8, escapes already decoded (raw strings arrive unwrapped)
 
        explicit LiteralString(source::Span span, std::string v)
            : Expression(node_kind, span), value(std::move(v)) {}
    };
 
    struct LiteralChar : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_CHAR;
 
        std::uint32_t value;    // Unicode scalar value (the spec's `rune`)
 
        explicit LiteralChar(source::Span span, std::uint32_t v)
            : Expression(node_kind, span), value(v) {}
    };
 
    struct LiteralBool : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_BOOL;
 
        bool value;
 
        explicit LiteralBool(source::Span span, bool v)
            : Expression(node_kind, span), value(v) {}
    };
 
    struct LiteralNullptr : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_NULLPTR;
 
        explicit LiteralNullptr(source::Span span)
            : Expression(node_kind, span) {}
    };
 
    // [ 4, 9, 16, 25 ]
    struct LiteralArray : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_ARRAY;
 
        std::vector<ExpressionPtr> elements;
        TypeExprPtr element_type;
        ExpressionPtr size_expr;
 
        explicit LiteralArray(
            source::Span span,
            std::vector<ExpressionPtr> elems,
            TypeExprPtr element_type_ = nullptr,
            ExpressionPtr size_expr_ = nullptr
        )
            : Expression(node_kind, span),
              elements(std::move(elems)), element_type(std::move(element_type_)),
              size_expr(std::move(size_expr_)) {}
    };

    struct FieldInitializer {
        source::Span span;
        std::string name;
        ExpressionPtr value;
    };

    struct LiteralStruct : public Expression {
        static constexpr NodeKind node_kind = NodeKind::LITERAL_STRUCT;

        TypeExprPtr type_expr;
        std::vector<FieldInitializer> fields;
        bool is_named;

        LiteralStruct(
            source::Span span,
            TypeExprPtr type,
            std::vector<FieldInitializer> fields_,
            bool is_named_
        )
            : Expression(node_kind, span), type_expr(std::move(type)),
              fields(std::move(fields_)), is_named(is_named_) {}
    };

} // namespace xenon::ast
