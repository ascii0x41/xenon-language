#include "common/source/source_file_manager.h"
#include "common/source/file_id.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace xenon::source {

FileID SourceFileManager::load(const std::filesystem::path& path) {
    const std::string normalized_path = normalize_path(path);

    // Avoid loading the same physical file more than once.
    if (const auto it = path_to_id_.find(normalized_path);
        it != path_to_id_.end()) {
        return it->second;
    }

    std::ifstream file(normalized_path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open source file: " + normalized_path
        );
    }

    std::string text(
        (std::istreambuf_iterator<char>(file)),
        (std::istreambuf_iterator<char>())
    );

    if (file.bad()) {
        throw std::runtime_error(
            "Failed to read source file: " + normalized_path
        );
    }

    const FileID id = add(normalized_path, std::move(text));
    path_to_id_.emplace(normalized_path, id);

    return id;
}

FileID SourceFileManager::add_virtual(
    std::string name,
    std::string text
) {
    return add(std::move(name), std::move(text));
}

FileID SourceFileManager::add(
    std::string path,
    std::string text
) {
    if (files_.size() >= FileID::invalid().value_) {
        throw std::overflow_error("Too many source files");
    }

    const auto id = FileID{ .value_ = static_cast<uint32_t>(files_.size())};

    files_.emplace_back(
        id,
        std::move(path),
        std::move(text)
    );

    return id;
}

const SourceFile& SourceFileManager::get(FileID id) const {
    if (id.value_ >= files_.size()) {
        throw std::out_of_range("Invalid source file ID");
    }

    return files_[id.value_];
}

std::size_t SourceFileManager::file_count() const noexcept {
    return files_.size();
}

std::string SourceFileManager::normalize_path(
    const std::filesystem::path& path
) const {
    if (path.empty()) {
        throw std::invalid_argument("Source file path must not be empty");
    }

    std::error_code error;

    auto normalized = std::filesystem::weakly_canonical(path, error);

    if (error) {
        error.clear();
        normalized = std::filesystem::absolute(path, error);

        if (error) {
            throw std::filesystem::filesystem_error(
                "Failed to normalize source file path",
                path,
                error
            );
        }
    }

    return normalized.lexically_normal().generic_string();
}

} // namespace xenon::source
