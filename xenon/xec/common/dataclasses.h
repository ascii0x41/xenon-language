#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <format>

namespace xenon {
    namespace fs = std::filesystem;
}

namespace xenon::common {

    inline std::string make_display_path(const std::string& file, const std::string& project_root = {}) {
        if (file.empty() || file == "xec" || project_root.empty()) {
            return file;
        }

        const fs::path file_path{file};
        const fs::path root_path{project_root};

        std::error_code ec;
        const fs::path abs_file = fs::absolute(file_path, ec);
        if (ec) {
            return file;
        }

        const fs::path abs_root = fs::absolute(root_path, ec);
        if (ec) {
            return file;
        }

        if (abs_file.is_absolute() && abs_root.is_absolute()) {
            const fs::path rel = fs::relative(abs_file, abs_root);
            if (!rel.empty() && rel != ".") {
                return rel.string();
            }
        }

        const std::string normalized_file = fs::path(file).lexically_normal().string();
        const std::string normalized_root = fs::path(project_root).lexically_normal().string();
        if (!normalized_root.empty()
            && normalized_file.rfind(normalized_root, 0) == 0
            && normalized_file.size() > normalized_root.size()
            && (normalized_file[normalized_root.size()] == '/' || normalized_file[normalized_root.size()] == '\\')) {
            return normalized_file.substr(normalized_root.size() + 1);
        }

        return file;
    }

    struct SourceLocation {
        uint32_t         line   = 0;
        uint32_t         column = 0;
        std::string      file = "xec";

        SourceLocation() = default;
        SourceLocation(uint32_t l, uint32_t c, std::string f = "")
            : line(l), column(c), file(std::move(f)) {}

        inline bool valid() const {
            return line != 0 || column != 0 || !file.empty();
        }

        [[nodiscard]]
        std::string format(const std::string& project_root = {}) const {
            std::string result;
            const std::string display_file = make_display_path(file, project_root);
            
            if (!display_file.empty()) {
                result = display_file;
                if (line != 0) {
                    result += std::format(":{}", line);
                    if (column != 0) {
                        result += std::format(":{}", column);
                    }
                }
            } else if (line != 0) {
                result = std::format("{}", line);
                if (column != 0) {
                    result += std::format(":{}", column);
                }
            }
            
            return result;
        }
    };

} // namespace xenon::common

namespace xenon::config {

    // ============================================
    // Enums
    // ============================================
    
    enum class Command {
        BUILD, CHECK, RUN, INIT, HELP, VERSION
    };
    
    enum class OptimisationLevel {
        NONE, DEBUG, RELEASE // -O0, -O1, -O2
    };
    
    enum class WarningLevel {
        IGNORE, WARN, ERROR // -W0, -W1, -W2
    };

    // ============================================
    // ONE config struct
    // ============================================
    
    struct CompilerConfig {
        // ---- From xenon.toml ----
        std::string project_name;
        std::string version = "0.1.0";
        std::vector<std::string> source_files; // relative to project root
        std::vector<std::string> authors;
        
        // ---- From TOML or CLI ----
        fs::path output_dir = "build/";
        std::string output_name;        // derived from project_name if empty
        std::string target_triple = "x86_64-linux";
        OptimisationLevel opt_level = OptimisationLevel::DEBUG;
        WarningLevel warning_level = WarningLevel::WARN;
        
        // ---- CLI only ----
        Command command = Command::BUILD;
        bool dump_tokens = false;
        bool dump_ast = false;
        bool no_colour = false;
        bool verbose = false;
        
        // ---- Resolved (set during load) ----
        fs::path project_root;
        std::vector<fs::path> source_paths;   // absolute resolved paths
    };

    // Parsers (keep these)
    inline OptimisationLevel parse_opt_level(const std::string& s) {
        if (s == "none") return OptimisationLevel::NONE;
        if (s == "release") return OptimisationLevel::RELEASE;
        return OptimisationLevel::DEBUG;
    }

    inline WarningLevel parse_warning_level(const std::string& s) {
        if (s == "ignore") return WarningLevel::IGNORE;
        if (s == "error") return WarningLevel::ERROR;
        return WarningLevel::WARN;
    }

} // namespace xenon::config