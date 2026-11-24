#pragma once

#include "common/diagnostics/diagnostic.h"

#include <cstddef>
#include <string>
#include <vector>

namespace xenon::diagnostics {

class DiagnosticCollector {
public:
    void report(Diagnostic diagnostic);

    void merge(const DiagnosticCollector& other);

    void note(
        std::string message,
        std::optional<source::Span> span = std::nullopt
    );

    void warning(
        std::string message,
        std::optional<source::Span> span = std::nullopt
    );

    void error(
        std::string message,
        std::optional<source::Span> span = std::nullopt
    );

    void fatal(
        std::string message,
        std::optional<source::Span> span = std::nullopt
    );

    [[nodiscard]] const std::vector<Diagnostic>&
    diagnostics() const noexcept;

    [[nodiscard]] std::size_t count(Severity severity) const noexcept;

    [[nodiscard]] std::size_t error_count() const noexcept;

    [[nodiscard]] std::size_t warning_count() const noexcept;

    [[nodiscard]] bool has_errors() const noexcept;

    [[nodiscard]] bool has_fatal_errors() const noexcept;

    void clear() noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
    std::size_t error_count_{};
    std::size_t warning_count_{};
    std::size_t fatal_count_{};
};

} // namespace xenon::diagnostics
