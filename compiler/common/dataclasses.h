#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <format>
#include <cctype>
#include <algorithm>

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

    enum class TargetType {
        X86_64_LINUX,
        X86_64_WINDOWS,
        X86_64_MACOS,
    };

    struct TargetInfo {
        TargetType type;
        const char* llvm_triple;
    };

    // ============================================
    // Configuration stages
    // ============================================

    struct TomlSettings {
        std::optional<std::string> project_name;
        std::optional<std::string> version;
        std::optional<std::vector<std::string>> authors;
        std::optional<std::vector<std::string>> source_files;

        std::optional<fs::path> output_dir;
        std::optional<std::string> target_triple;
        std::optional<OptimisationLevel> optimisation;
        std::optional<WarningLevel> warning;
    };

    struct CLIOverrides {
        Command command = Command::BUILD;
        std::optional<std::string> output_name;
        std::optional<fs::path> output_dir;
        std::optional<std::string> target_triple;
        std::optional<OptimisationLevel> optimisation;
        std::optional<WarningLevel> warning;
        std::optional<bool> dump_tokens;
        std::optional<bool> dump_ast;
        std::optional<bool> no_colour;
        std::optional<bool> verbose;
    };

    struct ResolvedConfig {
        // Required project metadata
        std::string project_name;
        std::string version = "0.1.0";
        std::vector<std::string> authors;

        // Resolved project layout
        fs::path project_root;
        std::vector<fs::path> source_paths;

        // Final build settings
        fs::path output_dir = "build/";
        std::string output_name;
        std::string target_triple;
        TargetInfo target_info{TargetType::X86_64_LINUX, "x86_64-pc-linux-gnu"};
        OptimisationLevel optimisation = OptimisationLevel::DEBUG;
        WarningLevel warning = WarningLevel::WARN;

        // CLI-only runtime flags
        Command command = Command::BUILD;
        bool dump_tokens = false;
        bool dump_ast = false;
        bool no_colour = false;
        bool verbose = false;
    };

    using CompilerConfig = ResolvedConfig;

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

    inline std::optional<TargetInfo> parse_target_triple(const std::string& s) {
        if (s.empty()) {
            return std::nullopt;
        }

        std::string normalized = s;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (normalized == "x86_64-linux" || normalized == "x86_64-pc-linux-gnu") {
            return TargetInfo{TargetType::X86_64_LINUX, "x86_64-pc-linux-gnu"};
        }
        if (normalized == "x86_64-windows" || normalized == "x86_64-pc-windows-msvc") {
            return TargetInfo{TargetType::X86_64_WINDOWS, "x86_64-pc-windows-msvc"};
        }
        if (normalized == "x86_64-macos" || normalized == "x86_64-apple-darwin") {
            return TargetInfo{TargetType::X86_64_MACOS, "x86_64-apple-darwin"};
        }

        return std::nullopt;
    }

} // namespace xenon::config