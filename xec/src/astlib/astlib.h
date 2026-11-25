#pragma once

#include "common/source/span.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace xenon::ast {

    enum class Kind {
        Path, DeclName, DocComment, Attribute, AttributeList,
        BuiltinType, NamedType, RefType, PtrType, ArrayType, FuncType, AutoType,
        IntLit, FloatLit, StrLit, RuneLit, BoolLit, NullptrLit, NameRef, SelfExpr,
        ParenExpr, UnaryExpr, BinaryExpr, CondExpr, CallExpr, FieldExpr, IndexExpr,
        StructLit, ArrayLit, SizeofExpr, AlignofExpr, CastExpr, RefExpr, PtrExpr,
        Block, ExprStmt, VarDecl, AssignStmt, IncDecStmt, ReturnStmt, BreakStmt,
        ContinueStmt, IfStmt, WhileStmt, DoWhileStmt, DoStmt, CtIfStmt,
        SourceFile, ModuleDecl, ImportDecl, TypeDecl, FieldDecl, ImplDecl, FuncDecl,
        Param, SelfParam, MethodDecl, CtIfItem, CtIfField, CtIfMember, ErrorExpr, ErrorStmt,
        ErrorItem, ErrorType
    };

    enum class PathSegmentKind { Identifier, TypeName, CompileTimeName };
    enum class DeclNameKind { Identifier, CompileTimeName, Wildcard };
    enum class BuiltinTypeKind {
        Str, U8, U16, U32, U64, Usize, I8, I16, I32, I64, Isize,
        F32, F64, Bool, Rune, Char
    };
    enum class NumericBase { Decimal, Hexadecimal, Binary, Octal };
    enum class UnaryOp { Negate, LogicalNot, BitwiseNot };
    enum class BinaryOp {
        Add, Subtract, Multiply, Divide, Power, ShiftLeft, ShiftRight,
        Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
        BitwiseAnd, BitwiseOr, BitwiseXor, LogicalAnd, LogicalOr
    };
    enum class AssignOp {
        Assign, Add, Subtract, Multiply, Divide, Power, BitwiseAnd,
        BitwiseOr, BitwiseXor, ShiftLeft, ShiftRight
    };
    enum class IncDecKind { Increment, Decrement };
    enum class StorageKind { Let, Static, Const };
    enum class SelfParamKind { Value, Ref, RefMut };
    enum class CastKind { Cast, Bitcast, Saturate, Truncate };
    enum class RefPtrKind { Ref, Ptr };

    [[nodiscard]] constexpr std::string_view to_string(Kind value) noexcept {
        switch (value) {
            case Kind::Path: return "Path";
            case Kind::DeclName: return "DeclName";
            case Kind::DocComment: return "DocComment";
            case Kind::Attribute: return "Attribute";
            case Kind::AttributeList: return "AttributeList";
            case Kind::BuiltinType: return "BuiltinType";
            case Kind::NamedType: return "NamedType";
            case Kind::RefType: return "RefType";
            case Kind::PtrType: return "PtrType";
            case Kind::ArrayType: return "ArrayType";
            case Kind::FuncType: return "FuncType";
            case Kind::AutoType: return "AutoType";
            case Kind::IntLit: return "IntLit";
            case Kind::FloatLit: return "FloatLit";
            case Kind::StrLit: return "StrLit";
            case Kind::RuneLit: return "RuneLit";
            case Kind::BoolLit: return "BoolLit";
            case Kind::NullptrLit: return "NullptrLit";
            case Kind::NameRef: return "NameRef";
            case Kind::SelfExpr: return "SelfExpr";
            case Kind::ParenExpr: return "ParenExpr";
            case Kind::UnaryExpr: return "UnaryExpr";
            case Kind::BinaryExpr: return "BinaryExpr";
            case Kind::CondExpr: return "CondExpr";
            case Kind::CallExpr: return "CallExpr";
            case Kind::FieldExpr: return "FieldExpr";
            case Kind::IndexExpr: return "IndexExpr";
            case Kind::StructLit: return "StructLit";
            case Kind::ArrayLit: return "ArrayLit";
            case Kind::SizeofExpr: return "SizeofExpr";
            case Kind::AlignofExpr: return "AlignofExpr";
            case Kind::CastExpr: return "CastExpr";
            case Kind::RefExpr: return "RefExpr";
            case Kind::PtrExpr: return "PtrExpr";
            case Kind::Block: return "Block";
            case Kind::ExprStmt: return "ExprStmt";
            case Kind::VarDecl: return "VarDecl";
            case Kind::AssignStmt: return "AssignStmt";
            case Kind::IncDecStmt: return "IncDecStmt";
            case Kind::ReturnStmt: return "ReturnStmt";
            case Kind::BreakStmt: return "BreakStmt";
            case Kind::ContinueStmt: return "ContinueStmt";
            case Kind::IfStmt: return "IfStmt";
            case Kind::WhileStmt: return "WhileStmt";
            case Kind::DoWhileStmt: return "DoWhileStmt";
            case Kind::DoStmt: return "DoStmt";
            case Kind::CtIfStmt: return "CtIfStmt";
            case Kind::SourceFile: return "SourceFile";
            case Kind::ModuleDecl: return "ModuleDecl";
            case Kind::ImportDecl: return "ImportDecl";
            case Kind::TypeDecl: return "TypeDecl";
            case Kind::FieldDecl: return "FieldDecl";
            case Kind::ImplDecl: return "ImplDecl";
            case Kind::FuncDecl: return "FuncDecl";
            case Kind::Param: return "Param";
            case Kind::SelfParam: return "SelfParam";
            case Kind::MethodDecl: return "MethodDecl";
            case Kind::CtIfItem: return "CtIfItem";
            case Kind::CtIfField: return "CtIfField";
            case Kind::CtIfMember: return "CtIfMember";
            case Kind::ErrorExpr: return "ErrorExpr";
            case Kind::ErrorStmt: return "ErrorStmt";
            case Kind::ErrorItem: return "ErrorItem";
            case Kind::ErrorType: return "ErrorType";
        }
        return "<invalid Kind>";
    }

#define XENON_AST_ENUM_TO_STRING(EnumType, ...) \
    [[nodiscard]] constexpr std::string_view to_string(EnumType value) noexcept { \
        switch (value) { __VA_ARGS__ } \
        return "<invalid " #EnumType ">"; \
    }

    XENON_AST_ENUM_TO_STRING(PathSegmentKind,
        case PathSegmentKind::Identifier: return "identifier";
        case PathSegmentKind::TypeName: return "type-name";
        case PathSegmentKind::CompileTimeName: return "compile-time-name";)
    XENON_AST_ENUM_TO_STRING(DeclNameKind,
        case DeclNameKind::Identifier: return "identifier";
        case DeclNameKind::CompileTimeName: return "compile-time-name";
        case DeclNameKind::Wildcard: return "wildcard";)
    XENON_AST_ENUM_TO_STRING(BuiltinTypeKind,
        case BuiltinTypeKind::Str: return "str";
        case BuiltinTypeKind::U8: return "u8";
        case BuiltinTypeKind::U16: return "u16";
        case BuiltinTypeKind::U32: return "u32";
        case BuiltinTypeKind::U64: return "u64";
        case BuiltinTypeKind::Usize: return "usize";
        case BuiltinTypeKind::I8: return "i8";
        case BuiltinTypeKind::I16: return "i16";
        case BuiltinTypeKind::I32: return "i32";
        case BuiltinTypeKind::I64: return "i64";
        case BuiltinTypeKind::Isize: return "isize";
        case BuiltinTypeKind::F32: return "f32";
        case BuiltinTypeKind::F64: return "f64";
        case BuiltinTypeKind::Bool: return "bool";
        case BuiltinTypeKind::Rune: return "rune";
        case BuiltinTypeKind::Char: return "char";)
    XENON_AST_ENUM_TO_STRING(NumericBase,
        case NumericBase::Decimal: return "decimal";
        case NumericBase::Hexadecimal: return "hexadecimal";
        case NumericBase::Binary: return "binary";
        case NumericBase::Octal: return "octal";)
    XENON_AST_ENUM_TO_STRING(UnaryOp,
        case UnaryOp::Negate: return "-";
        case UnaryOp::LogicalNot: return "!";
        case UnaryOp::BitwiseNot: return "~";)
    XENON_AST_ENUM_TO_STRING(BinaryOp,
        case BinaryOp::Add: return "+";
        case BinaryOp::Subtract: return "-";
        case BinaryOp::Multiply: return "*";
        case BinaryOp::Divide: return "/";
        case BinaryOp::Power: return "**";
        case BinaryOp::ShiftLeft: return "<<";
        case BinaryOp::ShiftRight: return ">>";
        case BinaryOp::Equal: return "==";
        case BinaryOp::NotEqual: return "!=";
        case BinaryOp::Less: return "<";
        case BinaryOp::LessEqual: return "<=";
        case BinaryOp::Greater: return ">";
        case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::BitwiseAnd: return "&";
        case BinaryOp::BitwiseOr: return "|";
        case BinaryOp::BitwiseXor: return "^";
        case BinaryOp::LogicalAnd: return "&&";
        case BinaryOp::LogicalOr: return "||";)
    XENON_AST_ENUM_TO_STRING(AssignOp,
        case AssignOp::Assign: return "=";
        case AssignOp::Add: return "+=";
        case AssignOp::Subtract: return "-=";
        case AssignOp::Multiply: return "*=";
        case AssignOp::Divide: return "/=";
        case AssignOp::Power: return "**=";
        case AssignOp::BitwiseAnd: return "&=";
        case AssignOp::BitwiseOr: return "|=";
        case AssignOp::BitwiseXor: return "^=";
        case AssignOp::ShiftLeft: return "<<=";
        case AssignOp::ShiftRight: return ">>=";)
    XENON_AST_ENUM_TO_STRING(IncDecKind,
        case IncDecKind::Increment: return "++";
        case IncDecKind::Decrement: return "--";)
    XENON_AST_ENUM_TO_STRING(StorageKind,
        case StorageKind::Let: return "let";
        case StorageKind::Static: return "static";
        case StorageKind::Const: return "const";)
    XENON_AST_ENUM_TO_STRING(SelfParamKind,
        case SelfParamKind::Value: return "self";
        case SelfParamKind::Ref: return "&self";
        case SelfParamKind::RefMut: return "&mut self";)
    XENON_AST_ENUM_TO_STRING(CastKind,
        case CastKind::Cast: return "$cast";
        case CastKind::Bitcast: return "$bitcast";
        case CastKind::Saturate: return "$saturate";
        case CastKind::Truncate: return "$truncate";)
    XENON_AST_ENUM_TO_STRING(RefPtrKind,
        case RefPtrKind::Ref: return "$ref";
        case RefPtrKind::Ptr: return "$ptr";)
#undef XENON_AST_ENUM_TO_STRING

    struct Node {
        const Kind kind;
        const source::Span span;
        Node(Kind node_kind, source::Span node_span) noexcept : kind(node_kind), span(node_span) {}
        virtual ~Node() = default;
    };

    using NodePtr = std::unique_ptr<Node>;
    using ExpressionPtr = NodePtr;
    using TypePtr = NodePtr;
    using StatementPtr = NodePtr;
    using ItemPtr = NodePtr;

    struct PathSegment {
        PathSegmentKind kind;
        source::Span span;
    };

    struct Path final : Node {
        std::vector<PathSegment> segments;
        Path(source::Span span, std::vector<PathSegment> parts) : Node(Kind::Path, span), segments(std::move(parts)) {}
    };
    using PathPtr = std::unique_ptr<Path>;

    struct DeclName final : Node {
        DeclNameKind name_kind;
        DeclName(source::Span span, DeclNameKind name_type) : Node(Kind::DeclName, span), name_kind(name_type) {}
    };
    using DeclNamePtr = std::unique_ptr<DeclName>;

    struct DocComment final : Node {
        DocComment(source::Span span) : Node(Kind::DocComment, span) {}
    };
    using DocCommentPtr = std::unique_ptr<DocComment>;
    using DocComments = std::vector<DocCommentPtr>;

    struct Attribute final : Node {
        source::Span name;
        std::vector<ExpressionPtr> arguments;
        Attribute(source::Span span, source::Span attribute_name, std::vector<ExpressionPtr> args = {})
            : Node(Kind::Attribute, span), name(attribute_name), arguments(std::move(args)) {}
    };
    using AttributePtr = std::unique_ptr<Attribute>;

    struct AttributeList final : Node {
        std::vector<AttributePtr> attributes;
        AttributeList(source::Span span, std::vector<AttributePtr> values = {})
            : Node(Kind::AttributeList, span), attributes(std::move(values)) {}
    };
    using AttributeListPtr = std::unique_ptr<AttributeList>;
    using AttributeLists = std::vector<AttributeListPtr>;

    struct BuiltinType final : Node {
        BuiltinTypeKind type;
        BuiltinType(source::Span span, BuiltinTypeKind value) : Node(Kind::BuiltinType, span), type(value) {}
    };
    struct NamedType final : Node {
        PathPtr path;
        NamedType(source::Span span, PathPtr value) : Node(Kind::NamedType, span), path(std::move(value)) {}
    };
    struct RefType final : Node {
        bool is_mut;
        TypePtr inner;
        RefType(source::Span span, bool mut, TypePtr value) : Node(Kind::RefType, span), is_mut(mut), inner(std::move(value)) {}
    };
    struct PtrType final : Node {
        bool is_mut;
        TypePtr inner;
        PtrType(source::Span span, bool mut, TypePtr value) : Node(Kind::PtrType, span), is_mut(mut), inner(std::move(value)) {}
    };
    struct ArrayType final : Node {
        ExpressionPtr size;
        TypePtr element;
        ArrayType(source::Span span, ExpressionPtr size_expr, TypePtr element_type)
            : Node(Kind::ArrayType, span), size(std::move(size_expr)), element(std::move(element_type)) {}
    };
    struct FuncType final : Node {
        std::vector<TypePtr> parameters;
        TypePtr return_type;
        FuncType(source::Span span, std::vector<TypePtr> params = {}, TypePtr result = nullptr)
            : Node(Kind::FuncType, span), parameters(std::move(params)), return_type(std::move(result)) {}
    };
    struct AutoType final : Node { AutoType(source::Span span) : Node(Kind::AutoType, span) {} };

    struct IntLit final : Node {
        NumericBase base;
        source::Span digits;
        std::optional<source::Span> suffix;
        bool unsuffixed;
        IntLit(source::Span span, NumericBase radix, source::Span digit_text,
               std::optional<source::Span> suffix_text, bool is_unsuffixed)
            : Node(Kind::IntLit, span), base(radix), digits(digit_text), suffix(suffix_text), unsuffixed(is_unsuffixed) {}
    };
    struct FloatLit final : Node {
        NumericBase base;
        source::Span digits;
        std::optional<source::Span> suffix;
        bool unsuffixed;
        FloatLit(source::Span span, NumericBase radix, source::Span digit_text,
                 std::optional<source::Span> suffix_text, bool is_unsuffixed)
            : Node(Kind::FloatLit, span), base(radix), digits(digit_text), suffix(suffix_text), unsuffixed(is_unsuffixed) {}
    };
    struct StrLit final : Node {
        source::Span text;
        StrLit(source::Span span, source::Span literal_text) : Node(Kind::StrLit, span), text(literal_text) {}
    };
    struct RuneLit final : Node {
        source::Span text;
        RuneLit(source::Span span, source::Span literal_text) : Node(Kind::RuneLit, span), text(literal_text) {}
    };
    struct BoolLit final : Node {
        bool value;
        BoolLit(source::Span span, bool literal_value) : Node(Kind::BoolLit, span), value(literal_value) {}
    };
    struct NullptrLit final : Node { NullptrLit(source::Span span) : Node(Kind::NullptrLit, span) {} };
    struct NameRef final : Node {
        PathPtr path;
        NameRef(source::Span span, PathPtr value) : Node(Kind::NameRef, span), path(std::move(value)) {}
    };
    struct SelfExpr final : Node { SelfExpr(source::Span span) : Node(Kind::SelfExpr, span) {} };
    struct ParenExpr final : Node {
        ExpressionPtr expression;
        ParenExpr(source::Span span, ExpressionPtr value) : Node(Kind::ParenExpr, span), expression(std::move(value)) {}
    };
    struct UnaryExpr final : Node {
        UnaryOp op;
        ExpressionPtr operand;
        UnaryExpr(source::Span span, UnaryOp operation, ExpressionPtr value)
            : Node(Kind::UnaryExpr, span), op(operation), operand(std::move(value)) {}
    };
    struct BinaryExpr final : Node {
        BinaryOp op;
        ExpressionPtr left;
        ExpressionPtr right;
        BinaryExpr(source::Span span, BinaryOp operation, ExpressionPtr lhs, ExpressionPtr rhs)
            : Node(Kind::BinaryExpr, span), op(operation), left(std::move(lhs)), right(std::move(rhs)) {}
    };
    struct CondExpr final : Node {
        ExpressionPtr condition;
        ExpressionPtr then_expr;
        ExpressionPtr else_expr;
        CondExpr(source::Span span, ExpressionPtr cond, ExpressionPtr then_value, ExpressionPtr else_value)
            : Node(Kind::CondExpr, span), condition(std::move(cond)), then_expr(std::move(then_value)), else_expr(std::move(else_value)) {}
    };
    struct CallExpr final : Node {
        ExpressionPtr callee;
        std::vector<ExpressionPtr> arguments;
        CallExpr(source::Span span, ExpressionPtr target, std::vector<ExpressionPtr> args = {})
            : Node(Kind::CallExpr, span), callee(std::move(target)), arguments(std::move(args)) {}
    };
    struct FieldExpr final : Node {
        ExpressionPtr base;
        source::Span field;
        FieldExpr(source::Span span, ExpressionPtr object, source::Span field_name)
            : Node(Kind::FieldExpr, span), base(std::move(object)), field(field_name) {}
    };
    struct IndexExpr final : Node {
        ExpressionPtr base;
        ExpressionPtr index;
        IndexExpr(source::Span span, ExpressionPtr object, ExpressionPtr subscript)
            : Node(Kind::IndexExpr, span), base(std::move(object)), index(std::move(subscript)) {}
    };
    struct FieldInit {
        source::Span name;
        ExpressionPtr value;
    };
    struct StructLit final : Node {
        PathPtr type_path;
        std::vector<FieldInit> fields;
        StructLit(source::Span span, PathPtr path, std::vector<FieldInit> initializers = {})
            : Node(Kind::StructLit, span), type_path(std::move(path)), fields(std::move(initializers)) {}
    };
    struct ArrayLit final : Node {
        TypePtr array_type;
        std::vector<ExpressionPtr> elements;
        ArrayLit(source::Span span, TypePtr type, std::vector<ExpressionPtr> values = {})
            : Node(Kind::ArrayLit, span), array_type(std::move(type)), elements(std::move(values)) {}
    };
    struct SizeofExpr final : Node {
        TypePtr type;
        SizeofExpr(source::Span span, TypePtr value) : Node(Kind::SizeofExpr, span), type(std::move(value)) {}
    };
    struct AlignofExpr final : Node {
        TypePtr type;
        AlignofExpr(source::Span span, TypePtr value) : Node(Kind::AlignofExpr, span), type(std::move(value)) {}
    };
    struct CastExpr final : Node {
        CastKind cast;
        TypePtr type;
        ExpressionPtr expression;
        CastExpr(source::Span span, CastKind operation, TypePtr target, ExpressionPtr value)
            : Node(Kind::CastExpr, span), cast(operation), type(std::move(target)), expression(std::move(value)) {}
    };
    struct RefExpr final : Node {
        RefPtrKind intrinsic;
        bool is_mut;
        ExpressionPtr place;
        RefExpr(source::Span span, RefPtrKind operation, bool mut, ExpressionPtr value)
            : Node(Kind::RefExpr, span), intrinsic(operation), is_mut(mut), place(std::move(value)) {}
    };
    struct PtrExpr final : Node {
        RefPtrKind intrinsic;
        bool is_mut;
        ExpressionPtr place;
        PtrExpr(source::Span span, RefPtrKind operation, bool mut, ExpressionPtr value)
            : Node(Kind::PtrExpr, span), intrinsic(operation), is_mut(mut), place(std::move(value)) {}
    };

    struct Block final : Node {
        std::vector<StatementPtr> statements;
        Block(source::Span span, std::vector<StatementPtr> values = {})
            : Node(Kind::Block, span), statements(std::move(values)) {}
    };
    struct ExprStmt final : Node {
        ExpressionPtr expression;
        ExprStmt(source::Span span, ExpressionPtr value) : Node(Kind::ExprStmt, span), expression(std::move(value)) {}
    };
    struct VarDecl final : Node {
        StorageKind storage;
        bool is_mut;
        TypePtr type;
        DeclNamePtr name;
        ExpressionPtr initializer;
        DocComments doc_comments;
        AttributeLists attributes;
        VarDecl(source::Span span, StorageKind storage_kind, bool mut, TypePtr declared_type,
            DeclNamePtr declared_name, ExpressionPtr init = nullptr,
            DocComments docs = {}, AttributeLists attrs = {})
            : Node(Kind::VarDecl, span), storage(storage_kind), is_mut(mut), type(std::move(declared_type)),
              name(std::move(declared_name)), initializer(std::move(init)),
              doc_comments(std::move(docs)), attributes(std::move(attrs)) {}
    };
    struct AssignStmt final : Node {
        AssignOp op;
        ExpressionPtr target;
        ExpressionPtr value;
        AssignStmt(source::Span span, AssignOp operation, ExpressionPtr place, ExpressionPtr rhs)
            : Node(Kind::AssignStmt, span), op(operation), target(std::move(place)), value(std::move(rhs)) {}
    };
    struct IncDecStmt final : Node {
        IncDecKind op;
        ExpressionPtr target;
        IncDecStmt(source::Span span, IncDecKind operation, ExpressionPtr place)
            : Node(Kind::IncDecStmt, span), op(operation), target(std::move(place)) {}
    };
    struct ReturnStmt final : Node {
        ExpressionPtr expression;
        ReturnStmt(source::Span span, ExpressionPtr value = nullptr)
            : Node(Kind::ReturnStmt, span), expression(std::move(value)) {}
    };
    struct BreakStmt final : Node { BreakStmt(source::Span span) : Node(Kind::BreakStmt, span) {} };
    struct ContinueStmt final : Node { ContinueStmt(source::Span span) : Node(Kind::ContinueStmt, span) {} };
    struct IfStmt final : Node {
        ExpressionPtr condition;
        std::unique_ptr<Block> then_block;
        NodePtr else_branch;
        IfStmt(source::Span span, ExpressionPtr cond, std::unique_ptr<Block> then_body, NodePtr otherwise = nullptr)
            : Node(Kind::IfStmt, span), condition(std::move(cond)), then_block(std::move(then_body)), else_branch(std::move(otherwise)) {}
    };
    struct WhileStmt final : Node {
        ExpressionPtr condition;
        std::unique_ptr<Block> body;
        WhileStmt(source::Span span, ExpressionPtr cond, std::unique_ptr<Block> loop_body)
            : Node(Kind::WhileStmt, span), condition(std::move(cond)), body(std::move(loop_body)) {}
    };
    struct DoWhileStmt final : Node {
        std::unique_ptr<Block> body;
        ExpressionPtr condition;
        DoWhileStmt(source::Span span, std::unique_ptr<Block> loop_body, ExpressionPtr cond)
            : Node(Kind::DoWhileStmt, span), body(std::move(loop_body)), condition(std::move(cond)) {}
    };
    struct DoStmt final : Node {
        TypePtr type;
        DeclNamePtr name;
        ExpressionPtr range;
        std::unique_ptr<Block> body;
        DoStmt(source::Span span, TypePtr declared_type, DeclNamePtr variable,
               ExpressionPtr range_expr, std::unique_ptr<Block> loop_body)
            : Node(Kind::DoStmt, span), type(std::move(declared_type)), name(std::move(variable)),
              range(std::move(range_expr)), body(std::move(loop_body)) {}
    };
    struct CtIfBranch {
        ExpressionPtr condition;
        std::vector<NodePtr> body;
    };
    struct CtIfStmt final : Node {
        std::vector<CtIfBranch> branches;
        std::vector<StatementPtr> else_body;
        CtIfStmt(source::Span span, std::vector<CtIfBranch> choices = {}, std::vector<StatementPtr> otherwise = {})
            : Node(Kind::CtIfStmt, span), branches(std::move(choices)), else_body(std::move(otherwise)) {}
    };

    struct SourceFile final : Node {
        NodePtr module;
        std::vector<NodePtr> imports;
        std::vector<ItemPtr> items;
        SourceFile(source::Span span, NodePtr module_decl = nullptr,
                   std::vector<NodePtr> import_decls = {}, std::vector<ItemPtr> file_items = {})
            : Node(Kind::SourceFile, span), module(std::move(module_decl)), imports(std::move(import_decls)), items(std::move(file_items)) {}
    };
    struct ModuleDecl final : Node {
        PathPtr path;
        DocComments doc_comments;
        ModuleDecl(source::Span span, PathPtr module_path, DocComments docs = {})
            : Node(Kind::ModuleDecl, span), path(std::move(module_path)), doc_comments(std::move(docs)) {}
    };
    struct ImportDecl final : Node {
        PathPtr path;
        DocComments doc_comments;
        ImportDecl(source::Span span, PathPtr imported_path, DocComments docs = {})
            : Node(Kind::ImportDecl, span), path(std::move(imported_path)), doc_comments(std::move(docs)) {}
    };
    struct TypeDecl final : Node {
        bool is_public;
        source::Span name;
        DocComments doc_comments;
        AttributeLists attributes;
        std::vector<NodePtr> members;
        TypeDecl(source::Span span, bool is_pub, source::Span type_name, DocComments docs = {},
                 AttributeLists attrs = {}, std::vector<NodePtr> fields = {})
            : Node(Kind::TypeDecl, span), is_public(is_pub), name(type_name), doc_comments(std::move(docs)),
              attributes(std::move(attrs)), members(std::move(fields)) {}
    };
    struct FieldDecl final : Node {
        bool is_public;
        TypePtr type;
        source::Span name;
        DocComments doc_comments;
        AttributeLists attributes;
        FieldDecl(source::Span span, bool is_pub, TypePtr field_type, source::Span field_name,
                  DocComments docs = {}, AttributeLists attrs = {})
            : Node(Kind::FieldDecl, span), is_public(is_pub), type(std::move(field_type)), name(field_name),
              doc_comments(std::move(docs)), attributes(std::move(attrs)) {}
    };
    struct ImplDecl final : Node {
        PathPtr type_path;
        std::vector<NodePtr> members;
                DocComments doc_comments;
                AttributeLists attributes;
                ImplDecl(source::Span span, PathPtr path, std::vector<NodePtr> impl_members = {},
                                 DocComments docs = {}, AttributeLists attrs = {})
                        : Node(Kind::ImplDecl, span), type_path(std::move(path)), members(std::move(impl_members)),
                            doc_comments(std::move(docs)), attributes(std::move(attrs)) {}
    };
    struct FuncDecl final : Node {
        bool is_public;
        source::Span name;
        NodePtr self_param;
        std::vector<NodePtr> parameters;
        TypePtr return_type;
        std::unique_ptr<Block> body;
        DocComments doc_comments;
        AttributeLists attributes;
        FuncDecl(source::Span span, bool is_pub, source::Span function_name,
                 NodePtr receiver = nullptr, std::vector<NodePtr> params = {},
                 TypePtr result = nullptr, std::unique_ptr<Block> function_body = nullptr,
                 DocComments docs = {}, AttributeLists attrs = {})
            : Node(Kind::FuncDecl, span), is_public(is_pub), name(function_name), self_param(std::move(receiver)),
              parameters(std::move(params)), return_type(std::move(result)), body(std::move(function_body)),
              doc_comments(std::move(docs)), attributes(std::move(attrs)) {}
    };
    struct Param final : Node {
        TypePtr type;
        DeclNamePtr name;
        Param(source::Span span, TypePtr param_type, DeclNamePtr param_name)
            : Node(Kind::Param, span), type(std::move(param_type)), name(std::move(param_name)) {}
    };
    struct MethodDecl final : Node {
        bool is_public;
        source::Span name;
        NodePtr self_param;
        std::vector<NodePtr> parameters;
        TypePtr return_type;
        std::unique_ptr<Block> body;
        DocComments doc_comments;
        AttributeLists attributes;
        MethodDecl(source::Span span, bool is_pub, source::Span method_name,
                   NodePtr receiver = nullptr, std::vector<NodePtr> params = {},
                   TypePtr result = nullptr, std::unique_ptr<Block> method_body = nullptr,
                   DocComments docs = {}, AttributeLists attrs = {})
            : Node(Kind::MethodDecl, span), is_public(is_pub), name(method_name),
              self_param(std::move(receiver)), parameters(std::move(params)),
              return_type(std::move(result)), body(std::move(method_body)),
              doc_comments(std::move(docs)), attributes(std::move(attrs)) {}
    };
    struct SelfParam final : Node {
        SelfParamKind form;
        SelfParam(source::Span span, SelfParamKind receiver_form) : Node(Kind::SelfParam, span), form(receiver_form) {}
    };
    struct CtIfItem final : Node {
        std::vector<CtIfBranch> branches;
        std::vector<ItemPtr> else_body;
                DocComments doc_comments;
                AttributeLists attributes;
                CtIfItem(source::Span span, std::vector<CtIfBranch> choices = {},
                                 std::vector<ItemPtr> otherwise = {}, DocComments docs = {}, AttributeLists attrs = {})
                        : Node(Kind::CtIfItem, span), branches(std::move(choices)), else_body(std::move(otherwise)),
                            doc_comments(std::move(docs)), attributes(std::move(attrs)) {}
    };
    struct CtIfField final : Node {
        std::vector<CtIfBranch> branches;
        std::vector<NodePtr> else_body;
        CtIfField(source::Span span, std::vector<CtIfBranch> choices = {}, std::vector<NodePtr> otherwise = {})
            : Node(Kind::CtIfField, span), branches(std::move(choices)), else_body(std::move(otherwise)) {}
    };
    struct CtIfMember final : Node {
        std::vector<CtIfBranch> branches;
        std::vector<NodePtr> else_body;
        CtIfMember(source::Span span, std::vector<CtIfBranch> choices = {}, std::vector<NodePtr> otherwise = {})
            : Node(Kind::CtIfMember, span), branches(std::move(choices)), else_body(std::move(otherwise)) {}
    };

    struct ErrorExpr final : Node { ErrorExpr(source::Span span) : Node(Kind::ErrorExpr, span) {} };
    struct ErrorStmt final : Node { ErrorStmt(source::Span span) : Node(Kind::ErrorStmt, span) {} };
    struct ErrorItem final : Node { ErrorItem(source::Span span) : Node(Kind::ErrorItem, span) {} };
    struct ErrorType final : Node { ErrorType(source::Span span) : Node(Kind::ErrorType, span) {} };

#define XENON_AST_FACTORY(Type) \
    using Type##Ptr = std::unique_ptr<Type>; \
    template<typename... Args> \
    [[nodiscard]] inline Type##Ptr make_##Type(Args&&... args) { \
        return std::make_unique<Type>(std::forward<Args>(args)...); \
    }

#define XENON_AST_FACTORY_WITH_ALIAS(Type) \
    template<typename... Args> \
    [[nodiscard]] inline Type##Ptr make_##Type(Args&&... args) { \
        return std::make_unique<Type>(std::forward<Args>(args)...); \
    }

    XENON_AST_FACTORY_WITH_ALIAS(Path)
    XENON_AST_FACTORY_WITH_ALIAS(DeclName)
    XENON_AST_FACTORY_WITH_ALIAS(DocComment)
    XENON_AST_FACTORY_WITH_ALIAS(Attribute)
    XENON_AST_FACTORY_WITH_ALIAS(AttributeList)
    XENON_AST_FACTORY(BuiltinType)
    XENON_AST_FACTORY(NamedType)
    XENON_AST_FACTORY(RefType)
    XENON_AST_FACTORY(PtrType)
    XENON_AST_FACTORY(ArrayType)
    XENON_AST_FACTORY(FuncType)
    XENON_AST_FACTORY(AutoType)
    XENON_AST_FACTORY(IntLit)
    XENON_AST_FACTORY(FloatLit)
    XENON_AST_FACTORY(StrLit)
    XENON_AST_FACTORY(RuneLit)
    XENON_AST_FACTORY(BoolLit)
    XENON_AST_FACTORY(NullptrLit)
    XENON_AST_FACTORY(NameRef)
    XENON_AST_FACTORY(SelfExpr)
    XENON_AST_FACTORY(ParenExpr)
    XENON_AST_FACTORY(UnaryExpr)
    XENON_AST_FACTORY(BinaryExpr)
    XENON_AST_FACTORY(CondExpr)
    XENON_AST_FACTORY(CallExpr)
    XENON_AST_FACTORY(FieldExpr)
    XENON_AST_FACTORY(IndexExpr)
    XENON_AST_FACTORY(StructLit)
    XENON_AST_FACTORY(ArrayLit)
    XENON_AST_FACTORY(SizeofExpr)
    XENON_AST_FACTORY(AlignofExpr)
    XENON_AST_FACTORY(CastExpr)
    XENON_AST_FACTORY(RefExpr)
    XENON_AST_FACTORY(PtrExpr)
    XENON_AST_FACTORY(Block)
    XENON_AST_FACTORY(ExprStmt)
    XENON_AST_FACTORY(VarDecl)
    XENON_AST_FACTORY(AssignStmt)
    XENON_AST_FACTORY(IncDecStmt)
    XENON_AST_FACTORY(ReturnStmt)
    XENON_AST_FACTORY(BreakStmt)
    XENON_AST_FACTORY(ContinueStmt)
    XENON_AST_FACTORY(IfStmt)
    XENON_AST_FACTORY(WhileStmt)
    XENON_AST_FACTORY(DoWhileStmt)
    XENON_AST_FACTORY(DoStmt)
    XENON_AST_FACTORY(CtIfStmt)
    XENON_AST_FACTORY(SourceFile)
    XENON_AST_FACTORY(ModuleDecl)
    XENON_AST_FACTORY(ImportDecl)
    XENON_AST_FACTORY(TypeDecl)
    XENON_AST_FACTORY(FieldDecl)
    XENON_AST_FACTORY(ImplDecl)
    XENON_AST_FACTORY(FuncDecl)
    XENON_AST_FACTORY(Param)
    XENON_AST_FACTORY(SelfParam)
    XENON_AST_FACTORY(MethodDecl)
    XENON_AST_FACTORY(CtIfItem)
    XENON_AST_FACTORY(CtIfField)
    XENON_AST_FACTORY(CtIfMember)
    XENON_AST_FACTORY(ErrorExpr)
    XENON_AST_FACTORY(ErrorStmt)
    XENON_AST_FACTORY(ErrorItem)
    XENON_AST_FACTORY(ErrorType)
#undef XENON_AST_FACTORY
#undef XENON_AST_FACTORY_WITH_ALIAS

} // namespace xenon::ast