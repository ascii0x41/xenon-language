#pragma once

#include "astlib/astlib.h"
#include "common/result/result.h"

#include <memory>

namespace xenon::parser {

    struct Output {
        ast::SourceFilePtr ast;
    };

    using ParserOutput = result::Output<Output>;

} // namespace xenon::parser
