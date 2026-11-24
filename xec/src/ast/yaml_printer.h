#pragma once

#include "parser/parser_result.h"

#include <filesystem>
#include <iosfwd>

namespace xenon::ast {

    void print_yaml(
        std::ostream& output,
        const std::filesystem::path& source_path,
        const parser::ParserOutput& result,
        bool first_source
    );

} // namespace xenon::ast