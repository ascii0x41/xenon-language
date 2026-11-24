#pragma once

#include "ast/ast.h"
#include "ast/header.h"
#include "common/result/result.h"

namespace xenon::sema {

    struct Output {
        ast::AST ast;
        ast::HeaderInfo header;
    };

    using AnalyserOutput = result::Output<Output>;

} // namespace xenon::sema
