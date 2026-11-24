#pragma once

#include "common/diagnostics/diagnostic.h"
#include "common/diagnostics/diagnostic_collector.h"
#include "common/source/source_file_manager.h"

#include <string>

namespace xenon::diagnostics {

struct FormatOptions {
    bool color{false};
    bool show_source_line{true};
};

class DiagnosticFormatter {
public:
    explicit DiagnosticFormatter(FormatOptions options = {});

    [[nodiscard]] std::string format(
        const Diagnostic& diagnostic,
        const source::SourceFileManager& sources
    ) const;

    [[nodiscard]] std::string format_all(
        const DiagnosticCollector& collector,
        const source::SourceFileManager& sources
    ) const;

private:
    [[nodiscard]] std::string severity_label(Severity severity) const;
    [[nodiscard]] std::string colourise(
        Severity severity,
        const std::string& text
    ) const;

    FormatOptions options_;
};

} // namespace xenon::diagnostics
