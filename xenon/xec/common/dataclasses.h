#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <format>

namespace xenon {

    namespace fs = std::filesystem;

    // ============================================================================
    // FORWARD DECLARATIONS
    // ============================================================================

    struct ASTNode;
    struct Block;
    struct ImportDecl;
    struct ExportDecl;

    // ============================================================================
    // SMART POINTER ALIASES
    // ============================================================================

    template<typename T>
    using Ptr = std::unique_ptr<T>;

    using ASTNodePtr = Ptr<ASTNode>;
    using BlockPtr = Ptr<Block>;

    // ============================================================================
    // SOURCE LOCATION
    // ============================================================================

    struct SourceLocation {
        uint32_t         line   = 0;
        uint32_t         column = 0;
        std::string      file;

        SourceLocation() = default;
        SourceLocation(uint32_t l, uint32_t c, std::string f = "")
            : line(l), column(c), file(std::move(f)) {}

        inline bool valid() const {
            return line != 0 || column != 0 || !file.empty();
        }

        [[nodiscard]]
        std::string format() const {
            std::string result;
            
            if (!file.empty()) {
                result = file;
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

    struct ParserResult {
        BlockPtr ast;
        std::vector<ImportDecl> imports;
        std::vector<ExportDecl> exports;
    };

    struct SourceFile {
        fs::path    path;       // absolute path
        std::string rel_path;   // relative to project root, for diagnostics
        std::string source;     // file contents
    };

    struct Module {
        SourceFile source_file;
        ParserResult parsed; // AST + imports/exports
    };

    namespace config {

    // ============================================
    // Enums and parsers
    // ============================================
    enum class OptimisationLevel { NONE, DEBUG, RELEASE };

    inline OptimisationLevel parse_optimisation_level(const std::string& s) {
        if (s == "none")    return OptimisationLevel::NONE;
        if (s == "debug")   return OptimisationLevel::DEBUG;
        if (s == "release") return OptimisationLevel::RELEASE;
        return OptimisationLevel::DEBUG;
    }

    enum class WarningLevel { WARN, IGNORE, ERROR };

    inline WarningLevel parse_warning_level(const std::string& s) {
        if (s == "ignore") return WarningLevel::IGNORE;
        if (s == "warn")   return WarningLevel::WARN;
        if (s == "error")  return WarningLevel::ERROR;
        return WarningLevel::WARN;
    }

    enum class Command { BUILD, CHECK, RUN, INIT, HELP, VERSION };

    // ============================================
    // Config sections (can come from TOML, CLI, or both)
    // ============================================

    // Only from xenon.toml - CLI never sets these
    struct ProjectMetadata {
        std::string name;
        std::string version;
        std::string entry;                  // relative to project root
        std::vector<std::string> authors;
    };

    // Can come from both TOML and CLI
    struct BuildConfig {
        fs::path output_dir = "build/";
        std::string output_name;            // binary name (derived from project.name if empty)
        std::string target_triple = "x86_64-linux";
        OptimisationLevel opt_level = OptimisationLevel::DEBUG;
    };

    // Can come from both TOML and CLI
    struct WarningConfig {
        bool warnings_as_errors = false;    // -Werror or [pedantic] in TOML
        std::unordered_map<std::string, WarningLevel> warnings = {
            {"dangling_ref",      WarningLevel::ERROR},
            {"unused_var",        WarningLevel::WARN},
            {"dead_code",         WarningLevel::WARN},
            {"unreachable_code",  WarningLevel::WARN}
        };
    };

    // xenon.toml
    struct ConfigFile {
        ProjectMetadata project;
        BuildConfig build;
        WarningConfig warnings;
        bool pedantic = false;              // if true, all warnings are treated as errors

        // Derived paths (resolved during loading)
        fs::path project_root;
        fs::path entry_path;                // absolute path to entry file
    };


    // Everything the driver needs to know about the project, after merging config file and CLI options
    struct CompilerOpts {
        // ---- Command ----
        Command command = Command::BUILD;
        
        // ---- Sections ----
        ProjectMetadata project;
        BuildConfig build;
        WarningConfig warnings;
        
        // ---- Derived paths (resolved during loading) ----
        fs::path project_root;
        fs::path entry_path;                // absolute path to entry file
        
        // ---- Debug/dev (CLI only) ----
        bool dump_tokens = false;
        bool dump_ast = false;
        
        // ---- Output (CLI only) ----
        bool no_colour = false;
        bool verbose = false;
    };

    } // namespace config

} // namespace xenon
