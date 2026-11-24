#include "common/diagnostics/diagnostic_formatter.h"
#include "common/diagnostics/diagnostic_collector.h"

#include <sstream>
#include <string>

namespace xenon::diagnostics {

DiagnosticFormatter::DiagnosticFormatter(FormatOptions options)
    : options_(options) {}

std::string DiagnosticFormatter::format(
    const Diagnostic& diagnostic,
    const source::SourceFileManager& sources
) const {
    std::ostringstream output;

    if (diagnostic.span) {
        const auto& span = *diagnostic.span;
        const auto& file = sources.get(span.file);

        // Validate the complete span, not just its starting offset.
        (void)file.slice(span);

        const auto position = file.line_col(span.start);

        output << file.path()
               << ':' << position.line
               << ':' << position.column
               << ": ";
    } else {
        output << "xec: ";
    }

    output << colourise(
        diagnostic.severity,
        severity_label(diagnostic.severity)
    );

    output << ": " << diagnostic.message;

    if (diagnostic.span && options_.show_source_line) {
        const auto& span = *diagnostic.span;
        const auto& file = sources.get(span.file);
        const auto position = file.line_col(span.start);
        const auto line = file.line_text(position.line);

        output << '\n'
               << line << '\n'
               << std::string(position.column - 1, ' ')
               << '^';
    }

    return output.str();
}

std::string DiagnosticFormatter::format_all(
    const DiagnosticCollector& collector,
    const source::SourceFileManager& sources
) const {
    std::ostringstream output;

    bool first = true;

    for (const auto& diagnostic : collector.diagnostics()) {
        if (!first) {
            output << '\n';
        }

        output << format(diagnostic, sources);
        first = false;
    }

    return output.str();
}

std::string DiagnosticFormatter::severity_label(
    Severity severity
) const {
    return std::string(severity_to_string(severity));
}

std::string DiagnosticFormatter::colourise(
    Severity severity,
    const std::string& text
) const {
    if (!options_.color) {
        return text;
    }

    const char* color = "";

    switch (severity) {
        case Severity::note:
            color = "\033[36m"; // Cyan
            break;

        case Severity::warning:
            color = "\033[33m"; // Yellow
            break;

        case Severity::error:
        case Severity::fatal:
            color = "\033[31m"; // Red
            break;
    }

    return std::string(color) + text + "\033[0m";
}

} // namespace xenon::diagnostics
