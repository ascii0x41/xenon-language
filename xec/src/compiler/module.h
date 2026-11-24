#pragma once

#include <string>

#include "ast/ast.h"
#include "common/source/file_id.h"


namespace xenon::compiler {

    class Module {
    public:
        Module(const std::string& n, const source::FileID& fid, ast::AST* ast)
            : name(n), source_file_id(fid), moduleAST(ast) {}

        const std::string& getName() const { return name; }
        const source::FileID& getSourceFileID() const { return source_file_id; }
        ast::AST* getModuleAST() const { return moduleAST; }
        bool is_root() const { return name.empty(); }

    private:
        std::string name;
        source::FileID source_file_id;
        ast::AST* moduleAST;
    };

} // namespace xenon::compiler
