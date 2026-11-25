#include "astlib/astlib.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "common/source/source_file_manager.h"

#include <iostream>
#include <string_view>

namespace {

std::string_view text(const xenon::source::Span& span) {
    return xenon::source::g_source_file_manager.get(span.file).slice(span);
}

void dump(const xenon::ast::Node& node, std::size_t indent = 0) {
    std::cout << std::string(indent, ' ') << xenon::ast::to_string(node.kind);
    if (node.kind == xenon::ast::Kind::NameRef) {
        const auto& n = static_cast<const xenon::ast::NameRef&>(node);
        std::cout << " " << text(n.path->segments.front().span);
    } else if (node.kind == xenon::ast::Kind::BinaryExpr) {
        const auto& b = static_cast<const xenon::ast::BinaryExpr&>(node);
        std::cout << " " << xenon::ast::to_string(b.op);
    } else if (node.kind == xenon::ast::Kind::UnaryExpr) {
        const auto& u = static_cast<const xenon::ast::UnaryExpr&>(node);
        std::cout << " " << xenon::ast::to_string(u.op);
    } else if (node.kind == xenon::ast::Kind::IntLit) {
        const auto& lit = static_cast<const xenon::ast::IntLit&>(node);
        std::cout << " " << text(lit.digits) << (lit.unsuffixed ? " unsuffixed" : "");
    } else if (node.kind == xenon::ast::Kind::FloatLit) {
        const auto& lit = static_cast<const xenon::ast::FloatLit&>(node);
        std::cout << " " << text(lit.digits) << (lit.unsuffixed ? " unsuffixed" : "");
    } else if (node.kind == xenon::ast::Kind::StrLit) {
        std::cout << " " << text(static_cast<const xenon::ast::StrLit&>(node).text);
    } else if (node.kind == xenon::ast::Kind::FuncDecl) {
        const auto& f = static_cast<const xenon::ast::FuncDecl&>(node);
        std::cout << " " << text(f.name);
    } else if (node.kind == xenon::ast::Kind::VarDecl) {
        const auto& v = static_cast<const xenon::ast::VarDecl&>(node);
        std::cout << " " << xenon::ast::to_string(v.storage) << " " << text(v.name->span);
    }
    std::cout << '\n';
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: xenon-parse file.xe\n";
        return 2;
    }
    const auto fid = xenon::source::g_source_file_manager.load(argv[1]);
    const auto& source = xenon::source::g_source_file_manager.get(fid);
    auto lexed = xenon::lexer::Lexer::lex(source);
    if (!lexed) {
        for (const auto& diagnostic : lexed.diagnostics()) {
            std::cerr << diagnostic.message << '\n';
        }
        return 1;
    }
    auto parsed = xenon::parser::Parser::parse(lexed.value(), fid);
    if (parsed.value().ast) {
        const auto& ast = *parsed.value().ast;
        std::cout << xenon::ast::to_string(ast.kind) << '\n';
        if (ast.module) dump(*ast.module, 2);
        for (const auto& item : ast.items) if (item) dump(*item, 2);
    }
    for (const auto& diagnostic : parsed.diagnostics().diagnostics()) {
        std::cerr << diagnostic.message << '\n';
    }
    return parsed.success() ? 0 : 1;
}
