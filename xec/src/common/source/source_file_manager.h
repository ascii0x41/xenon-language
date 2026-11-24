#pragma once

#include "common/source/file_id.h"
#include "common/source/source_file.h"

#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace xenon::source {

    class SourceFileManager {
    public:
        SourceFileManager() = default;

        SourceFileManager(const SourceFileManager&) = delete;
        SourceFileManager& operator=(const SourceFileManager&) = delete;

        // Loads a file from disk, or returns its existing ID if already loaded.
        [[nodiscard]] FileID load(const std::filesystem::path& path);

        // Adds source text that does not necessarily exist on disk.
        [[nodiscard]] FileID add_virtual(
            std::string name,
            std::string text
        );

        [[nodiscard]] const SourceFile& get(FileID id) const;

        [[nodiscard]] std::size_t file_count() const noexcept;

    private:
        [[nodiscard]] FileID add(
            std::string path,
            std::string text
        );

        [[nodiscard]] std::string normalize_path(
            const std::filesystem::path& path
        ) const;

        std::deque<SourceFile> files_;

        // Maps normalized physical paths to their assigned IDs.
        std::unordered_map<std::string, FileID> path_to_id_;
    };

    inline SourceFileManager g_source_file_manager = SourceFileManager();

} // namespace xenon::source
