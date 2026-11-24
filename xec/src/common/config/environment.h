#pragma once

#include "common/config/config_error.h"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace xenon::config {

    namespace fs = std::filesystem;

    // One entry of an environment's `required_functions`, for example
    //   "main::main(usize, *str) -> i32"      or      "main::setup()"
    // The return type is optional; if omitted it is `void`.
    //
    // An entry is the WIDEST signature the environment will call. A program's
    // function may omit trailing parameters (`main()` and `main(usize)` both
    // satisfy the entry above) but may not retype, reorder or add any, and the
    // return type must match exactly. This prefix rule is applied by semantic
    // analysis, not here.
    //
    // Types are kept as text (whitespace-collapsed). Semantic analysis checks them
    // against the program by parsing them with the normal Xenon type parser, so
    // this struct never has to understand types.
    struct RequiredFunction {
        std::string text;                          // as written, trimmed
        std::vector<std::string> module_path;      // {"main"}; empty for a global-scope function
        std::string name;                          // "main"
        std::vector<std::string> parameter_types;  // {} for "()"
        std::string return_type = "void";
    };

    // Parses one required-function signature. Returns nullopt on a malformed
    // string and, if `error` is non-null, stores the reason there.
    std::optional<RequiredFunction> parse_required_function(std::string_view text, std::string* error = nullptr);

    struct Target {
        std::string name;                          // the key under [targets.<name>]
        std::string llvm_triple;
        unsigned pointer_width = 0;                // bits: 16, 32 or 64

        fs::path runtime;                          // absolute; the target's runtime.xe
        fs::path c_source;                         // absolute; the C runtime source

        std::optional<std::string> cpu;
        std::optional<fs::path> linker_script;     // absolute

        // Metadata only in v1: stored, merged and never checked.
        std::map<std::string, bool> capabilities;
        std::set<std::string> configurable;
    };

    struct Environment {
        std::string name;                          // equals the environment directory name
        std::map<std::string, Target> targets;

        // Metadata only in v1: stored, merged and never checked.
        std::map<std::string, bool> capabilities;

        std::vector<RequiredFunction> required_functions;
    };

    // Loads `<environment_dir>/environment.toml`. Paths inside it are relative to
    // `environment_dir` and are returned absolute. The runtime, C source and linker
    // script files must exist. Throws ConfigError otherwise.
    Environment load_environment(const fs::path& environment_dir);

} // namespace xenon::config
