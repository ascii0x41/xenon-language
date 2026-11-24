#pragma once

#include "common/source/file_id.h"

#include <cstddef>

namespace xenon::source {

    struct Span {
        FileID file = FileID::invalid();
        std::size_t start{};
        std::size_t end{};

        [[nodiscard]]
        std::size_t length() const noexcept {
            return valid() ? end - start : 0;
        }

        [[nodiscard]]
        bool valid() const noexcept {
            return file.value_ != FileID::invalid().value_ && start <= end;
        }
    };

} // namespace xenon::source
