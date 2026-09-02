#pragma once

#include "common/dataclasses.h"
#include <format>
#include <string>
#include <vector>
#include <stdexcept>
#include <format>
#include <iostream>
#include <fstream>
#include <optional>

namespace xenon {
    enum class Severity { NOTE, WARNING, ERROR, FATAL };

    inline std::string severity_to_string(Severity s) {
        switch (s) {
            case Severity::NOTE:    return "note";
            case Severity::WARNING: return "warning";
            case Severity::ERROR:   return "error";
            case Severity::FATAL:   return "fatal error";
            default:                return "?";
        }
    }

    namespace colour {
        inline constexpr const char* RESET   = "\033[0m";
        inline constexpr const char* BOLD    = "\033[1m";
        inline constexpr const char* RED     = "\033[31m";
        inline constexpr const char* YELLOW  = "\033[33m";
        inline constexpr const char* CYAN    = "\033[36m";
        inline constexpr const char* WHITE   = "\033[37m";
    }

    inline std::string severity_to_colour(Severity s) {
        switch (s) {
            case Severity::NOTE:    return colour::CYAN;
            case Severity::WARNING: return colour::YELLOW;
            case Severity::ERROR:
            case Severity::FATAL:   return colour::RED;
            default:                return colour::WHITE;
        }
    }

    // Escapes control characters for safe single-line display in a
    // diagnostic message (e.g. a decoded string-literal lexeme that
    // contains a real '\n' byte, which would otherwise break the
    // "file:line:col: severity: message" line across two terminal lines).
    inline std::string escape_for_display(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) {
            switch (c) {
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                case '\0': out += "\\0"; break;
                default:
                    if (c < 0x20) {
                        out += std::format("\\x{:02x}", c);
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
        return out;
    }

    // Re-reads the offending line straight from disk for display
    // purposes. This shows the original source text (e.g. the literal
    // backslash-n a user typed), rather than a token's already-decoded
    // lexeme, which is what you actually want under a caret.
    inline std::optional<std::string> read_source_line(const std::string& file, uint32_t line) {
        if (file.empty() || line == 0) return std::nullopt;
        std::ifstream in(file);
        if (!in) return std::nullopt;

        std::string text;
        for (uint32_t i = 1; i <= line; ++i) {
            if (!std::getline(in, text)) return std::nullopt;
        }
        return text;
    }


    class CompilerException : public std::exception {
    public:
        std::string message;
        common::SourceLocation location;
        Severity severity;

        // Defaults to FATAL: driver-level throw sites (missing config,
        // unresolved import, circular dependency) don't need to change.
        // Parser/lexer throw sites pass Severity::ERROR explicitly, since
        // a syntax mistake is routine, not a reason to badge the message
        // as if the compiler itself broke.
        CompilerException(std::string msg, common::SourceLocation loc = common::SourceLocation(), Severity sev = Severity::FATAL)
            : message(std::move(msg)), location(loc), severity(sev) {}

        const char* what() const noexcept override {
            return message.c_str();
        }
    };

    struct Diagnostic {
        Severity severity;
        std::string message;
        // 3. Initialize driver
        common::SourceLocation location;

        // Format as  "file:line:col: severity: message", followed by the
        // offending source line and a caret under the reported column
        // (when the file is readable and the location is valid).
        std::string to_string(bool use_colour = true, const std::string& project_root = {}) const {
            const std::string escaped_message = escape_for_display(message);
            std::string result = !use_colour
                ? std::format("{}: {}: {}",
                    location.format(project_root),
                    severity_to_string(severity),
                    escaped_message)
                : std::string(colour::BOLD)
                    + location.format(project_root) + ": "
                    + severity_to_colour(severity)
                    + severity_to_string(severity)
                    + colour::RESET + colour::BOLD + ": "
                    + escaped_message + colour::RESET;

            if (auto src_line = read_source_line(location.file, location.line)) {
                result += "\n    " + escape_for_display(*src_line);

                if (location.column >= 1 && location.column <= src_line->size() + 1) {
                    std::string caret_line = "\n    " + std::string(location.column - 1, ' ');
                    caret_line += use_colour
                        ? std::string(colour::BOLD) + colour::RED + "^" + colour::RESET
                        : "^";
                    result += caret_line;
                }
            }

            return result;
        }
    };

    class DiagnosticCollector {
        void push(Severity sev, std::string msg, common::SourceLocation location) {
            diagnostics_.emplace_back(sev, std::move(msg), common::SourceLocation{location.line, location.column, location.file});
        }

        std::vector<Diagnostic> diagnostics_;
        size_t error_count_   = 0;
        size_t warning_count_ = 0;
        std::string project_root_;
    public:
        bool has_errors()   const { return error_count_   > 0; }
        bool has_warnings() const { return warning_count_ > 0; }

        size_t error_count()   const { return error_count_;   }
        size_t warning_count() const { return warning_count_; }

        const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

        void set_project_root(std::string project_root) {
            project_root_ = std::move(project_root);
        }

        int exit_gracefully(bool use_colour = true) const {
            for (const auto& d : diagnostics_)
                std::cerr << d.to_string(use_colour, project_root_) << "\n";

            if (error_count_ > 0) {
                std::string summary = std::format(
                    "{}{} error{} generated.{}",
                    use_colour ? colour::BOLD : "",
                    error_count_,
                    error_count_ == 1 ? "" : "s",
                    use_colour ? colour::RESET : ""
                );
                std::cerr << summary << "\n";

            }

            if (warning_count_ > 0) {
                std::string summary = std::format(
                    "{}{} warning{} generated.{}",
                    use_colour ? colour::BOLD : "",
                    warning_count_,
                    warning_count_ == 1 ? "" : "s",
                    use_colour ? colour::RESET : ""
                );
                std::cerr << summary << "\n";
            }

            if (error_count_ > 0) { std::cerr << "Task failed due to errors.\n"; return 1; }
            else if (warning_count_ > 0) std::cerr << "Task succeeded with warnings.\n";
            else std::cerr << "Task succeeded.\n";
            return 0;
        }

            void note(std::string msg, common::SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::NOTE, std::move(msg), std::move(l));
            }

            void warning(std::string msg, common::SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::WARNING, std::move(msg), std::move(l));
                ++warning_count_;
            }

            void error(std::string msg, common::SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::ERROR, std::move(msg), std::move(l));
                ++error_count_;
            }

            void syntax_error(std::string msg, common::SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::ERROR, std::move(msg), std::move(l));
                ++error_count_;
            }


            void fatal(std::string msg, common::SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::FATAL, std::move(msg), std::move(l));
                ++error_count_;
            }
    };

    inline thread_local DiagnosticCollector g_diagnostics;
} // namespace xenon
