#pragma once

#include "ast/ast.h"
#include "ast/header.h"
#include "common/result/result.h"

#include <memory>

namespace xenon::parser {

    struct Output {
        std::unique_ptr<ast::AST> ast;
        ast::HeaderInfo header;
    };

    using ParserOutput = result::Output<Output>;

} // namespace xenon::parser
