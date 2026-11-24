#pragma once

#include <utility>
#include <vector>

#include "common/diagnostics/diagnostic.h"
#include "common/diagnostics/diagnostic_collector.h"

namespace xenon::result {

    template<typename T>
    class Result {
    public:
        Result(const T& data)
            : data_(data), success_(true) {}

        Result(T&& data)
            : data_(std::move(data)), success_(true) {}

        Result(const T& data, std::vector<diagnostics::Diagnostic> diagnostics, bool success)
            : data_(data), diagnostics_(std::move(diagnostics)), success_(success) {}

        Result(T&& data, std::vector<diagnostics::Diagnostic> diagnostics, bool success)
            : data_(std::move(data)), diagnostics_(std::move(diagnostics)), success_(success) {}

        Result(const diagnostics::Diagnostic& diagnostic)
            : data_(), diagnostics_{diagnostic}, success_(false) {}

        Result(diagnostics::Diagnostic&& diagnostic)
            : data_(), diagnostics_{std::move(diagnostic)}, success_(false) {}

        Result(std::vector<diagnostics::Diagnostic> diagnostics, bool success)
            : data_(), diagnostics_(std::move(diagnostics)), success_(success) {}

        [[nodiscard]] explicit operator bool() const noexcept {
            return success_;
        }

        [[nodiscard]] bool has_value() const noexcept {
            return success_;
        }

        [[nodiscard]] T& value() noexcept { return data_; }
        [[nodiscard]] const T& value() const noexcept { return data_; }

        [[nodiscard]] T& data() noexcept { return data_; }
        [[nodiscard]] const T& data() const noexcept { return data_; }

        [[nodiscard]] std::vector<diagnostics::Diagnostic>& diagnostics() noexcept {
            return diagnostics_;
        }
        [[nodiscard]] const std::vector<diagnostics::Diagnostic>& diagnostics() const noexcept {
            return diagnostics_;
        }

        [[nodiscard]] bool success() const noexcept { return success_; }

        [[nodiscard]] diagnostics::Diagnostic& error() {
            return diagnostics_.front();
        }
        [[nodiscard]] const diagnostics::Diagnostic& error() const {
            return diagnostics_.front();
        }

    private:
        T data_;
        std::vector<diagnostics::Diagnostic> diagnostics_;
        bool success_;
    };

    template<typename T>
    class Output {
    public:
        Output(const T& data)
            : data_(data), success_(true) {}

        Output(T&& data)
            : data_(std::move(data)), success_(true) {}

        Output(const T& data, diagnostics::DiagnosticCollector diagnostics, bool success)
            : data_(data), diagnostics_(std::move(diagnostics)), success_(success) {}

        Output(T&& data, diagnostics::DiagnosticCollector diagnostics, bool success)
            : data_(std::move(data)), diagnostics_(std::move(diagnostics)), success_(success) {}

        Output(const diagnostics::Diagnostic& diagnostic)
            : data_(), success_(false) {
            diagnostics_.report(diagnostic);
        }

        Output(diagnostics::Diagnostic&& diagnostic)
            : data_(), success_(false) {
            diagnostics_.report(std::move(diagnostic));
        }

        Output(diagnostics::DiagnosticCollector diagnostics, bool success)
            : data_(), diagnostics_(std::move(diagnostics)), success_(success) {}

        [[nodiscard]] explicit operator bool() const noexcept {
            return success_;
        }

        [[nodiscard]] bool has_value() const noexcept {
            return success_;
        }

        [[nodiscard]] T& value() noexcept { return data_; }
        [[nodiscard]] const T& value() const noexcept { return data_; }

        [[nodiscard]] T& data() noexcept { return data_; }
        [[nodiscard]] const T& data() const noexcept { return data_; }

        [[nodiscard]] diagnostics::DiagnosticCollector& diagnostics() noexcept {
            return diagnostics_;
        }
        [[nodiscard]] const diagnostics::DiagnosticCollector& diagnostics() const noexcept {
            return diagnostics_;
        }

        [[nodiscard]] bool success() const noexcept { return success_; }

        [[nodiscard]] const diagnostics::Diagnostic& error() {
            return diagnostics_.diagnostics().front();
        }
        [[nodiscard]] const diagnostics::Diagnostic& error() const {
            return diagnostics_.diagnostics().front();
        }

    private:
        T data_;
        diagnostics::DiagnosticCollector diagnostics_;
        bool success_;
    };

} // namespace xenon::result
