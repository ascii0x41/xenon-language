#pragma once

#include "common/dataclasses.h"
#include "ast/astlib.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace xenon::validation {

    using ASTNodeViewPtr = const ASTNode*;
    
    struct TypeInfo {
        enum class Kind { BUILTIN, CALLABLE, USER_DEFINED, TRAIT } kind;
        explicit TypeInfo(Kind k) : kind(k) {}
        virtual ~TypeInfo() = default;
    };

    struct BuiltinTypeInfo : public TypeInfo {
        enum class BuiltinKind {
            I8,
            I16,
            I32,
            I64,
            U8,
            U16,
            U32,
            U64,
            F32,
            F64,
            BOOL,
            NULL_T,
            ERROR
        } b_kind;
        explicit BuiltinTypeInfo(BuiltinKind bk) 
            : TypeInfo(Kind::BUILTIN), b_kind(bk) {}
    };

    struct CallableTypeInfo : public TypeInfo {
        std::shared_ptr<TypeInfo> return_type;
        std::vector<std::shared_ptr<TypeInfo>> param_types;
    };

    struct UserDefinedTypeInfo : public TypeInfo {
        std::string fully_qualified_name;
        struct FieldInfo {
            std::string fully_qualified_name;
            std::shared_ptr<TypeInfo> type;
            bool is_public;
            bool is_mut;

        };
        struct MethodInfo {
            std::string fully_qualified_name;
            std::shared_ptr<TypeInfo> return_type;
            std::vector<std::shared_ptr<TypeInfo>> param_types;
            bool is_mut;
            bool is_public;
        };

        std::vector<FieldInfo> fields;
        std::vector<MethodInfo> methods;
        std::vector<std::string> implemented_traits;

        ASTNodeViewPtr decl_node; // for error reporting
    };

    struct TraitInfo {
        std::string fully_qualified_name;
        struct MethodReq {
            std::string fully_qualified_name;
            std::shared_ptr<TypeInfo> return_type;
            std::vector<std::shared_ptr<TypeInfo>> param_types;
        };
        std::vector<MethodReq> method_requirements;

        ASTNodeViewPtr decl_node; // for error reporting
    };

    struct RuntimeObject {
        std::shared_ptr<TypeInfo> type;
        bool is_dangling; // for dangling reference warnings

        // Value Semantics Tracker
        enum class Mutability { CONST, MUT } mutability;
        enum class ValueCategory { LVALUE, RVALUE } category;
        enum class StorageKind { VALUE, RAW_PTR, REF, STATIC } storage;        
        ASTNodeViewPtr decl_node; // for error reporting
    };

    struct ExportTable {
        // Flat mapping of unrolled strings to semantic objects
        std::unordered_map<std::string, std::shared_ptr<UserDefinedTypeInfo>> types;
        std::unordered_map<std::string, std::shared_ptr<TraitInfo>> traits;
        std::unordered_map<std::string, std::shared_ptr<CallableTypeInfo>> global_functions;
    };    

} // namespace xenon::validation