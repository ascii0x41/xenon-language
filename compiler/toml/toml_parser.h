#pragma once

#include <optional>
#include <unordered_map>
#include <string>

#include "common/dataclasses.h"

namespace xenon::toml {

    using TOMLMap = std::unordered_map<std::string, std::string>;
    using TOMLResult = std::optional<TOMLMap>;

    TOMLResult parse_toml_file(const fs::path& path);
    TOMLResult parse_toml_string(const std::string& source);

} // namespace xenon::toml