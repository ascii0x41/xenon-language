#include "common/config/environment.h"

#include "common/config/toml_access.h"

#include <cctype>

namespace xenon::config {

    namespace {

        std::string_view trim(std::string_view text) {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
                text.remove_prefix(1);
            }
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
                text.remove_suffix(1);
            }
            return text;
        }

        // Trims and collapses every run of whitespace into a single space.
        std::string collapse_whitespace(std::string_view text) {
            std::string out;
            bool pending_space = false;
            for (const char c : trim(text)) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    pending_space = true;
                    continue;
                }
                if (pending_space) {
                    out += ' ';
                    pending_space = false;
                }
                out += c;
            }
            return out;
        }

        bool is_identifier(std::string_view text) {
            if (text.empty()) {
                return false;
            }
            const auto first = static_cast<unsigned char>(text.front());
            if (!std::isalpha(first) && first != '_') {
                return false;
            }
            for (const char c : text) {
                const auto u = static_cast<unsigned char>(c);
                if (!std::isalnum(u) && u != '_') {
                    return false;
                }
            }
            return true;
        }

        std::string directory_name(const fs::path& directory) {
            fs::path path = directory;
            if (!path.has_filename()) {
                path = path.parent_path();
            }
            return path.filename().string();
        }

        // Resolves a path from environment.toml against the environment directory and
        // requires the file to exist.
        fs::path existing_file(const fs::path& environment_dir, const std::string& relative,
                               const toml::node& where, std::string_view key,
                               const fs::path& file) {
            const fs::path resolved = (environment_dir / relative).lexically_normal();
            std::error_code ec;
            if (!fs::is_regular_file(resolved, ec)) {
                detail::fail_at(file, where,
                                std::format("'{}' not found: {} (is the installation incomplete?)",
                                            key, resolved.string()));
            }
            return resolved;
        }

        Target load_target(const toml::table& table, const std::string& name,
                           const fs::path& environment_dir, const fs::path& file) {
            using namespace detail;

            const std::string context = "targets." + name + ".";
            reject_unknown_keys(table,
                                {"llvm_triple", "pointer_width", "runtime", "c_source", "cpu",
                                 "linker_script", "configurable", "capabilities"},
                                file, context);

            Target target;
            target.name = name;
            target.llvm_triple = require_nonempty_string(table, "llvm_triple", file, context);

            const auto width = optional_integer(table, "pointer_width", file, context);
            if (!width) {
                fail_at(file, table, std::format("missing required key '{}pointer_width'", context));
            }
            if (*width != 16 && *width != 32 && *width != 64) {
                fail_at(file, *table.get("pointer_width"),
                        std::format("'{}pointer_width' must be 16, 32 or 64 (bits), found {}", context, *width));
            }
            target.pointer_width = static_cast<unsigned>(*width);

            target.runtime = existing_file(environment_dir, require_nonempty_string(table, "runtime", file, context),
                                           *table.get("runtime"), context + "runtime", file);
            target.c_source = existing_file(environment_dir, require_nonempty_string(table, "c_source", file, context),
                                            *table.get("c_source"), context + "c_source", file);

            target.cpu = optional_nonempty_string(table, "cpu", file, context);
            if (auto script = optional_nonempty_string(table, "linker_script", file, context)) {
                target.linker_script = existing_file(environment_dir, *script, *table.get("linker_script"),
                                                     context + "linker_script", file);
            }

            if (auto configurable = optional_string_array(table, "configurable", file, context)) {
                target.configurable.insert(configurable->begin(), configurable->end());
            }
            if (const toml::table* capabilities = optional_table(table, "capabilities", file, context)) {
                target.capabilities = read_bool_table(*capabilities, file, context + "capabilities.");
            }
            return target;
        }

    } // namespace

    std::optional<RequiredFunction> parse_required_function(std::string_view text, std::string* error) {
        const auto fail = [&](std::string message) -> std::optional<RequiredFunction> {
            if (error != nullptr) {
                *error = std::move(message);
            }
            return std::nullopt;
        };

        const std::string_view full = trim(text);
        const std::size_t open = full.find('(');
        if (open == std::string_view::npos) {
            return fail("expected '(' after the function name");
        }

        // module::path::name
        const std::string_view head = trim(full.substr(0, open));
        if (head.empty()) {
            return fail("missing function name");
        }
        std::vector<std::string> parts;
        std::size_t position = 0;
        while (true) {
            const std::size_t next = head.find("::", position);
            const std::string_view part =
                trim(head.substr(position, next == std::string_view::npos ? std::string_view::npos : next - position));
            if (!is_identifier(part)) {
                return fail(std::format("'{}' is not a valid name", part));
            }
            parts.emplace_back(part);
            if (next == std::string_view::npos) {
                break;
            }
            position = next + 2;
        }

        RequiredFunction function;
        function.text = std::string(full);
        function.name = parts.back();
        parts.pop_back();
        function.module_path = std::move(parts);

        // Find the ')' that matches the '(' (types may contain brackets).
        int depth = 0;
        std::size_t close = std::string_view::npos;
        for (std::size_t i = open; i < full.size(); ++i) {
            if (full[i] == '(' || full[i] == '[') {
                ++depth;
            } else if (full[i] == ')' || full[i] == ']') {
                --depth;
                if (depth == 0) {
                    close = i;
                    break;
                }
            }
        }
        if (close == std::string_view::npos) {
            return fail("missing ')'");
        }

        // Parameter types, split on top-level commas.
        const std::string_view inner = full.substr(open + 1, close - open - 1);
        if (!trim(inner).empty()) {
            int nesting = 0;
            std::size_t start = 0;
            for (std::size_t i = 0; i <= inner.size(); ++i) {
                const bool at_end = i == inner.size();
                if (!at_end) {
                    const char c = inner[i];
                    if (c == '(' || c == '[' || c == '<') {
                        ++nesting;
                    } else if (c == ')' || c == ']' || c == '>') {
                        --nesting;
                    }
                }
                if (at_end || (inner[i] == ',' && nesting == 0)) {
                    const std::string type = collapse_whitespace(inner.substr(start, i - start));
                    if (type.empty()) {
                        return fail("empty parameter type");
                    }
                    function.parameter_types.push_back(type);
                    start = i + 1;
                }
            }
            if (nesting != 0) {
                return fail("unbalanced brackets in the parameter list");
            }
        }

        // Optional "-> type". Omitted means void.
        const std::string_view rest = trim(full.substr(close + 1));
        if (!rest.empty()) {
            if (!rest.starts_with("->")) {
                return fail(std::format("expected '->' or the end of the signature, found '{}'", rest));
            }
            const std::string return_type = collapse_whitespace(rest.substr(2));
            if (return_type.empty()) {
                return fail("missing return type after '->'");
            }
            function.return_type = return_type;
        }

        return function;
    }

    Environment load_environment(const fs::path& environment_dir_in) {
        using namespace detail;

        const fs::path environment_dir = fs::absolute(environment_dir_in).lexically_normal();
        const fs::path file = environment_dir / "environment.toml";
        const toml::table root = parse_file(file);
        reject_unknown_keys(root, {"name", "required_functions", "capabilities", "targets"}, file, "");

        Environment environment;
        environment.name = require_nonempty_string(root, "name", file, "");
        const std::string directory = directory_name(environment_dir);
        if (environment.name != directory) {
            fail_at(file, *root.get("name"),
                    std::format("environment name '{}' does not match its directory name '{}'", environment.name, directory));
        }

        // required_functions
        if (const toml::node* node = root.get("required_functions")) {
            (void)optional_string_array(root, "required_functions", file, "");   // checks every entry is a string
            for (const toml::node& element : *node->as_array()) {
                const std::string& text = element.as_string()->get();
                std::string reason;
                auto function = parse_required_function(text, &reason);
                if (!function) {
                    fail_at(file, element, std::format("invalid required function \"{}\": {}", text, reason));
                }
                environment.required_functions.push_back(std::move(*function));
            }
        }

        // [capabilities]
        if (const toml::table* capabilities = optional_table(root, "capabilities", file, "")) {
            environment.capabilities = read_bool_table(*capabilities, file, "capabilities.");
        }

        // [targets.<name>]
        const toml::table* targets = optional_table(root, "targets", file, "");
        if (targets == nullptr || targets->empty()) {
            fail_at(file, root, "environment defines no targets (add a [targets.<name>] table)");
        }
        for (auto&& [key, value] : *targets) {
            const std::string name(key.str());
            const toml::table* table = value.as_table();
            if (table == nullptr) {
                fail_at(file, value, std::format("'targets.{}' must be a table, found {}", name, type_name(value)));
            }
            environment.targets.emplace(name, load_target(*table, name, environment_dir, file));
        }

        return environment;
    }

} // namespace xenon::config
