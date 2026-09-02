#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>

#include "common/dataclasses.h"
#include "common/diagnostics.h"
#include "ast/astlib.h"

namespace xenon::semantic {

    // A symbol is a named entity in the program such as a variable, a function, or a class
   struct Symbol {
        enum class SymbolKind {
            VARIABLE,
            NAMESPACE_VAR,  // `static let` at namespace/class scope -- see NamespaceVarSemantic
            FUNCTION,
            CLASS,
        } kind;
        bool is_public;
        const ast::ASTNode* ast_node;  // The AST node that this symbol corresponds to
        explicit Symbol(SymbolKind k, bool is_pub = false, const ast::ASTNode* ast = nullptr) : kind(k), is_public(is_pub), ast_node(ast) {}
        virtual ~Symbol() = default;
    };


    // Types
    struct TypeInfo;
    using TypeInfoPtr = std::shared_ptr<TypeInfo>;


    /**
     * Fields/methods/operators live here
     * Same instances are shared so `i32 + i32` is simple
     */
    struct ClassDefinition;

    // Singletons -- VOID/NULLTYPE/ERROR have no fields or operators, so there's never a reason for more than one instance of each to exist.
    TypeInfoPtr get_error_type();
    TypeInfoPtr get_void_type();
    TypeInfoPtr get_null_type();


    struct TypeInfo {
        enum class TypeKind {
            PRIMITIVE,      // i8-i64, u8-u64, float, double, complex, size, string, char, bool
            USER_DEFINED,   // a declared `class`
            POINTER,        // (mut) ptr T, (mut) box T
            REFERENCE,      // (mut) ref T
            ARRAY,          // [T; N] or [T]
            NULLTYPE,       // type of nullptr
            VOID,           // void
            ERROR,          // short-circuit sentinel -- see get_error_type()
        } kind;

        explicit TypeInfo(TypeKind k) : kind(k) {}
        virtual ~TypeInfo() = default;

        /**
         * Compares two types for structural equality
         * i32 == i32 -> true
         * i32 != f32 -> false
         */
        virtual bool equals(const TypeInfo& other) const {
            return kind == other.kind;
        }

        /**
         * Looks up what type an operator returns when applied to a value of this type.
         * i32 + i32 -> i32
         * i32 + f64 -> <error>
         */
        virtual TypeInfoPtr get_operator_result_t(ast::OperatorKind op, const std::optional<TypeInfoPtr>& rhs = std::nullopt) const {
            return get_error_type();
        }

        /**
         * Looks up what type a method returns when called on a value of this type.
         * string.length() -> size
         * [T].push(U) -> <error>
         */
        virtual TypeInfoPtr get_method_result_t(const std::string& name, const std::vector<TypeInfoPtr>& args) const {
            return get_error_type();
        }

        /**
         * Human-readable name for diagnostics -- "i32", "ptr mut Point",
         * "[string; 4]". Not meant for mangling or codegen, just for error
         * messages ("expected i32, found string").
         *
         * The base default only covers the three singleton kinds
         * (NULLTYPE/VOID/ERROR) -- every other kind (PRIMITIVE, USER_DEFINED,
         * POINTER, REFERENCE, ARRAY) has real structure to describe and
         * overrides this itself.
         */
        virtual std::string get_type_name() const {
            switch (kind) {
                case TypeKind::NULLTYPE: return "nulltype";
                case TypeKind::VOID:     return "void";
                case TypeKind::ERROR:    return "<error>";
                default:                 return "<unknown type>";  // unreachable: every other kind overrides this
            }
        }

    };


    // -- Named types (primitives + user classes, unified) -------------------------

    struct NamedTypeInfo : public TypeInfo {
        std::shared_ptr<ClassDefinition> definition;  // where operators, fields, and methods live

        NamedTypeInfo(TypeKind k, std::shared_ptr<ClassDefinition> def)
            : TypeInfo(k), definition(std::move(def)) {}

        bool equals(const TypeInfo& other) const override {
            if (other.kind != kind) return false;
            // Same ClassDefinition => same type. Two different `class Point`
            // declarations (e.g. from different modules) get different
            // ClassDefinitions even if the name matches, which is what you want.
            return definition.get() == static_cast<const NamedTypeInfo&>(other).definition.get();
        }

        // Both defined out-of-line below, once ClassDefinition actually exists.
        TypeInfoPtr get_operator_result_t(ast::OperatorKind op, const std::optional<TypeInfoPtr>& rhs = std::nullopt) const override;
        TypeInfoPtr get_method_result_t(const std::string& name, const std::vector<TypeInfoPtr>& args) const override;

        // One implementation covers both PRIMITIVE and USER_DEFINED -- same
        // reasoning as get_size_of_type/get_align_of_type further down: both
        // are just "a ClassDefinition", whether it's ours (i8, string) or the
        // user's (Point). definition->name is what get_type_name reads.
        std::string get_type_name() const override;
    };

    struct PrimitiveTypeInfo : public NamedTypeInfo {
        enum class PrimitiveKind {
            I8, I16, I32, I64,
            U8, U16, U32, U64, SIZE,
            FLOAT,
            DOUBLE,
            CPLX128,
            STRING,
            CHAR,
            BOOL,
        } primitive_kind;

        PrimitiveTypeInfo(PrimitiveKind pk, std::shared_ptr<ClassDefinition> def)
            : NamedTypeInfo(TypeKind::PRIMITIVE, std::move(def)), primitive_kind(pk) {}
    };

    struct UserDefinedTypeInfo : public NamedTypeInfo {
        explicit UserDefinedTypeInfo(std::shared_ptr<ClassDefinition> def)
            : NamedTypeInfo(TypeKind::USER_DEFINED, std::move(def)) {}
    };

    // -- Numeric classification -------------------------------------------------

    inline bool is_signed_int(const TypeInfo& t) {
        if (t.kind != TypeInfo::TypeKind::PRIMITIVE) return false;
        switch (static_cast<const PrimitiveTypeInfo&>(t).primitive_kind) {
            case PrimitiveTypeInfo::PrimitiveKind::I8:
            case PrimitiveTypeInfo::PrimitiveKind::I16:
            case PrimitiveTypeInfo::PrimitiveKind::I32:
            case PrimitiveTypeInfo::PrimitiveKind::I64:
                return true;
            default:
                return false;
        }
    }

    inline bool is_unsigned_int(const TypeInfo& t) {
        if (t.kind != TypeInfo::TypeKind::PRIMITIVE) return false;
        switch (static_cast<const PrimitiveTypeInfo&>(t).primitive_kind) {
            case PrimitiveTypeInfo::PrimitiveKind::U8:
            case PrimitiveTypeInfo::PrimitiveKind::U16:
            case PrimitiveTypeInfo::PrimitiveKind::U32:
            case PrimitiveTypeInfo::PrimitiveKind::U64:
            case PrimitiveTypeInfo::PrimitiveKind::SIZE:
                return true;
            default:
                return false;
        }
    }

    inline bool is_integer_type(const TypeInfo& t) {
        return is_signed_int(t) || is_unsigned_int(t);
    }

    inline bool is_floating_point(const TypeInfo& t) {
        if (t.kind != TypeInfo::TypeKind::PRIMITIVE) return false;
        switch (static_cast<const PrimitiveTypeInfo&>(t).primitive_kind) {
            case PrimitiveTypeInfo::PrimitiveKind::FLOAT:
            case PrimitiveTypeInfo::PrimitiveKind::DOUBLE:
                return true;
            default:
                return false;
        }
    }

    // 0 for kinds where "widening by bit width" doesn't apply (STRING, CHAR, BOOL)
    inline unsigned bit_width(const PrimitiveTypeInfo& t) {
        switch (t.primitive_kind) {
            case PrimitiveTypeInfo::PrimitiveKind::I8:
            case PrimitiveTypeInfo::PrimitiveKind::U8:
                return 8;
            case PrimitiveTypeInfo::PrimitiveKind::I16:
            case PrimitiveTypeInfo::PrimitiveKind::U16:
                return 16;
            case PrimitiveTypeInfo::PrimitiveKind::I32:
            case PrimitiveTypeInfo::PrimitiveKind::U32:
            case PrimitiveTypeInfo::PrimitiveKind::FLOAT:
                return 32;
            case PrimitiveTypeInfo::PrimitiveKind::I64:
            case PrimitiveTypeInfo::PrimitiveKind::U64:
            case PrimitiveTypeInfo::PrimitiveKind::SIZE:
            case PrimitiveTypeInfo::PrimitiveKind::DOUBLE:
                return 64;
            case PrimitiveTypeInfo::PrimitiveKind::CPLX128:
                return 128;
            default:
                return 0;
        }
    }

    // -- Implicit widening --------------------------------------------------------
    //
    // Whether a value of type `from` can be used where `to` is expected without
    // an explicit cast. NOT the same question as `from->equals(*to)` (which
    // asks "are these the same type") -- this asks "is a same-or-safer type".
    //
    // Currently disallows: crossing signedness (u8 -> i16), int -> float
    // promotion (i32 -> double), and real -> complex (double -> cplx128).
    // That's a real language design decision, not an oversight -- if you want
    // any of those to be implicit, add a rule for it; if you want them to stay
    // explicit-cast-only (Rust's stance, roughly), this is already correct.
    inline bool can_implicitly_widen(const TypeInfo& from, const TypeInfo& to) {
        // ERROR is a short-circuit sentinel: once one type error has already
        // been reported upstream, treat everything touching it as fine so the
        // checker doesn't cascade into a wall of secondary diagnostics for the
        // same root cause. This has to be the first check.
        if (from.kind == TypeInfo::TypeKind::ERROR || to.kind == TypeInfo::TypeKind::ERROR) {
            return true;
        }

        // nullptr widens to any pointer type -- otherwise `let p: ptr i32 =
        // nullptr;` has no legal type for the literal on the right.
        if (from.kind == TypeInfo::TypeKind::NULLTYPE && to.kind == TypeInfo::TypeKind::POINTER) {
            return true;
        }

        // Exact match, structurally. Not name-based (TypeInfo doesn't have a
        // `name`) -- this is exactly what equals() is for.
        if (from.equals(to)) {
            return true;
        }

        if (is_unsigned_int(from) && is_unsigned_int(to)) {
            return bit_width(static_cast<const PrimitiveTypeInfo&>(from)) <=
                   bit_width(static_cast<const PrimitiveTypeInfo&>(to));
        }
        if (is_signed_int(from) && is_signed_int(to)) {
            return bit_width(static_cast<const PrimitiveTypeInfo&>(from)) <=
                   bit_width(static_cast<const PrimitiveTypeInfo&>(to));
        }
        if (is_floating_point(from) && is_floating_point(to)) {
            return bit_width(static_cast<const PrimitiveTypeInfo&>(from)) <=
                   bit_width(static_cast<const PrimitiveTypeInfo&>(to));
        }

        return false;
    }

    inline bool can_implicitly_widen(const TypeInfoPtr& from, const TypeInfoPtr& to) {
        return can_implicitly_widen(*from, *to);
    }


    // -- Structural (unnamed) types -------------------------------------------------

    struct PointerTypeInfo : public TypeInfo {
        TypeInfoPtr element_type;
        bool is_mut;  // mut ptr T or ptr T
        bool is_box;  // box T or ptr T
        PointerTypeInfo(TypeInfoPtr elem, bool mut = false, bool boxed = false)
            : TypeInfo(TypeKind::POINTER), element_type(std::move(elem)), is_mut(mut), is_box(boxed) {}

        bool equals(const TypeInfo& other) const override {
            if (other.kind != TypeKind::POINTER) return false;
            const auto& o = static_cast<const PointerTypeInfo&>(other);
            return is_mut == o.is_mut && is_box == o.is_box && element_type->equals(*o.element_type);
        }

        std::string get_type_name() const override {
            return (is_mut ? "mut " : "") + std::string(is_box ? "box " : "ptr ") + element_type->get_type_name();
        }
    };

    struct ReferenceTypeInfo : public TypeInfo {
        TypeInfoPtr element_type;
        bool is_mut;  // mut ref T or ref T
        ReferenceTypeInfo(TypeInfoPtr elem, bool mut = false)
            : TypeInfo(TypeKind::REFERENCE), element_type(std::move(elem)), is_mut(mut) {}

        bool equals(const TypeInfo& other) const override {
            if (other.kind != TypeKind::REFERENCE) return false;
            const auto& o = static_cast<const ReferenceTypeInfo&>(other);
            return is_mut == o.is_mut && element_type->equals(*o.element_type);
        }

        std::string get_type_name() const override {
            return (is_mut ? "mut ref " : "ref ") + element_type->get_type_name();
        }
    };

    struct ArrayTypeInfo : public TypeInfo {
        TypeInfoPtr element_type;
        std::optional<size_t> size;  // nullopt for dynamic arrays
        ArrayTypeInfo(TypeInfoPtr elem, std::optional<size_t> sz = std::nullopt)
            : TypeInfo(TypeKind::ARRAY), element_type(std::move(elem)), size(sz) {}

        bool equals(const TypeInfo& other) const override {
            if (other.kind != TypeKind::ARRAY) return false;
            const auto& o = static_cast<const ArrayTypeInfo&>(other);
            return size == o.size && element_type->equals(*o.element_type);
        }

        // No ClassDefinition here on purpose: [] and .length()/.push() aren't
        // user-overloadable, and their signatures depend on element_type, which
        // is different for every array instantiation ([i32] vs [string]) -- a
        // single stored ClassDefinition can't express that the way it can for a
        // concrete, non-generic type like i32 or UserClass. Handled directly
        // instead. If arrays grow more builtin operators later, they go here too.
    TypeInfoPtr get_operator_result_t(ast::OperatorKind op, const std::optional<TypeInfoPtr>& rhs = std::nullopt) const override {
        if (op == ast::OperatorKind::INDEX_READ) {
            if (!rhs.has_value()) {
                return get_error_type(); // no index expression
            }
            if (!is_integer_type(*rhs.value())) {
                return get_error_type(); // index must be integer
            }
            return element_type;
        }
        return get_error_type(); // arrays have no unary operators
    }

        TypeInfoPtr get_method_result_t(const std::string& name, const std::vector<TypeInfoPtr>& args) const override {
            if (name == "length" && args.empty()) {
                // Valid on both [T] and [T; N] -- a fixed-size array still
                // has a length (it's just already known at compile time as
                // N), so there's no reason to restrict this one.
                return builtins::get_size_type();
            }
            if (name == "push" && args.size() == 1 && args[0]->equals(*element_type)) {
                // Only [T] can grow. [T; N] has no capacity beyond N and no
                // reallocation story -- this isn't "wrong argument type", the
                // method just doesn't exist on a fixed-size array at all.
                if (size.has_value()) {
                    return get_error_type();
                }
                return get_void_type();
            }
            // further builtins (pop, contains, ...) follow the same shape --
            // whichever of these are dynamic-only get the same size.has_value() guard as push
            return get_error_type();
        }

        std::string get_type_name() const override {
            std::string s = "[" + element_type->get_type_name();
            if (size.has_value()) s += "; " + std::to_string(size.value());
            return s + "]";
        }
    };

    // `[T]` -- dynamic array. `size` is nullopt, which is exactly what
    // ArrayTypeInfo::equals uses to distinguish it from any `[T; N]`.
    inline TypeInfoPtr make_dynamic_array(const TypeInfoPtr& element_type) {
        return std::make_shared<ArrayTypeInfo>(element_type, std::nullopt);
    }

    // `[T; N]` -- static array. N must already be a resolved compile-time
    // constant by the time this is called (see is_constexpr below) -- this
    // factory doesn't re-check that, it just stores the value.
    inline TypeInfoPtr make_static_array(const TypeInfoPtr& element_type, size_t N) {
        return std::make_shared<ArrayTypeInfo>(element_type, N);
    }


    // -- Singleton types ------------------------------------------------------------

    struct NullTypeInfo : public TypeInfo {
        NullTypeInfo() : TypeInfo(TypeKind::NULLTYPE) {}
    };

    struct VoidTypeInfo : public TypeInfo {
        VoidTypeInfo() : TypeInfo(TypeKind::VOID) {}
    };

    struct ErrorTypeInfo : public TypeInfo {
        ErrorTypeInfo() : TypeInfo(TypeKind::ERROR) {}
    };

    inline TypeInfoPtr get_error_type() {
        static TypeInfoPtr instance = std::make_shared<ErrorTypeInfo>();
        return instance;
    }

    inline TypeInfoPtr get_void_type() {
        static TypeInfoPtr instance = std::make_shared<VoidTypeInfo>();
        return instance;
    }

    inline TypeInfoPtr get_null_type() {
        static TypeInfoPtr instance = std::make_shared<NullTypeInfo>();
        return instance;
    }


    // -- Compile-time evaluation ---------------------------------------------------
    //
    // Used to check whether the `N` in `[T; N]` is legal -- array sizes must be
    // known at compile time, so `size_expr` has to reduce to a constant.
    //
    // This overload is syntax-only: it can walk literals and operator chains,
    // but a bare `NAME` might refer to a `static let` constant, and answering
    // that needs a scope lookup. Once `Scope` and `NamespaceVarSemantic` exist
    // (further down this file) there's a second overload that also handles
    // that case -- use it wherever a Scope is available, which in practice is
    // everywhere the checker actually calls this.
    inline bool is_constexpr(const ast::Expression& expr) {
        switch (expr.kind) {
            case ast::ASTNode::NodeKind::LITERAL_INT:
            case ast::ASTNode::NodeKind::LITERAL_FLOAT:
            case ast::ASTNode::NodeKind::LITERAL_BOOL:
                return true;

            case ast::ASTNode::NodeKind::OPERATION_EXPR: {
                const auto& op_expr = static_cast<const ast::OperationExpr&>(expr);
                // Unary ops store their operand in lhs and leave rhs null
                // (see OperationExpr::is_binary()); check whichever is present.
                if (op_expr.lhs && !is_constexpr(*op_expr.lhs)) return false;
                if (op_expr.rhs && !is_constexpr(*op_expr.rhs)) return false;
                return true;
            }

            // A bare name can only be constexpr via a scope lookup -- see the
            // Scope-aware overload below.
            case ast::ASTNode::NodeKind::NAME:
            default:
                return false;
        }
    }


    // ============================================================================
    // Symbols
    // ============================================================================

    /**
     * Every variable has a type, mutability, and initialization state.
     */
    struct VariableSemantic : public Symbol {
        TypeInfoPtr type;
        bool is_lvalue;        // can we assign to it?
        bool is_mutable;       // can we modify it?
        bool is_initialized;   // does it have a value?

        VariableSemantic(TypeInfoPtr t, bool lval, bool mut, bool init, bool is_pub = false, const ast::ASTNode* ast = nullptr)
            : Symbol(SymbolKind::VARIABLE, is_pub, ast), type(std::move(t)), is_lvalue(lval), is_mutable(mut), is_initialized(init) {}
    };

    /**
     * `static let`/`static let mut` at namespace or class scope (ClassImplementationDecl::static_vars,
     * or a top-level NamespaceVarDecl). Deliberately NOT a VariableSemantic:
     * a namespace var has no `is_lvalue` question (it's always addressable by
     * name, never a temporary) and lives in exactly one place in memory for
     * the whole program, unlike a field, which is per-instance and only makes
     * sense in the context of a `self`. Folding it into VariableSemantic would
     * mean carrying a meaningless is_lvalue everywhere just for this case.
     */
    struct NamespaceVarSemantic : public Symbol {
        TypeInfoPtr type;
        bool is_mutable;               // `static let` (false) vs `static let mut` (true)
        bool is_initialized;
        bool initialiser_is_constexpr; // computed once at decl time via is_constexpr(*initialiser), cached here so
                                        // later lookups (e.g. resolving `[T; max_value]`) don't re-walk the initialiser

        NamespaceVarSemantic(TypeInfoPtr t, bool mut, bool init, bool init_is_constexpr,
                              bool is_pub = false, const ast::ASTNode* ast = nullptr)
            : Symbol(SymbolKind::NAMESPACE_VAR, is_pub, ast),
              type(std::move(t)), is_mutable(mut), is_initialized(init),
              initialiser_is_constexpr(init_is_constexpr) {}
    };

    struct FunctionSemantic : public Symbol {
        std::vector<VariableSemantic> parameters;
        TypeInfoPtr return_type;
        bool is_static;  // `pub static func init(...)` etc -- no implicit `self` in `parameters[0]`.
                          // Matters because NamedTypeInfo::get_method_result_t (further down)
                          // assumes parameters[0] is self for everything in ClassDefinition::methods;
                          // static methods are called through the class name, not a value, so they
                          // never go through that path -- this flag is what tells the checker which
                          // call-resolution path applies before it gets there.

        bool is_signature_equal(const FunctionSemantic& other) const {
            if (parameters.size() != other.parameters.size()) {
                return false;
            }
            for (size_t i = 0; i < parameters.size(); ++i) {
                if (!parameters[i].type->equals(*other.parameters[i].type)) {
                    return false;
                }
            }
            return return_type->equals(*other.return_type);
        }

        FunctionSemantic(std::vector<VariableSemantic> params, TypeInfoPtr ret_type, bool is_static = false,
                          bool is_pub = false, const ast::ASTNode* ast = nullptr)
            : Symbol(SymbolKind::FUNCTION, is_pub, ast), parameters(std::move(params)),
              return_type(std::move(ret_type)), is_static(is_static) {}
    };

    struct OperatorSemantic {
        ast::OperatorKind operator_symbol;
        FunctionSemantic function_semantic;

        OperatorSemantic(ast::OperatorKind op_sym, FunctionSemantic func_sem)
            : operator_symbol(op_sym), function_semantic(std::move(func_sem)) {}
    };

    struct ClassDefinition : public Symbol {
        // Nothing else on the Symbol/ClassDefinition/TypeInfo chain stores
        // this -- Scope maps name -> Symbol, but a ClassDefinition reached
        // via a TypeInfoPtr (the normal case once you're past the initial
        // lookup) has no way back to the string it was registered under.
        // Needed for get_type_name() below to print "Point" instead of
        // "<user-defined type>" in a diagnostic.
        std::string name;

        // -- from ClassStructureDecl (pure memory blueprint) --
        //
        // A vector, not an unordered_map: declaration order IS the memory
        // layout order (see compute_class_layout below), so it has to be a
        // single source of truth rather than a map plus a separate order
        // list that could drift out of sync. Classes only have a handful of
        // fields in practice, so a linear scan in find_field() below is
        // cheap enough that there's no real case for also keeping an
        // unordered_map<string, size_t> index alongside it.
        std::vector<std::pair<std::string, VariableSemantic>> fields;

        // Linear lookup by name -- see the comment on `fields` above for why
        // this is O(n) instead of a map find. Returns nullptr if no field
        // with this name exists (caller's job to turn that into a
        // diagnostic; duplicate-name checking happens at insertion time,
        // when the field is first added, not here).
        const VariableSemantic* find_field(const std::string& name) const {
            for (const auto& [field_name, field] : fields) {
                if (field_name == name) return &field;
            }
            return nullptr;
        }

        // -- from ClassImplementationDecl --
        std::unordered_map<std::string, std::vector<FunctionSemantic>> methods;  // No two methods can have the same name and signature in a class
        std::unordered_map<ast::OperatorKind, std::vector<OperatorSemantic>> operator_overloads;  // No two operator overloads can have the same operator and signature in a class
        std::unordered_map<std::string, NamespaceVarSemantic> static_vars;  // `pub static let max_value: float = ...` etc. -- no per-instance storage
        std::optional<FunctionSemantic> destructor;  // `func drop(self: ref T)`, at most one per class

        // -- layout, for codegen (see get_size_of_type / get_align_of_type below) --
        // `size` is nullopt for a class whose fields haven't been laid out
        // yet. Deliberately optional rather than "0 means not computed" --
        // 0 is also the technically-legitimate size of an empty struct once
        // layout HAS run, so a bare unsigned couldn't tell those two states
        // apart. `align` doesn't need the same treatment: nothing ever reads
        // align without size being checked first (see get_size_of_type's
        // assert below), so 1 staying a plain "no constraint yet" default is
        // fine -- there's no code path where a stale default align is read
        // as if it were real.
        std::optional<size_t> size;  // total size in bytes
        unsigned align;              // alignment requirement, in bytes

        ClassDefinition(bool is_pub = false, const ast::ASTNode* ast = nullptr)
            : Symbol(SymbolKind::CLASS, is_pub, ast), size(std::nullopt), align(1) {}
    };


    inline TypeInfoPtr NamedTypeInfo::get_operator_result_t(ast::OperatorKind op, const std::optional<TypeInfoPtr>& rhs) const {
        auto it = definition->operator_overloads.find(op);
        if (it == definition->operator_overloads.end()) {
            return get_error_type();
        }
        
        // If there's no RHS (unary operator), match on param count
        if (!rhs.has_value()) {
            for (const auto& overload : it->second) {
                const auto& params = overload.function_semantic.parameters;
                // Unary operators have exactly ONE parameter (self)
                if (params.size() == 1) {
                    return overload.function_semantic.return_type;
                }
            }
            return get_error_type();
        }
        
        // Binary operators: params[0] = self, params[1] = rhs. Exact match
        // first -- `i32 + i32` resolves to i32's own overload without going
        // anywhere near widening.
        for (const auto& overload : it->second) {
            const auto& params = overload.function_semantic.parameters;
            if (params.size() == 2 && params[1].type->equals(*rhs.value())) {
                return overload.function_semantic.return_type;
            }
        }

        // No exact match. If self and rhs are already the same type, there's
        // nothing widening can add -- the loop just above already asked "does
        // this type define op(self, self)", and if it said no, asking again
        // via the widening path below would just be the same question a
        // second time (and, for the rhs-wider branch, would recurse into
        // this exact call forever). Bail out now instead.
        if (this->equals(*rhs.value())) {
            return get_error_type();
        }

        // Usual-arithmetic-conversions fallback: `i32 + i64` has no `(i32,
        // i32)` overload that fits (rhs is i64), but i32 widens into i64, so
        // promote both operands to i64 and check i64's OWN homogeneous
        // overload instead. This never reorders self/rhs -- `a - b` stays
        // `a - b`, both operands just get notionally widened to the same
        // common type first, exactly like C's usual arithmetic conversions.
        if (can_implicitly_widen(*rhs.value(), *this)) {
            // rhs widens into self -- self IS the common type, so check
            // self's own (T, T) overload directly. No recursive call needed:
            // `it` already IS self's overload list.
            for (const auto& overload : it->second) {
                const auto& params = overload.function_semantic.parameters;
                if (params.size() == 2 && params[1].type->equals(*params[0].type)) {
                    return overload.function_semantic.return_type;
                }
            }
        } else if (can_implicitly_widen(*this, *rhs.value())) {
            // self widens into rhs -- rhs IS the common type, so ask IT for
            // its own (T, T) overload. Safe from infinite recursion: this
            // only fires when self and rhs are different types (checked
            // above), so the recursive call's (self, rhs) pair can never be
            // the same pair we started with -- it collapses to (rhs, rhs),
            // which hits the equals() guard above on the very next call and
            // returns immediately either way.
            return rhs.value()->get_operator_result_t(op, rhs.value());
        }

        return get_error_type();
    }

    inline TypeInfoPtr NamedTypeInfo::get_method_result_t(const std::string& name, const std::vector<TypeInfoPtr>& args) const {
        auto it = definition->methods.find(name);
        if (it == definition->methods.end()) {
            return get_error_type();
        }
        for (const auto& overload : it->second) {
            // parameters[0] is self, parameters[1..] are user args
            if (overload.parameters.size() != args.size() + 1) continue;
            bool match = true;
            for (size_t i = 0; i < args.size(); ++i) {
                if (!overload.parameters[i + 1].type->equals(*args[i])) { match = false; break; }
            }
            if (match) return overload.return_type;
        }
        return get_error_type();
    }

    inline std::string NamedTypeInfo::get_type_name() const {
        return definition->name;
    }


    // -- Sizing ---------------------------------------------------------------------
    //
    // All of these are in bytes. `WORD_SIZE` is the one knob that encodes
    // "target is currently 64-bit" -- `size`, pointers/references, and the
    // bookkeeping words inside `string`/`[T]` are all expressed in terms of
    // it rather than a hardcoded 8, so retargeting to a 32-bit backend later
    // is a one-line change here instead of an audit of every call site.
    inline constexpr unsigned WORD_SIZE = 8;

    inline unsigned get_align_of_type(const TypeInfoPtr& type);  // fwd decl, mutually recursive with get_size_of_type for ARRAY

    inline unsigned get_size_of_type(const TypeInfoPtr& type) {
        switch (type->kind) {
            // Both PRIMITIVE and USER_DEFINED are NamedTypeInfo -- a class
            // with a ClassDefinition backing it, whether that class was
            // written by the user or is one of ours (i8, string, ...). Both
            // read from the same place: definition->size. There is no
            // separate "primitive size table" -- a primitive's size is set
            // once, at the point its ClassDefinition is constructed (see
            // builtins::register_builtin_types below), the exact same way a
            // user class's size is set once by compute_all_class_layouts.
            // Two DIFFERENT mechanisms used to answer the same question
            // (this hardcoded switch here, vs. reading a real
            // ClassDefinition::size there) is exactly the kind of split that
            // lets one of them go stale -- e.g. `char` silently reporting a
            // wrong size if its ClassDefinition were ever accidentally
            // shared with `i8`'s (which, before this fix, it was).
            case TypeInfo::TypeKind::PRIMITIVE:
            case TypeInfo::TypeKind::USER_DEFINED: {
                const auto& def = static_cast<const NamedTypeInfo&>(*type).definition;
                // Loudly wrong beats silently wrong: reading the size of a
                // class before it's laid out used to return 0 and let the
                // caller carry on (e.g. into pointer arithmetic on a
                // "zero-sized" type that's actually just unfinished). def->size
                // being std::optional makes "not computed yet" a real,
                // checkable state instead of indistinguishable from a
                // genuinely empty struct -- this assert is what actually
                // uses that. For a user class: compute_all_class_layouts
                // hasn't run yet, or it rejected the graph as cyclic
                // (ClassLayoutCycleError) and the caller pressed on anyway.
                // For a primitive: its ClassDefinition was built without an
                // explicit_size (see make_class_definition) -- every
                // builtins:: entry must pass one.
                assert(def->size.has_value() && "get_size_of_type: class layout not computed yet");
                return static_cast<unsigned>(def->size.value());
            }

            case TypeInfo::TypeKind::POINTER:
            case TypeInfo::TypeKind::REFERENCE:
                // Just an address, independent of what it points to.
                return WORD_SIZE;

            case TypeInfo::TypeKind::ARRAY: {
                const auto& arr = static_cast<const ArrayTypeInfo&>(*type);
                if (arr.size.has_value()) {
                    // [T; N] -- N contiguous elements, no header at all.
                    return static_cast<unsigned>(get_size_of_type(arr.element_type) * arr.size.value());
                }
                // [T] -- {data ptr, length, capacity}: 3 machine words,
                // regardless of element_type. This is why ArrayTypeInfo
                // doesn't need element_type's size to answer this case.
                return WORD_SIZE * 3;
            }

            case TypeInfo::TypeKind::NULLTYPE:
            case TypeInfo::TypeKind::VOID:
            case TypeInfo::TypeKind::ERROR:
                return 0;
        }
        return 0;
    }

    inline unsigned get_align_of_type(const TypeInfoPtr& type) {
        switch (type->kind) {
            // Same reasoning as get_size_of_type above: both are
            // NamedTypeInfo, both read definition->align, no separate table.
            case TypeInfo::TypeKind::PRIMITIVE:
            case TypeInfo::TypeKind::USER_DEFINED:
                return static_cast<const NamedTypeInfo&>(*type).definition->align;

            case TypeInfo::TypeKind::ARRAY: {
                const auto& arr = static_cast<const ArrayTypeInfo&>(*type);
                // [T; N] aligns like its element ([i32; 4] aligns like i32).
                // [T] aligns like a pointer -- it's a 3-word header, the
                // element_type's alignment doesn't come into it.
                return arr.size.has_value() ? get_align_of_type(arr.element_type) : WORD_SIZE;
            }

            // POINTER, REFERENCE, and the singletons: self-describing
            // scalars with no internal structure to over-align for, so
            // align == size.
            default:
                return get_size_of_type(type);
        }
    }

    // Naive sequential layout: fields placed in declaration order (now that
    // `fields` is a vector, this really is declaration order), each padded
    // up to its own alignment, struct align = max field align, total size
    // rounded up to that align. This is a starting point, not a final ABI --
    // it doesn't reorder fields to minimize padding and doesn't support
    // `#[repr(packed)]`-style overrides.
    //
    // Assumes every field's own class (if it has a hard dependency on one --
    // see get_hard_dependency below) has already had ITS layout computed.
    // Don't call this directly for a class with any user-defined-class
    // fields; call compute_all_class_layouts instead, which computes a valid
    // order (and rejects the graph if no valid order exists) before calling
    // this per class.
    inline void compute_class_layout(ClassDefinition& def) {
        unsigned offset = 0;
        unsigned max_align = 1;
        for (auto& [name, field] : def.fields) {
            unsigned field_size = get_size_of_type(field.type);
            unsigned field_align = get_align_of_type(field.type);
            max_align = std::max(max_align, field_align);
            offset = (offset + field_align - 1) / field_align * field_align;  // pad up to this field's alignment
            offset += field_size;
        }
        def.align = max_align;
        def.size = (offset + max_align - 1) / max_align * max_align;  // pad total size up to struct alignment -- assigns into the optional<size_t>, "computed" now has a value
    }

    // -- Layout ordering & cycle detection ------------------------------------------
    //
    // `compute_class_layout` above assumes its class's fields are already
    // laid out. Nothing enforced that -- and worse, some dependency orders
    // don't exist at all:
    //
    //   class A { b: B; }
    //   class B { a: A; }
    //
    // A needs B's size to know its own size; B needs A's size to know ITS
    // own size. Neither can go first. This isn't a "process classes in the
    // right order" bug, it's a genuinely infinite type -- same reason Rust
    // rejects this exact shape with "recursive type has infinite size" and
    // makes you write `b: box B` instead. A pointer/reference/dynamic-array
    // field doesn't have this problem because its size (WORD_SIZE, or
    // WORD_SIZE*3 for [T]) doesn't depend on what it points to -- only a
    // directly-embedded value field, or a fixed-size array of one
    // ([B; 4] embeds 4 whole Bs), creates a hard dependency.

    // Returns the ClassDefinition that must be fully laid out before `type`
    // (as a field) can contribute a size -- or nullptr if there's no such
    // dependency (POINTER, REFERENCE, dynamic [T], and non-class types all
    // return nullptr; a static [T; N] recurses into T).
    inline ClassDefinition* get_hard_dependency(const TypeInfo& type) {
        if (type.kind == TypeInfo::TypeKind::USER_DEFINED) {
            return static_cast<const NamedTypeInfo&>(type).definition.get();
        }
        if (type.kind == TypeInfo::TypeKind::ARRAY) {
            const auto& arr = static_cast<const ArrayTypeInfo&>(type);
            if (arr.size.has_value()) {
                return get_hard_dependency(*arr.element_type);
            }
        }
        return nullptr;
    }

    // The classes involved in a hard-dependency cycle, in cycle order
    // (cycle.front() and cycle.back() name the same class, closing the loop) --
    // enough for a diagnostic like "A -> B -> A: infinite size, use box/ptr/ref
    // to break the cycle".
    struct ClassLayoutCycleError {
        std::vector<ClassDefinition*> cycle;
    };

    // Lays out every class in `all_classes`, choosing a valid dependency
    // order itself (a plain DFS post-order: lay out dependencies, then
    // yourself) rather than requiring the caller to have already sorted
    // them -- callers generally can't, since classes can reference each
    // other regardless of declaration or file order. Returns the cycle if
    // one exists instead of laying anything out; call this once, after every
    // ClassDefinition in the module (or program, for cross-module field
    // types) has been constructed via make_class_definition, and before
    // anything downstream asks get_size_of_type/get_align_of_type about a
    // USER_DEFINED type.
    inline std::optional<ClassLayoutCycleError> compute_all_class_layouts(
        const std::vector<std::shared_ptr<ClassDefinition>>& all_classes)
    {
        enum class Mark { UNVISITED, IN_PROGRESS, DONE };
        std::unordered_map<ClassDefinition*, Mark> marks;
        for (const auto& c : all_classes) marks[c.get()] = Mark::UNVISITED;

        std::vector<ClassDefinition*> stack;  // current DFS path, for cycle reporting
        std::optional<ClassLayoutCycleError> error;

        std::function<void(ClassDefinition*)> visit = [&](ClassDefinition* def) {
            if (error.has_value() || marks[def] == Mark::DONE) return;

            marks[def] = Mark::IN_PROGRESS;
            stack.push_back(def);

            for (const auto& [name, field] : def->fields) {
                ClassDefinition* dep = get_hard_dependency(*field.type);
                if (!dep) continue;

                if (marks[dep] == Mark::IN_PROGRESS) {
                    // `dep` is already on the current path -- everything from
                    // its first appearance onward, plus `dep` again to close
                    // the loop, is the cycle.
                    auto it = std::find(stack.begin(), stack.end(), dep);
                    std::vector<ClassDefinition*> cycle(it, stack.end());
                    cycle.push_back(dep);
                    error = ClassLayoutCycleError{std::move(cycle)};
                    return;
                }
                if (marks[dep] == Mark::UNVISITED) {
                    visit(dep);
                    if (error.has_value()) return;
                }
            }

            compute_class_layout(*def);
            marks[def] = Mark::DONE;
            stack.pop_back();
        };

        for (const auto& c : all_classes) {
            visit(c.get());
            if (error.has_value()) return error;
        }
        return std::nullopt;
    }


    // -- Semantic node factories ----------------------------------------------------
    //
    // Thin wrappers around the constructors above. Two of them (VariableSemantic,
    // FunctionSemantic) are genuinely trivial right now -- they exist so every
    // Symbol subtype is built the same way, and so there's one place to add
    // validation later (e.g. "a field's type can't be VOID") without having to
    // hunt down every call site that currently calls the constructor directly.

    inline VariableSemantic make_variable_semantic(TypeInfoPtr type, bool is_lvalue, bool is_mutable, bool is_initialized,
                                                    bool is_pub = false, const ast::ASTNode* ast = nullptr) {
        return VariableSemantic(std::move(type), is_lvalue, is_mutable, is_initialized, is_pub, ast);
    }

    // Used for free functions, instance methods, and static methods alike --
    // `is_static` is what tells them apart (see the comment on FunctionSemantic
    // itself). Note `parameters` for an instance method must already include
    // `self` as parameters[0] by the time it reaches here -- this factory
    // doesn't inject it; that's the caller's job when walking a
    // ClassMethodDecl, since only the caller knows whether `self` was `ref`
    // or `mut ref`.
    inline FunctionSemantic make_function_semantic(std::vector<VariableSemantic> parameters, TypeInfoPtr return_type,
                                                     bool is_static = false, bool is_pub = false,
                                                     const ast::ASTNode* ast = nullptr) {
        return FunctionSemantic(std::move(parameters), std::move(return_type), is_static, is_pub, ast);
    }

    // OperatorSemantic isn't itself a Symbol (see its definition above), so
    // there's no is_pub/ast to thread through here -- build the FunctionSemantic
    // with make_function_semantic first (is_static should always be false for
    // an operator overload; `pub operator+(self: ref Point, other: ref Point)`
    // always takes an implicit self, unlike a static method), then wrap it.
    inline OperatorSemantic make_operator_semantic(ast::OperatorKind op, FunctionSemantic function_semantic) {
        return OperatorSemantic(op, std::move(function_semantic));
    }

    // The one non-trivial factory. Building a ClassDefinition by hand (as in
    // the sketch this was based on) is an easy place to introduce a subtle
    // bug: declaring the shared_ptr `static` inside the function means every
    // call returns the SAME ClassDefinition instance, so `class Point` and
    // `class Vec3` would silently alias one object. `static` locals are
    // exactly right for the VOID/NULLTYPE/ERROR singletons up in get_void_type()
    // etc (deliberately only one instance should ever exist) -- but a class
    // definition needs a fresh instance every single call, since every
    // `class` declaration in the program is a distinct type. A plain local
    // (no `static`) is what makes that true.
    //
    // Deliberately does NOT call compute_class_layout itself for the
    // ordinary (fields-derived) case. A single class being constructed can
    // reference other classes that haven't been built yet at all (forward
    // reference, another module, or -- see compute_all_class_layouts above
    // -- part of a cycle), so layout can't happen per-class as each one is
    // made. Call compute_all_class_layouts once, after every class in the
    // module has been constructed -- unless explicit_size/explicit_align
    // were given, in which case there's nothing for that pass to do for
    // this one; see below.
    //
    // `explicit_size`/`explicit_align`, if given, are written straight to
    // ClassDefinition::size/align instead of leaving them at 0/1 for
    // compute_all_class_layouts to fill in later. This is for callers who
    // already know their type's layout up front and don't derive it from
    // `fields` at all -- the builtins are the case in point: i8/i32/string/
    // etc. all have zero fields (they really don't have any), so there's
    // nothing for a layout pass to compute from, but they still need a real
    // size for get_size_of_type to return once PRIMITIVE reads
    // definition->size the same way USER_DEFINED does (see get_size_of_type
    // above). Leave both as nullopt for an ordinary user class -- that's
    // what tells compute_all_class_layouts it still needs to run for this
    // one.
    inline std::shared_ptr<ClassDefinition> make_class_definition(
        std::string name,
        std::vector<std::pair<std::string, VariableSemantic>> fields,
        std::unordered_map<std::string, std::vector<FunctionSemantic>> methods,
        std::unordered_map<ast::OperatorKind, std::vector<OperatorSemantic>> operator_overloads,
        std::unordered_map<std::string, NamespaceVarSemantic> static_vars,
        std::optional<FunctionSemantic> destructor,
        bool is_pub = false,
        const ast::ASTNode* ast = nullptr,
        std::optional<size_t> explicit_size = std::nullopt,
        std::optional<unsigned> explicit_align = std::nullopt)
    {
        auto def = std::make_shared<ClassDefinition>(is_pub, ast);
        def->name = std::move(name);
        def->fields = std::move(fields);
        def->methods = std::move(methods);
        def->operator_overloads = std::move(operator_overloads);
        def->static_vars = std::move(static_vars);
        def->destructor = std::move(destructor);
        if (explicit_size.has_value())  def->size  = explicit_size.value();
        if (explicit_align.has_value()) def->align = explicit_align.value();
        return def;
    }


    struct Scope {
        std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
        std::shared_ptr<Scope> parent_scope;

        Scope(std::shared_ptr<Scope> parent = nullptr) : parent_scope(std::move(parent)) {}

        void add_symbol(const std::string& name, std::shared_ptr<Symbol> symbol) {
            symbols[name] = std::move(symbol);
        }

        std::shared_ptr<Symbol> lookup(const std::string& name) const {
            auto it = symbols.find(name);
            if (it != symbols.end()) {
                return it->second;
            }
            if (parent_scope) {
                return parent_scope->lookup(name);
            }
            return nullptr;  // Not found
        }
    };

    // Scope-aware is_constexpr: adds the one case the syntax-only overload
    // above can't handle by itself -- a bare NAME that resolves to an
    // immutable namespace var whose own initialiser was already constexpr
    // (e.g. `max_value` in `[Point; max_value]`, given `pub static let
    // max_value: float = 10000;`). `is_mutable` is checked because a `static
    // let mut` can be reassigned at runtime, so even a constexpr initial
    // value doesn't make later reads of it a compile-time constant.
    inline bool is_constexpr(const ast::Expression& expr, const Scope& scope) {
        if (expr.kind == ast::ASTNode::NodeKind::NAME) {
            const auto& name = static_cast<const ast::Name&>(expr);
            auto sym = scope.lookup(name.identifier);
            if (!sym || sym->kind != Symbol::SymbolKind::NAMESPACE_VAR) return false;
            const auto& ns_var = static_cast<const NamespaceVarSemantic&>(*sym);
            return !ns_var.is_mutable && ns_var.initialiser_is_constexpr;
        }
        if (expr.kind == ast::ASTNode::NodeKind::OPERATION_EXPR) {
            const auto& op_expr = static_cast<const ast::OperationExpr&>(expr);
            if (op_expr.lhs && !is_constexpr(*op_expr.lhs, scope)) return false;
            if (op_expr.rhs && !is_constexpr(*op_expr.rhs, scope)) return false;
            return true;
        }
        return is_constexpr(expr);  // falls back to the literal-only cases
    }

    // The other reason NamespaceVarSemantic gets a factory instead of a bare
    // constructor call: `is_initialized` and `initialiser_is_constexpr` are
    // both derivable from the initialiser expression itself, so there's no
    // reason to make every call site recompute (or worse, mis-set) them by
    // hand. `initialiser` may be nullptr (an uninitialized `static let mut`,
    // if the language allows that) -- in that case both derived flags are
    // false, since there's nothing to evaluate.
    inline NamespaceVarSemantic make_namespace_var_semantic(TypeInfoPtr type, bool is_mutable,
                                                              const ast::Expression* initialiser, const Scope& scope,
                                                              bool is_pub = false, const ast::ASTNode* ast = nullptr) {
        const bool is_initialized = (initialiser != nullptr);
        const bool init_is_constexpr = is_initialized && is_constexpr(*initialiser, scope);
        return NamespaceVarSemantic(std::move(type), is_mutable, is_initialized, init_is_constexpr, is_pub, ast);
    }

    namespace builtins {

        // `const shared_ptr<ClassDefinition>`, not `const ClassDefinition`: the
        // POINTER is fixed (never reseated), but the ClassDefinition it points to
        // stays mutable, so a later register_builtin_operators() pass can still fill in
        // operator_overloads/methods/fields after this file has loaded. Declaring
        // these `const ClassDefinition` outright would make them permanently
        // empty -- fine for `equals()` and identity, useless for actually typing
        // `i32 + i32`.
        //
        // `inline` because this is a header -- without it, including builtins.h
        // from more than one translation unit is an ODR violation.
        //
        // Built via make_class_definition with explicit_size/explicit_align
        // rather than left for compute_all_class_layouts: primitives have no
        // fields for a layout pass to derive a size from (they really don't
        // have any -- `fields` is empty here for exactly the same reason it'd
        // be empty on a genuinely fieldless user class), so their size has to
        // be supplied directly, once, at the one place each is constructed.
        // `methods`/`operator_overloads` start empty and get filled in later
        // by register_builtin_operators() (e.g. `i32 + i32`) -- that's the "stays
        // mutable" part of the comment above; `size`/`align`, once set here,
        // never need to change again.
        //
        // IMPORTANT: each primitive gets its OWN ClassDefinition. Sharing one
        // between two primitives (e.g. giving `char` the same ClassDefinition
        // as `i8`, which get_char_type() used to do below) doesn't just get the
        // size wrong -- NamedTypeInfo::equals() compares
        // `definition.get() == other.definition.get()`, so two primitives
        // sharing a definition become the SAME type as far as the checker is
        // concerned. `i8 == char` would silently type-check.
        inline const std::shared_ptr<ClassDefinition> XENON_I8_DEF = make_class_definition("i8", {}, {}, {}, {}, std::nullopt, true, nullptr, 1, 1);
        inline const std::shared_ptr<ClassDefinition> XENON_I16_DEF = make_class_definition("i16", {}, {}, {}, {}, std::nullopt, true, nullptr, 2, 2);
        inline const std::shared_ptr<ClassDefinition> XENON_I32_DEF = make_class_definition("i32", {}, {}, {}, {}, std::nullopt, true, nullptr, 4, 4);
        inline const std::shared_ptr<ClassDefinition> XENON_I64_DEF = make_class_definition("i64", {}, {}, {}, {}, std::nullopt, true, nullptr, WORD_SIZE, WORD_SIZE);

        inline const std::shared_ptr<ClassDefinition> XENON_U8_DEF = make_class_definition("u8", {}, {}, {}, {}, std::nullopt, true, nullptr, 1, 1);
        inline const std::shared_ptr<ClassDefinition> XENON_U16_DEF = make_class_definition("u16", {}, {}, {}, {}, std::nullopt, true, nullptr, 2, 2);
        inline const std::shared_ptr<ClassDefinition> XENON_U32_DEF = make_class_definition("u32", {}, {}, {}, {}, std::nullopt, true, nullptr, 4, 4);
        inline const std::shared_ptr<ClassDefinition> XENON_U64_DEF = make_class_definition("u64", {}, {}, {}, {}, std::nullopt, true, nullptr, WORD_SIZE, WORD_SIZE);

        inline const std::shared_ptr<ClassDefinition> XENON_FLOAT_DEF = make_class_definition("float", {}, {}, {}, {}, std::nullopt, true, nullptr, 4, 4);
        inline const std::shared_ptr<ClassDefinition> XENON_DOUBLE_DEF = make_class_definition("double", {}, {}, {}, {}, std::nullopt, true, nullptr, WORD_SIZE, WORD_SIZE);
        // 2 x WORD_SIZE-byte components (real, imaginary) -- aligns like one
        // component (WORD_SIZE), not like its own total size, the same way a
        // C `struct { double re, im; }` aligns to 8 even though sizeof is 16.
        inline const std::shared_ptr<ClassDefinition> XENON_CPLX128_DEF = make_class_definition("cplx128", {}, {}, {}, {}, std::nullopt, true, nullptr, WORD_SIZE * 2, WORD_SIZE);

        inline const std::shared_ptr<ClassDefinition> XENON_SIZE_DEF = make_class_definition("size", {}, {}, {}, {}, std::nullopt, true, nullptr, WORD_SIZE, WORD_SIZE);

        inline const std::shared_ptr<ClassDefinition> XENON_BOOL_DEF = make_class_definition("bool", {}, {}, {}, {}, std::nullopt, true, nullptr, 1, 1);
        // 4 bytes -- Xenon chars are Unicode scalar values, not bytes; a
        // distinct ClassDefinition from XENON_I8_DEF (see the IMPORTANT note
        // above) is exactly what makes that size stick.
        inline const std::shared_ptr<ClassDefinition> XENON_CHAR_DEF = make_class_definition("char", {}, {}, {}, {}, std::nullopt, true, nullptr, 4, 4);
        // {data ptr, length} -- 2 machine words; aligns like one word, same
        // reasoning as cplx128 above.
        inline const std::shared_ptr<ClassDefinition> XENON_STRING_DEF = make_class_definition("string", {}, {}, {}, {}, std::nullopt, true, nullptr, WORD_SIZE * 2, WORD_SIZE);



        // One accessor per primitive, wrapping its ClassDefinition in the
        // TypeInfoPtr that the rest of the checker actually deals in. All of these
        // are the same three lines; a small generator would replace this list once
        // it's finalized, rather than hand-writing ~15 of them.
        inline TypeInfoPtr get_i8_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::I8, XENON_I8_DEF);
            return t;
        }
        inline TypeInfoPtr get_i16_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::I16, XENON_I16_DEF);
            return t;
        }
        inline TypeInfoPtr get_i32_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::I32, XENON_I32_DEF);
            return t;
        }
        inline TypeInfoPtr get_i64_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::I64, XENON_I64_DEF);
            return t;
        }

        inline TypeInfoPtr get_u8_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::U8, XENON_U8_DEF);
            return t;
        }
        inline TypeInfoPtr get_u16_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::U16, XENON_U16_DEF);
            return t;
        }
        inline TypeInfoPtr get_u32_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::U32, XENON_U32_DEF);
            return t;
        }
        inline TypeInfoPtr get_u64_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::U64, XENON_U64_DEF);
            return t;
        }


        inline TypeInfoPtr get_float_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::FLOAT, XENON_FLOAT_DEF);
            return t;
        }
        inline TypeInfoPtr get_double_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::DOUBLE, XENON_DOUBLE_DEF);
            return t;
        }
        inline TypeInfoPtr get_cplx128_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::CPLX128, XENON_CPLX128_DEF);
            return t;
        }

        inline TypeInfoPtr get_size_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::SIZE, XENON_SIZE_DEF);
            return t;
        }


        inline TypeInfoPtr get_char_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::CHAR, XENON_CHAR_DEF);
            return t;
        }
        inline TypeInfoPtr get_string_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::STRING, XENON_STRING_DEF);
            return t;
        }
        inline TypeInfoPtr get_bool_type() {
            static TypeInfoPtr t = std::make_shared<PrimitiveTypeInfo>(PrimitiveTypeInfo::PrimitiveKind::BOOL, XENON_BOOL_DEF);
            return t;
        }

        // -- Builtin operators -----------------------------------------------------
        //
        // Every ClassDefinition above was constructed with an empty
        // operator_overloads -- this is what actually fills it in. Split from
        // register_builtin_types because this only touches the
        // ClassDefinitions directly (no Scope involved); call both once at
        // startup, in either order.
        namespace detail {
            // One (T, T) -> return_type overload. Parameters are plain `T`,
            // not `mut ref T` -- every builtin here is a cheap scalar passed
            // by value, unlike a real class's `self: ref Point`.
            inline void add_binary_op(const std::shared_ptr<ClassDefinition>& def, ast::OperatorKind op,
                                       const TypeInfoPtr& operand_type, const TypeInfoPtr& return_type) {
                std::vector<VariableSemantic> params{
                    make_variable_semantic(operand_type, /*is_lvalue=*/true, /*is_mutable=*/false, /*is_initialized=*/true),
                    make_variable_semantic(operand_type, /*is_lvalue=*/true, /*is_mutable=*/false, /*is_initialized=*/true),
                };
                def->operator_overloads[op].push_back(
                    make_operator_semantic(op, make_function_semantic(std::move(params), return_type)));
            }

            inline constexpr std::array<ast::OperatorKind, 5> ARITHMETIC_OPS = {
                ast::OperatorKind::ADD, ast::OperatorKind::SUBTRACT, ast::OperatorKind::MULTIPLY,
                ast::OperatorKind::DIVIDE, ast::OperatorKind::MODULO,
            };
            inline constexpr std::array<ast::OperatorKind, 6> COMPARISON_OPS = {
                ast::OperatorKind::EQUAL, ast::OperatorKind::NOT_EQUAL, ast::OperatorKind::LESS_THAN,
                ast::OperatorKind::LESS_EQUAL, ast::OperatorKind::GREATER_THAN, ast::OperatorKind::GREATER_EQUAL,
            };
            inline constexpr std::array<ast::OperatorKind, 5> BITWISE_OPS = {
                ast::OperatorKind::BITWISE_AND, ast::OperatorKind::BITWISE_OR, ast::OperatorKind::BITWISE_XOR,
                ast::OperatorKind::SHIFT_LEFT, ast::OperatorKind::SHIFT_RIGHT,
            };
        }

        // +, -, *, /, %  (T, T) -> T,  and  ==, !=, <, <=, >, >=  (T, T) -> bool.
        // This is what the widening fallback in NamedTypeInfo::get_operator_result_t
        // actually promotes UP TO: `i32 + i64` only works because i64 has one
        // of these homogeneous overloads and i32 widens into i64 -- there's
        // no separate mixed-width overload to register, on purpose.
        inline void register_numeric_operators(const std::shared_ptr<ClassDefinition>& def, const TypeInfoPtr& self_type) {
            for (auto op : detail::ARITHMETIC_OPS) detail::add_binary_op(def, op, self_type, self_type);
            for (auto op : detail::COMPARISON_OPS) detail::add_binary_op(def, op, self_type, get_bool_type());
        }

        // &, |, ^, <<, >>  (T, T) -> T -- integer types only. Not defined for
        // FLOAT/DOUBLE/CPLX128, so this is a separate call rather than folded
        // into register_numeric_operators and silently skipped per-type there.
        inline void register_bitwise_operators(const std::shared_ptr<ClassDefinition>& def, const TypeInfoPtr& self_type) {
            for (auto op : detail::BITWISE_OPS) detail::add_binary_op(def, op, self_type, self_type);
        }

        // Fills in every builtin arithmetic/comparison/bitwise operator
        // overload on the ClassDefinitions constructed above.
        //
        // Deliberately NOT included here -- left for a follow-up rather than
        // guessed at:
        //   - Compound assignment (+=, -=, ...): these need `self: mut ref T`
        //     and return void (or self), a different shape than the pure
        //     value-in-value-out overloads every helper above assumes.
        //   - Unary NEGATE / BITWISE_NOT.
        //   - string's `+` (concatenation) and `==`/`!=`; bool's `==`/`!=`;
        //     char's ordering comparisons. Each of those is a real language
        //     design call (does string support `<` for lexicographic
        //     ordering? is bool's `&` a type error or logical-and?) rather
        //     than something to silently assume here.
        inline void register_builtin_operators() {
            register_numeric_operators(XENON_I8_DEF, get_i8_type());
            register_numeric_operators(XENON_I16_DEF, get_i16_type());
            register_numeric_operators(XENON_I32_DEF, get_i32_type());
            register_numeric_operators(XENON_I64_DEF, get_i64_type());
            register_bitwise_operators(XENON_I8_DEF, get_i8_type());
            register_bitwise_operators(XENON_I16_DEF, get_i16_type());
            register_bitwise_operators(XENON_I32_DEF, get_i32_type());
            register_bitwise_operators(XENON_I64_DEF, get_i64_type());

            register_numeric_operators(XENON_U8_DEF, get_u8_type());
            register_numeric_operators(XENON_U16_DEF, get_u16_type());
            register_numeric_operators(XENON_U32_DEF, get_u32_type());
            register_numeric_operators(XENON_U64_DEF, get_u64_type());
            register_bitwise_operators(XENON_U8_DEF, get_u8_type());
            register_bitwise_operators(XENON_U16_DEF, get_u16_type());
            register_bitwise_operators(XENON_U32_DEF, get_u32_type());
            register_bitwise_operators(XENON_U64_DEF, get_u64_type());

            register_numeric_operators(XENON_FLOAT_DEF, get_float_type());
            register_numeric_operators(XENON_DOUBLE_DEF, get_double_type());
        }

        // Registers every builtin name into `scope` so `i32`, `string`, etc.
        // resolve via ordinary Scope::lookup the same way a user class would --
        // call this once on g_parent_scope at startup, before parsing anything.
        inline void register_builtin_types(Scope& scope) {
            scope.add_symbol("i8", XENON_I8_DEF);
            scope.add_symbol("i16", XENON_I16_DEF);
            scope.add_symbol("i32", XENON_I32_DEF);
            scope.add_symbol("i64", XENON_I64_DEF);
            scope.add_symbol("u8", XENON_U8_DEF);
            scope.add_symbol("u16", XENON_U16_DEF);
            scope.add_symbol("u32", XENON_U32_DEF);
            scope.add_symbol("u64", XENON_U64_DEF);
            scope.add_symbol("float", XENON_FLOAT_DEF);
            scope.add_symbol("double", XENON_DOUBLE_DEF);
            scope.add_symbol("cplx128", XENON_CPLX128_DEF);
            scope.add_symbol("size", XENON_SIZE_DEF);
            scope.add_symbol("bool", XENON_BOOL_DEF);
            scope.add_symbol("char", XENON_CHAR_DEF);
            scope.add_symbol("string", XENON_STRING_DEF);

            // Deliberately not registering nulltype/void here -- they're not
            // NamedTypeInfo (see types.h), have no ClassDefinition, and aren't
            // written as ordinary type names in source (`void` only appears as a
            // return type, `nulltype` is never spelled by the user, it's the type
            // of a `nullptr` literal). Nothing to look up by name.
        }
    }


} // namespace xenon::semantic 