#pragma once

#include "common/source/file_id.h"
#include "common/source/line_col.h"
#include "common/source/span.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace xenon::source {

class SourceFile {
public:
    SourceFile(FileID id, std::string path, std::string text);

    [[nodiscard]] FileID id() const noexcept;
    [[nodiscard]] const std::string& path() const noexcept;
    [[nodiscard]] std::string_view text() const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    // Converts a byte offset into a one-based line and column.
    [[nodiscard]] LineCol line_col(std::size_t offset) const;

    // Returns the contents of a one-based source line.
    [[nodiscard]] std::string_view line_text(std::size_t line) const;

    // Returns the text covered by a span belonging to this file.
    [[nodiscard]] std::string_view slice(Span span) const;

private:
    void build_line_starts();

    FileID id_;
    std::string path_;
    std::string text_;

    // Byte offset of the first byte of each line.
    // The first line always starts at offset zero.
    std::vector<std::size_t> line_starts_;
};

} // namespace xenon::source
