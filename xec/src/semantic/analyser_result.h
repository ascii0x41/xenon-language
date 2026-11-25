#pragma once

#include "astlib/astlib.h"
#include "common/result/result.h"

namespace xenon::sema {

    struct Output {
        ast::SourceFilePtr ast;
    };

    using AnalyserOutput = result::Output<Output>;

} // namespace xenon::sema
