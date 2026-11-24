#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace xenon::config {

    // A fatal problem with project, environment or standard-library configuration.
    // The loaders and the resolver throw it; the driver catches it in one place
    // and reports it. `what()` is formatted as "file:line:col: message" (the
    // location parts are omitted when unknown).
    class ConfigError : public std::runtime_error {
    public:
        explicit ConfigError(std::string message,
                             std::filesystem::path file = {},
                             unsigned line = 0,
                             unsigned column = 0)
            : std::runtime_error(format(message, file, line, column)),
              message_(std::move(message)),
              file_(std::move(file)),
              line_(line),
              column_(column) {}

        const std::string& message() const noexcept { return message_; }
        const std::filesystem::path& file() const noexcept { return file_; }
        unsigned line() const noexcept { return line_; }
        unsigned column() const noexcept { return column_; }

    private:
        static std::string format(const std::string& message,
                                  const std::filesystem::path& file,
                                  unsigned line,
                                  unsigned column) {
            std::string out;
            if (!file.empty()) {
                out += file.string();
                if (line != 0) {
                    out += ":" + std::to_string(line);
                    if (column != 0) {
                        out += ":" + std::to_string(column);
                    }
                }
                out += ": ";
            }
            out += message;
            return out;
        }

        std::string message_;
        std::filesystem::path file_;
        unsigned line_;
        unsigned column_;
    };

} // namespace xenon::config
