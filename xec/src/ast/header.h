#pragma once

#include <optional>
#include <optional>
#include <vector>

#include "declarations.h"

namespace xenon::ast {

    struct HeaderInfo {
        std::optional<ModuleDecl> module_decl;
        std::vector<ImportDecl> imports;
    };

} // namespace xenon::ast