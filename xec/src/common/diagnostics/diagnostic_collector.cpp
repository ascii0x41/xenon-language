#include "common/diagnostics/diagnostic_collector.h"

#include <utility>

namespace xenon::diagnostics {

void DiagnosticCollector::report(Diagnostic diagnostic) {
    switch (diagnostic.severity) {
        case Severity::note:
            break;

        case Severity::warning:
            ++warning_count_;
            break;

        case Severity::error:
        case Severity::fatal:
            ++error_count_;
            if (diagnostic.severity == Severity::fatal) {
                ++fatal_count_;
            }
            break;
    }

    diagnostics_.push_back(std::move(diagnostic));
}

void DiagnosticCollector::merge(const DiagnosticCollector& other) {
    if (this == &other) {
        return;
    }

    for (const Diagnostic& diagnostic : other.diagnostics_) {
        report(diagnostic);
    }
}

void DiagnosticCollector::note(
    std::string message,
    std::optional<source::Span> span
) {
    report({Severity::note, std::move(message), std::move(span)});
}

void DiagnosticCollector::warning(
    std::string message,
    std::optional<source::Span> span
) {
    report({Severity::warning, std::move(message), std::move(span)});
}

void DiagnosticCollector::error(
    std::string message,
    std::optional<source::Span> span
) {
    report({Severity::error, std::move(message), std::move(span)});
}

void DiagnosticCollector::fatal(
    std::string message,
    std::optional<source::Span> span
) {
    report({Severity::fatal, std::move(message), std::move(span)});
}

const std::vector<Diagnostic>&
DiagnosticCollector::diagnostics() const noexcept {
    return diagnostics_;
}

std::size_t DiagnosticCollector::count(Severity severity) const noexcept {
    switch (severity) {
        case Severity::note:
            return diagnostics_.size()
                - error_count_
                - warning_count_;

        case Severity::warning:
            return warning_count_;

        case Severity::error:
            return error_count_ - fatal_count_;

        case Severity::fatal:
            return fatal_count_;
    }

    return 0;
}

std::size_t DiagnosticCollector::error_count() const noexcept {
    // Includes fatal diagnostics.
    return error_count_;
}

std::size_t DiagnosticCollector::warning_count() const noexcept {
    return warning_count_;
}

bool DiagnosticCollector::has_errors() const noexcept {
    return error_count_ > 0;
}

bool DiagnosticCollector::has_fatal_errors() const noexcept {
    return fatal_count_ > 0;
}

void DiagnosticCollector::clear() noexcept {
    diagnostics_.clear();
    error_count_ = 0;
    warning_count_ = 0;
    fatal_count_ = 0;
}

} // namespace xenon::diagnostics
