#pragma once

#include "common/dataclasses.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <format>
#include <iostream>

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


    class CompilerException : public std::exception {
    public:
        std::string message;
        SourceLocation location;

        CompilerException(std::string msg, SourceLocation loc = {})
            : message(std::move(msg)), location(loc) {}

        const char* what() const noexcept override {
            return message.c_str();
        }
    };

    struct Diagnostic {
        Severity severity;
        std::string message;
        SourceLocation location;

        // Format as  "file:line:col: severity: message"
        std::string to_string(bool use_colour = true) const {
            if (!use_colour) 
                return std::format("{}: {}: {}", 
                    location.format(), 
                    severity_to_string(severity),
                    message);
                
            return std::string(colour::BOLD)
                + location.format() + ": "
                + severity_to_colour(severity)
                + severity_to_string(severity)
                + colour::RESET + colour::BOLD + ": "
                + message + colour::RESET;
        }
    };

    class DiagnosticCollector {
        void push(Severity sev, std::string msg, SourceLocation location) {
            diagnostics_.emplace_back(sev, std::move(msg), SourceLocation{location.line, location.column, location.file});
        }      

        std::vector<Diagnostic> diagnostics_;
        size_t error_count_   = 0;
        size_t warning_count_ = 0;
    public:
        bool has_errors()   const { return error_count_   > 0; }
        bool has_warnings() const { return warning_count_ > 0; }

        size_t error_count()   const { return error_count_;   }
        size_t warning_count() const { return warning_count_; }

        const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

        int exit_gracefully(bool use_colour = true) const {
            for (const auto& d : diagnostics_)
                std::cerr << d.to_string(use_colour) << "\n";

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

            if (error_count_ > 0) { std::cerr << "Compilation failed due to errors.\n"; return 1; }
            else if (warning_count_ > 0) std::cerr << "Compilation succeeded with warnings.\n";
            else std::cerr << "Compilation succeeded.\n";
            return 0;
        }

            void note(std::string msg, SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::NOTE, std::move(msg), std::move(l));
            }

            void warning(std::string msg, SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::WARNING, std::move(msg), std::move(l));
                ++warning_count_;
            }

            void error(std::string msg, SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::ERROR, std::move(msg), std::move(l));
                ++error_count_;
            }

            void syntax_error(std::string msg, SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::ERROR, std::move(msg), std::move(l));
                ++error_count_;
            }


            void fatal(std::string msg, SourceLocation l = { 0, 0, "xec" }) {
                push(Severity::FATAL, std::move(msg), std::move(l));
                ++error_count_;
            }
    };

    inline thread_local DiagnosticCollector g_diagnostics;
} // namespace xenon
