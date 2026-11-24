#pragma once

#include "common/diagnostics/severity.h"
#include "common/source/span.h"

#include <optional>
#include <string>
#include <utility>

namespace xenon::diagnostics {

struct Diagnostic {
    Severity severity{Severity::error};
    std::string message;
    std::optional<source::Span> span;

    Diagnostic() = default;

    Diagnostic(
        Severity severity_,
        std::string message_,
        std::optional<source::Span> span_ = std::nullopt
    )
        : severity(severity_),
          message(std::move(message_)),
          span(std::move(span_)) {}
};

} // namespace xenon::diagnostics