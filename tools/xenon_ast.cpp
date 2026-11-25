#include "astlib/astlib.h"
#include "common/source/source_file_manager.h"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {

    using namespace xenon;
    using namespace xenon::ast;

    int failures = 0;

    template<typename T>
    void check_node(std::unique_ptr<T> node, Kind expected_kind, source::Span expected_span) {
        static_assert(std::is_base_of_v<Node, T>);
        if (!node || node->kind != expected_kind || node->span.file != expected_span.file ||
            node->span.start != expected_span.start || node->span.end != expected_span.end) {
            std::cerr << "node check failed for " << to_string(expected_kind) << '\n';
            ++failures;
        }
    }

    PathPtr path(source::Span span) {
        return make_Path(span, std::vector<PathSegment>{{PathSegmentKind::Identifier, span}});
    }

    DeclNamePtr decl_name(source::Span span) {
        return make_DeclName(span, DeclNameKind::Identifier);
    }

    ExpressionPtr expression(source::Span span) {
        return make_BoolLit(span, true);
    }

    TypePtr type(source::Span span) {
        return make_AutoType(span);
    }

#define CHECK_NODE(expression, NodeType) \
    check_node((expression), Kind::NodeType, span)

    void check_stringifiers() {
        if (to_string(Kind::SourceFile) != "SourceFile" ||
            to_string(UnaryOp::Negate) != "-" ||
            to_string(BinaryOp::Power) != "**" ||
            to_string(AssignOp::Power) != "**=" ||
            to_string(StorageKind::Const) != "const" ||
            to_string(BuiltinTypeKind::Rune) != "rune" ||
            to_string(CastKind::Bitcast) != "$bitcast" ||
            to_string(RefPtrKind::Ptr) != "$ptr") {
            std::cerr << "enum stringifier check failed\n";
            ++failures;
        }
    }

    void check_all_kinds(source::Span span) {
        CHECK_NODE(make_Path(span, std::vector<PathSegment>{{PathSegmentKind::TypeName, span}}), Path);
        CHECK_NODE(make_DeclName(span, DeclNameKind::Wildcard), DeclName);
        CHECK_NODE(make_DocComment(span), DocComment);
        CHECK_NODE(make_Attribute(span, span), Attribute);
        CHECK_NODE(make_AttributeList(span), AttributeList);

        CHECK_NODE(make_BuiltinType(span, BuiltinTypeKind::F64), BuiltinType);
        CHECK_NODE(make_NamedType(span, path(span)), NamedType);
        CHECK_NODE(make_RefType(span, true, type(span)), RefType);
        CHECK_NODE(make_PtrType(span, false, type(span)), PtrType);
        CHECK_NODE(make_ArrayType(span, expression(span), type(span)), ArrayType);
        CHECK_NODE(make_FuncType(span), FuncType);
        CHECK_NODE(make_AutoType(span), AutoType);

        CHECK_NODE(make_IntLit(span, NumericBase::Hexadecimal, span, std::nullopt, true), IntLit);
        CHECK_NODE(make_FloatLit(span, NumericBase::Decimal, span, span, false), FloatLit);
        CHECK_NODE(make_StrLit(span, span), StrLit);
        CHECK_NODE(make_RuneLit(span, span), RuneLit);
        CHECK_NODE(make_BoolLit(span, true), BoolLit);
        CHECK_NODE(make_NullptrLit(span), NullptrLit);
        CHECK_NODE(make_NameRef(span, path(span)), NameRef);
        CHECK_NODE(make_SelfExpr(span), SelfExpr);
        CHECK_NODE(make_ParenExpr(span, expression(span)), ParenExpr);
        CHECK_NODE(make_UnaryExpr(span, UnaryOp::Negate, expression(span)), UnaryExpr);
        CHECK_NODE(make_BinaryExpr(span, BinaryOp::Power, expression(span), expression(span)), BinaryExpr);
        CHECK_NODE(make_CondExpr(span, expression(span), expression(span), expression(span)), CondExpr);
        CHECK_NODE(make_CallExpr(span, expression(span)), CallExpr);
        CHECK_NODE(make_FieldExpr(span, expression(span), span), FieldExpr);
        CHECK_NODE(make_IndexExpr(span, expression(span), expression(span)), IndexExpr);
        CHECK_NODE(make_StructLit(span, path(span)), StructLit);
        CHECK_NODE(make_ArrayLit(span, make_ArrayType(span, expression(span), type(span))), ArrayLit);
        CHECK_NODE(make_SizeofExpr(span, type(span)), SizeofExpr);
        CHECK_NODE(make_AlignofExpr(span, type(span)), AlignofExpr);
        CHECK_NODE(make_CastExpr(span, CastKind::Saturate, type(span), expression(span)), CastExpr);
        CHECK_NODE(make_RefExpr(span, RefPtrKind::Ref, true, expression(span)), RefExpr);
        CHECK_NODE(make_PtrExpr(span, RefPtrKind::Ptr, false, expression(span)), PtrExpr);

        CHECK_NODE(make_Block(span), Block);
        CHECK_NODE(make_ExprStmt(span, expression(span)), ExprStmt);
        CHECK_NODE(make_VarDecl(span, StorageKind::Let, true, type(span), decl_name(span)), VarDecl);
        CHECK_NODE(make_AssignStmt(span, AssignOp::Power, expression(span), expression(span)), AssignStmt);
        CHECK_NODE(make_IncDecStmt(span, IncDecKind::Increment, expression(span)), IncDecStmt);
        CHECK_NODE(make_ReturnStmt(span, expression(span)), ReturnStmt);
        CHECK_NODE(make_BreakStmt(span), BreakStmt);
        CHECK_NODE(make_ContinueStmt(span), ContinueStmt);
        CHECK_NODE(make_IfStmt(span, expression(span), make_Block(span)), IfStmt);
        CHECK_NODE(make_WhileStmt(span, expression(span), make_Block(span)), WhileStmt);
        CHECK_NODE(make_DoWhileStmt(span, make_Block(span), expression(span)), DoWhileStmt);
        CHECK_NODE(make_DoStmt(span, type(span), decl_name(span), expression(span), make_Block(span)), DoStmt);
        CHECK_NODE(make_CtIfStmt(span), CtIfStmt);

        CHECK_NODE(make_SourceFile(span), SourceFile);
        CHECK_NODE(make_ModuleDecl(span, path(span)), ModuleDecl);
        CHECK_NODE(make_ImportDecl(span, path(span)), ImportDecl);
        CHECK_NODE(make_TypeDecl(span, true, span), TypeDecl);
        CHECK_NODE(make_FieldDecl(span, false, type(span), span), FieldDecl);
        CHECK_NODE(make_ImplDecl(span, path(span)), ImplDecl);
        CHECK_NODE(make_FuncDecl(span, true, span), FuncDecl);
        CHECK_NODE(make_Param(span, type(span), decl_name(span)), Param);
        CHECK_NODE(make_SelfParam(span, SelfParamKind::RefMut), SelfParam);
        CHECK_NODE(make_CtIfItem(span), CtIfItem);
        CHECK_NODE(make_CtIfField(span), CtIfField);
        CHECK_NODE(make_CtIfMember(span), CtIfMember);

        CHECK_NODE(make_ErrorExpr(span), ErrorExpr);
        CHECK_NODE(make_ErrorStmt(span), ErrorStmt);
        CHECK_NODE(make_ErrorItem(span), ErrorItem);
        CHECK_NODE(make_ErrorType(span), ErrorType);
    }

}

#undef CHECK_NODE

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]).ends_with(".xe") == false) {
        std::cerr << "usage: test/xenon-ast <file.xe>\n";
        return 2;
    }

    try {
        const auto file_id = xenon::source::g_source_file_manager.load(argv[1]);
        const auto& file = xenon::source::g_source_file_manager.get(file_id);
        const xenon::source::Span span{file_id, 0, std::min<std::size_t>(1, file.size())};
        check_all_kinds(span);
        check_stringifiers();
    } catch (const std::exception& error) {
        std::cerr << "could not load test source: " << error.what() << '\n';
        return 2;
    }

    if (failures != 0) {
        return 1;
    }
    std::cout << "all AST node kinds and spans passed\n";
    return 0;
}