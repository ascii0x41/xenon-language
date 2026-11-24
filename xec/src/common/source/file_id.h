#pragma once

#include <cstdint>
#include <limits>

namespace xenon::source {

    struct FileID {
        std::uint32_t value_;

        [[nodiscard]]
        static constexpr FileID invalid() noexcept {
            return {std::numeric_limits<std::uint32_t>::max()};
        }

        constexpr bool operator==(const FileID&) const = default;
    };

} // namespace xenon::source
