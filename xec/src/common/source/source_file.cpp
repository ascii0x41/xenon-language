#include "common/source/source_file.h"
#include "common/source/file_id.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace xenon::source {

namespace {

std::size_t invalid_utf8_offset(std::string_view text) noexcept {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first <= 0x7F) {
            ++i;
            continue;
        }

        std::size_t length = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            length = 2;
        } else if (first >= 0xE0 && first <= 0xEF) {
            length = 3;
        } else if (first >= 0xF0 && first <= 0xF4) {
            length = 4;
        } else {
            return i;
        }

        if (length > text.size() - i) {
            return i;
        }

        for (std::size_t j = 1; j < length; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            if ((next & 0xC0) != 0x80) {
                return i + j;
            }
            if (j == 1 &&
                ((first == 0xE0 && next < 0xA0) ||
                 (first == 0xED && next > 0x9F) ||
                 (first == 0xF0 && next < 0x90) ||
                 (first == 0xF4 && next > 0x8F))) {
                return i + j;
            }
        }

        i += length;
    }

    return std::string_view::npos;
}

} // namespace

SourceFile::SourceFile(FileID id, std::string path, std::string text)
    : id_(id),
      path_(std::move(path)),
      text_(std::move(text)) {
    if (id == FileID::invalid()) {
        throw std::invalid_argument("Invalid source file ID");
    }

    const std::size_t invalid_offset = invalid_utf8_offset(text_);
    if (invalid_offset != std::string_view::npos) {
        throw std::invalid_argument(
            "Source text is not valid UTF-8 at byte offset " +
            std::to_string(invalid_offset)
        );
    }

    build_line_starts();
}

FileID SourceFile::id() const noexcept {
    return id_;
}

const std::string& SourceFile::path() const noexcept {
    return path_;
}

std::string_view SourceFile::text() const noexcept {
    return text_;
}

std::size_t SourceFile::size() const noexcept {
    return text_.size();
}

bool SourceFile::empty() const noexcept {
    return text_.empty();
}

void SourceFile::build_line_starts() {
    line_starts_.clear();
    line_starts_.push_back(0);

    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') {
            line_starts_.push_back(i + 1);
        }
    }
}

LineCol SourceFile::line_col(std::size_t offset) const {
    if (offset > text_.size()) {
        throw std::out_of_range("Source offset is outside the file");
    }

    const auto it = std::upper_bound(
        line_starts_.begin(),
        line_starts_.end(),
        offset
    );

    const std::size_t line_index =
        static_cast<std::size_t>(it - line_starts_.begin() - 1);

    return {
        .line = line_index + 1,
        .column = offset - line_starts_[line_index] + 1
    };
}

std::string_view SourceFile::line_text(std::size_t line) const {
    if (line == 0 || line > line_starts_.size()) {
        throw std::out_of_range("Source line is outside the file");
    }

    const std::size_t start = line_starts_[line - 1];

    std::size_t end = text_.size();

    if (line < line_starts_.size()) {
        end = line_starts_[line] - 1; // Exclude the '\n'.
    }

    return std::string_view(text_).substr(start, end - start);
}

std::string_view SourceFile::slice(Span span) const {
    if (span.file != id_) {
        throw std::invalid_argument("Span belongs to a different source file");
    }

    if (!span.valid() || span.end > text_.size()) {
        throw std::out_of_range("Span is outside the source file");
    }

    return std::string_view(text_).substr(
        span.start,
        span.length()
    );
}

} // namespace xenon::source
