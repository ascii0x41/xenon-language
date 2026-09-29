#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
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
        std::string output_name;        // derived from project_name if empty; empty is unambiguous, no flag needed
        std::string target_triple;
        TargetInfo target_info{TargetType::X86_64_LINUX, "x86_64-pc-linux-gnu"};
        OptimisationLevel opt_level = OptimisationLevel::DEBUG;
        WarningLevel warning_level = WarningLevel::WARN;

        // Set by the CLI arg parser when the corresponding flag is actually
        // passed on the command line. Needed because these fields have
        // non-empty/non-zero defaults, so "value equals the default" can't be
        // used to tell "user didn't pass this flag" apart from "user passed
        // this flag with the default value" — the driver's TOML/CLI merge
        // needs the real answer to decide whether to let TOML override it.
        bool output_dir_explicit = false;
        bool target_triple_explicit = false;
        bool opt_level_explicit = false;
        bool warning_level_explicit = false;
        
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