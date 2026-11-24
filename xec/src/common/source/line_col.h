#pragma once

#include <cstddef>

namespace xenon::source {

    struct LineCol {
        std::size_t line{};
        std::size_t column{};
    };

} // namespace xenon::source
