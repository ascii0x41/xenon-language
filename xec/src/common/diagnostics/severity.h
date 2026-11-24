#pragma once

#include <string_view>

namespace xenon::diagnostics {

enum class Severity {
    note,
    warning,
    error,
    fatal
};

[[nodiscard]] constexpr std::string_view
severity_to_string(Severity severity) noexcept {
    switch (severity) {
        case Severity::note:    return "note";
        case Severity::warning: return "warning";
        case Severity::error:   return "error";
        case Severity::fatal:   return "fatal";
    }

    return "unknown";
}

} // namespace xenon::diagnostics
