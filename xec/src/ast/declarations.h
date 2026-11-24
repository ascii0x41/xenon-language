#pragma once

#include <vector>
#include <optional>

#include "nodes.h"
#include "name.h"
#include "statements.h"

namespace xenon::ast {

        // An attribute: `#[name]`, `#[name(a, b)]`. Attributes are parsed generically -
    // no attribute is known to the parser; later phases interpret `name` and `args`.
    struct Attribute {
        source::Span                source;
        NamePtr                     name;
        std::vector<ExpressionPtr>  args;
    };
 
    // variable, function declarations, etc.
    struct Declaration : public Statement {
        std::string            doc;         // merged `///` lines, "" if undocumented
        std::vector<Attribute> attributes;  // every `#[...]` written before the declaration
 
        explicit Declaration(NodeKind k, source::Span span)
            : Statement(k, span) {}
    };
 
    using DeclarationPtr = std::unique_ptr<Declaration>;

    struct ModuleDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::MODULE_DECL;

        NamePtr name;

        ModuleDecl(source::Span span, NamePtr n)
            : Declaration(node_kind, span), name(std::move(n)) {}
    };

    struct ImportDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::IMPORT_DECL;

        NamePtr name;

        ImportDecl(source::Span span, NamePtr n)
            : Declaration(node_kind, span), name(std::move(n)) {}
    };
 
    struct VariableDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::VARIABLE_DECL;
 
        std::string name;
        TypeExprPtr type_expr;  // can be nullptr
        ExpressionPtr init_expr;  // can be nullptr
        bool is_mut;
        bool is_public;
 
        VariableDecl(source::Span span, std::string n, TypeExprPtr t = nullptr, ExpressionPtr i = nullptr, bool m = false, bool p = false)
            : Declaration(node_kind, span), name(std::move(n)), type_expr(std::move(t)), init_expr(std::move(i)), is_mut(m), is_public(p) {}
    };
 
    using VariableDeclPtr = std::unique_ptr<VariableDecl>;
 
    struct FunctionDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::FUNCTION_DECL;
 
        std::string name;
        std::vector<VariableDeclPtr> parameters;
        TypeExprPtr return_type;  // never nullptr from the parser (defaults to `void`)
        BlockPtr body;          // nullptr for a declaration without a body (`func f();`, e.g. extern)
        bool is_public;
 
        FunctionDecl(source::Span span, std::string n, std::vector<VariableDeclPtr> params, TypeExprPtr ret_type = nullptr, BlockPtr b = nullptr, bool p = false)
            : Declaration(node_kind, span), name(std::move(n)), parameters(std::move(params)), return_type(std::move(ret_type)), body(std::move(b)), is_public(p) {}
    };
 
    using FunctionDeclPtr = std::unique_ptr<FunctionDecl>;
 
 
    // A field of a `type` declaration:  [pub] name: T;
    struct StructFieldDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::STRUCT_FIELD_DECL;
 
        std::string name;
        TypeExprPtr type_expr;
        bool is_public;
 
        StructFieldDecl(source::Span span, std::string n,
                        TypeExprPtr t = nullptr,
                        bool p = false)
            : Declaration(node_kind, span),
              name(std::move(n)), type_expr(std::move(t)), is_public(p) {}
    };
 
    using StructFieldDeclPtr = std::unique_ptr<StructFieldDecl>;
 
 
    enum class SelfReceiverType {
        VALUE,
        REFERENCE,
    };
 
    struct SelfReceiver {
        SelfReceiverType type;
        bool is_mut;
    };
 
    // A function inside an `impl` block: a method (has a receiver) or a
    // `static func` (is_static, no receiver). Having neither, or both, is an error
    // the parser or sema reports; the node just records what was written.
    struct MethodDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::METHOD_DECL;
 
        std::string name;
        std::optional<SelfReceiver> receiver;
        std::vector<VariableDeclPtr> parameters;
        TypeExprPtr return_type;
        BlockPtr body;
        bool is_public;
        bool is_static;
 
        MethodDecl(source::Span span, std::string n,
                   std::optional<SelfReceiver> r = std::nullopt,
                   std::vector<VariableDeclPtr> params = {},
                   TypeExprPtr ret_type = nullptr,
                   BlockPtr b = nullptr,
                   bool p = false,
                   bool s = false)
            : Declaration(node_kind, span),
              name(std::move(n)), receiver(std::move(r)), parameters(std::move(params)),
              return_type(std::move(ret_type)), body(std::move(b)),
              is_public(p), is_static(s) {}
    };
 
    using MethodDeclPtr = std::unique_ptr<MethodDecl>;
 
    // type Name { fields }
    // (`type Name = T;` aliases will get their own node later.)
    struct StructDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::STRUCT_DECL;
 
        std::string name;
        std::vector<StructFieldDeclPtr> fields;
        bool is_public;
 
        StructDecl(source::Span span, std::string n,
                   std::vector<StructFieldDeclPtr> f,
                   bool p = false)
            : Declaration(node_kind, span),
              name(std::move(n)), fields(std::move(f)), is_public(p) {}
    };
 
    using StructDeclPtr = std::unique_ptr<StructDecl>;
 
    // impl Name { static lets, methods }
    struct ImplDecl : public Declaration {
        static constexpr NodeKind node_kind = NodeKind::IMPL_DECL;
 
        NamePtr type_name;
        std::vector<VariableDeclPtr> static_vars;
        std::vector<MethodDeclPtr> methods;
 
        ImplDecl(source::Span span, NamePtr n,
                 std::vector<VariableDeclPtr> sv = {},
                 std::vector<MethodDeclPtr> m = {})
            : Declaration(node_kind, span),
              type_name(std::move(n)), static_vars(std::move(sv)),
              methods(std::move(m)) {}
    };
 
    using ImplDeclPtr = std::unique_ptr<ImplDecl>;

}
