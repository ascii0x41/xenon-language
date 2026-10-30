#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <array>
#include <format>
#include <memory>
#include <limits>

#include "ast/astlib.h"


[[noreturn]] inline void unreachable() {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#else
    std::abort();
#endif
}

// Word size and alignment (default targets). These can be adjusted at
// runtime by the semantic analyser based on the target triple.
// Use inline variables instead of macros so they can be changed later.


namespace xenon::semantic {
    struct Scope;

    using ByteSize = uint64_t;
    using Alignment = uint64_t;

    // Default word size and alignment; semantic analyser may override.
    inline ByteSize WORD_SIZE = 8;
    inline Alignment WORD_ALIGN = 8;
    
    enum class SymbolKind { VARIABLE, FUNCTION, FUNCTION_VARIANTS, CLASS, TYPE };
    struct Symbol {
        std::string name;
        const ast::ASTNode* node;
        bool is_exported;
        SymbolKind kind;

        explicit Symbol(std::string name_, const ast::ASTNode* node_, bool is_exported_, SymbolKind kind_)
            : name(std::move(name_)), node(node_), is_exported(is_exported_), kind(kind_) {}
        ~Symbol() = default;
    };

    struct Type;

    struct Variable : public Symbol {
        Type* type = nullptr;
        bool is_mutable = false;
        explicit Variable(std::string name_, const ast::ASTNode* node_, bool is_exported_, Type* type_, bool is_mutable_)
            : Symbol(std::move(name_), node_, is_exported_, SymbolKind::VARIABLE), type(type_), is_mutable(is_mutable_) {}
        ~Variable() = default;
    };

    struct Parameter {
        std::string name;
        Type* type = nullptr;
        bool is_mutable = false;
        Parameter() = default;
        explicit Parameter(std::string name_, Type* type_, bool is_mutable_)
            : name(std::move(name_)), type(type_), is_mutable(is_mutable_) {}
        ~Parameter() = default;
    };

    struct Field {
        std::string name;
        Type* type = nullptr;
        bool is_public = false;
        Field() = default;
        explicit Field(std::string name_, Type* type_, bool is_public_)
            : name(std::move(name_)), type(type_), is_public(is_public_) {}
        ~Field() = default;
    };

    struct Function : public Symbol {
        std::vector<Parameter> parameters;
        Type* return_type = nullptr;
        explicit Function(std::string name_, const ast::ASTNode* node_, bool is_exported_, std::vector<Parameter> parameters_, Type* return_type_)
            : Symbol(std::move(name_), node_, is_exported_, SymbolKind::FUNCTION),
            parameters(std::move(parameters_)), return_type(return_type_) {}
        ~Function() = default;
    };

    struct FunctionVariants : public Symbol {
        std::vector<Function*> overloads;

        FunctionVariants() : Symbol("", nullptr, false, SymbolKind::FUNCTION_VARIANTS) {}

        explicit FunctionVariants(std::string name_)
            : Symbol(std::move(name_), nullptr, false, SymbolKind::FUNCTION_VARIANTS) {}

        bool empty() const {
            return overloads.empty();
        }
    };

    enum class OperatorKind {
        ADD, SUB, MUL, DIV, MOD,
        IADD, ISUB, IMUL, IDIV, IMOD,
        EQ, NEQ, LT, LTE, GT, GTE,
        BADD, BOR, BXOR, BSHFTL, BSHFTR,
        IBADD, IBOR, IBXOR, IBSHFTL, IBSHFTR,
        IDX,
        NEG, BNOT,
        // Not overloadable by users
        LOGICAL_AND, LOGICAL_OR, LOGICAL_NOT, ADDRESS_OF, DEREFERENCE,
    };

    struct Operator {
        OperatorKind kind;
        std::vector<Parameter> parameters;
        Type* return_type = nullptr;
        ast::ASTNode* node = nullptr;

        Operator() = default;
        explicit Operator(OperatorKind op_kind, std::vector<Parameter> parameters_, Type* return_type_, ast::ASTNode* n = nullptr)
            : kind(op_kind), parameters(std::move(parameters_)), return_type(return_type_), node(n) {}
        ~Operator() = default;
    };


    struct ClassImplementationSemantic;

    enum class TypeKind {
        USER_DEFINED, PRIMITIVE, POINTER, REFERENCE, ARRAY, NULLTYPE, VOID, ERROR
    };

    struct Type : Symbol {
        TypeKind kind;

        std::vector<Field> fields;

        std::unordered_map<
            std::string,
            FunctionVariants
        > methods;

        std::unordered_map<
            std::string,
            FunctionVariants
        > static_methods;

        std::vector<std::unique_ptr<Function>> owned_functions;

        std::unordered_map<
            std::string,
            Field
        > static_fields;

        std::unordered_map<
            OperatorKind,
            std::vector<Operator>
        > operators;

        std::optional<ByteSize> size;
        Alignment align;


        Type(
            TypeKind k,
            std::string n,
            std::vector<Field> f = {},
            std::optional<ByteSize> s = std::nullopt,
            Alignment a = 1
        )
            : Symbol(std::move(n), nullptr, false, SymbolKind::TYPE),
            kind(k),
            fields(std::move(f)),
            size(s),
            align(a) {}

        virtual ~Type() = default;

        void add_method(const Function& method) {
            auto owned = std::make_unique<Function>(method);
            Function* raw = owned.get();
            auto& variants = methods.try_emplace(method.name, FunctionVariants(method.name)).first->second;
            variants.overloads.push_back(raw);
            owned_functions.push_back(std::move(owned));
        }

        void add_method(std::unique_ptr<Function> method) {
            Function* raw = method.get();
            auto& variants = methods.try_emplace(raw->name, FunctionVariants(raw->name)).first->second;
            variants.overloads.push_back(raw);
            owned_functions.push_back(std::move(method));
        }

        void add_static_method(const Function& method) {
            auto owned = std::make_unique<Function>(method);
            Function* raw = owned.get();
            auto& variants = static_methods.try_emplace(method.name, FunctionVariants(method.name)).first->second;
            variants.overloads.push_back(raw);
            owned_functions.push_back(std::move(owned));
        }

        void add_static_method(std::unique_ptr<Function> method) {
            Function* raw = method.get();
            auto& variants = static_methods.try_emplace(raw->name, FunctionVariants(raw->name)).first->second;
            variants.overloads.push_back(raw);
            owned_functions.push_back(std::move(method));
        }

        void add_operator(const Operator& op) {
            operators[op.kind].push_back(op);
        }

        void add_static_field(const Field& field) {
            static_fields[field.name] = field;
        }

        void add_field(const Field& field) {
            fields.push_back(field);
        }
    };


    struct BuiltinType : Type {
        enum class BuiltinKind {
            U8, U16, U32, U64,
            I8, I16, I32, I64,
            F32, F64,
            CPLX128,
            STRING,
            BOOL,
            CHAR,
            SIZE
        } kind;

        static constexpr std::string_view builtin_to_string(BuiltinKind kind) {
            switch (kind) {
                case BuiltinKind::U8:      return "u8";
                case BuiltinKind::U16:     return "u16";
                case BuiltinKind::U32:     return "u32";
                case BuiltinKind::U64:     return "u64";

                case BuiltinKind::I8:      return "i8";
                case BuiltinKind::I16:     return "i16";
                case BuiltinKind::I32:     return "i32";
                case BuiltinKind::I64:     return "i64";

                case BuiltinKind::F32:     return "f32";
                case BuiltinKind::F64:     return "f64";

                case BuiltinKind::CPLX128: return "cplx128";
                case BuiltinKind::STRING:  return "string";
                case BuiltinKind::BOOL:    return "bool";
                case BuiltinKind::CHAR:    return "char";
                case BuiltinKind::SIZE:    return "size";
            }

            unreachable();
        }

        static constexpr ByteSize builtin_size(BuiltinKind kind) {
            switch (kind) {
                case BuiltinKind::U8:      return 1;
                case BuiltinKind::U16:     return 2;
                case BuiltinKind::U32:     return 4;
                case BuiltinKind::U64:     return 8;

                case BuiltinKind::I8:      return 1;
                case BuiltinKind::I16:     return 2;
                case BuiltinKind::I32:     return 4;
                case BuiltinKind::I64:     return 8;

                case BuiltinKind::F32:     return 4;
                case BuiltinKind::F64:     return 8;

                case BuiltinKind::CPLX128: return 8;
                case BuiltinKind::BOOL:    return 1;
                case BuiltinKind::CHAR:    return 4;
                case BuiltinKind::STRING:  return WORD_SIZE + WORD_SIZE; // pointer + length
                case BuiltinKind::SIZE:    return WORD_SIZE;
            }

            unreachable();
        }

        static constexpr Alignment builtin_align(BuiltinKind kind) {
            switch (kind) {
                case BuiltinKind::U8:      return 1;
                case BuiltinKind::U16:     return 2;
                case BuiltinKind::U32:     return 4;
                case BuiltinKind::U64:     return 8;

                case BuiltinKind::I8:      return 1;
                case BuiltinKind::I16:     return 2;
                case BuiltinKind::I32:     return 4;
                case BuiltinKind::I64:     return 8;

                case BuiltinKind::F32:     return 4;
                case BuiltinKind::F64:     return 8;

                case BuiltinKind::CPLX128: return 8;
                case BuiltinKind::BOOL:    return 1;
                case BuiltinKind::CHAR:    return 4;
                case BuiltinKind::STRING:  return WORD_ALIGN;
                case BuiltinKind::SIZE:    return WORD_ALIGN;
            }

            unreachable();
        }

        explicit BuiltinType(BuiltinKind k)
            : Type(TypeKind::PRIMITIVE, std::string(BuiltinType::builtin_to_string(k))),
            kind(k)
        {
            size = BuiltinType::builtin_size(k);
            align = BuiltinType::builtin_align(k);
        }
    };

    struct NullType : public Type {
        explicit NullType()
            : Type(TypeKind::NULLTYPE, "null") {
                size = 0;
                align = 1;
            }
    };

    struct VoidType : public Type {
        explicit VoidType()
            : Type(TypeKind::VOID, "void") {
                size = 0;
                align = 1;
            }
    };

    struct ErrorType : public Type {
        explicit ErrorType()
            : Type(TypeKind::ERROR, "<error>") {
                size = 0;
                align = 1;
            }
    };

    struct PointerType : public Type {
        Type* pointee_type;
        bool is_mutable;

        explicit PointerType(Type* p_type, bool is_mut = false)
            : Type(TypeKind::POINTER, std::format("*{}{}", (is_mut ? "mut " : ""), p_type->name)), pointee_type(p_type), is_mutable(is_mut) {
                size = WORD_SIZE;
                align = WORD_ALIGN;
            }
    };

    struct ReferenceType : public Type {
        Type* referent_type;
        bool is_mutable;

        explicit ReferenceType(Type* r_type, bool is_mut = false)
            : Type(TypeKind::REFERENCE, std::format("&{}{}", (is_mut ? "mut " : ""), r_type->name)), referent_type(r_type), is_mutable(is_mut) {
                size = WORD_SIZE;
                align = WORD_ALIGN;
            }
    };

    struct ArrayType : public Type {
        Type* element_type;
        std::optional<ByteSize> length;

        explicit ArrayType(Type* elem_type, std::optional<ByteSize> len = std::nullopt)
            : Type(TypeKind::ARRAY, std::format("[{}{}]", elem_type->name, len.has_value() ? std::format("; {}", len.value()) : "")), element_type(elem_type), length(len) {
            if (length.has_value()) {
                size = element_type->size.value_or(0) * length.value();
                align = element_type->align;
            } else {
                size = WORD_SIZE * 3;
                align = WORD_ALIGN;
            }
        }
    };

    inline Alignment align_up(Alignment value, Alignment alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    struct FieldLayout {
        Field field;
        ByteSize offset;
        ByteSize size;
        Alignment align;
    };

    struct TypeLayout {
        std::vector<FieldLayout> fields;
        ByteSize size;
        Alignment align;
    };

    ByteSize calculate_type_size(Type* type);
    Alignment calculate_type_align(Type* type);
    TypeLayout calculate_type_layout(Type* type);

    // ------------------------------------------------------------
    // TypeRegistry (moved here from scope.h)
    // ------------------------------------------------------------
    class TypeRegistry {
        std::unordered_map<std::string, std::unique_ptr<Type>> types;
        std::unordered_map<TypeKind, std::vector<Type*>> types_by_kind;
        std::unique_ptr<Type> error_type = std::make_unique<ErrorType>();

        Type* register_builtin_type(BuiltinType::BuiltinKind kind);

    public:
        Type* register_type(std::unique_ptr<Type> type);
        Type* register_pointer(Type* type, bool is_mut = false);
        Type* register_reference(Type* type, bool is_mut = false);
        Type* register_array_type(Type* elem_type, std::optional<unsigned> arr_length = std::nullopt);

        std::vector<Type*> all_types() const;

        TypeRegistry() = default;
        ~TypeRegistry() = default;

        void initialise_builtin_types(Scope& global_scope);

        Type* get_builtin_type(BuiltinType::BuiltinKind kind);
        Type* get_type(const std::string& full_name) const;
        Type* get_pointer(Type* type, bool is_mutable);
        Type* get_reference(Type* type, bool is_mutable);
        Type* get_array(Type* element_type, std::optional<size_t> length = std::nullopt);
        Type* get_void_type();
        Type* get_null_type();
        Type* get_error_type();

        bool is_signed_integer(const Type* type) const;
        bool is_unsigned_integer(const Type* type) const;
        bool is_integer(const Type* type) const;
        bool is_floating(const Type* type) const;
        bool is_complex(const Type* type) const;
        bool is_builtin_numeric(const Type* type) const;
        bool can_implicitly_convert(const Type* from, const Type* to) const;
    };

} // namespace xenon::semantic
