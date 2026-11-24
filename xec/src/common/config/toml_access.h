#pragma once

// INTERNAL: typed access helpers shared by the config loaders (xenon.toml,
// environment.toml, stdlib module.toml). This header includes toml++, so only
// include it from .cpp files. Nothing outside common/config sees toml++.
//
// Requires toml++ v3 with exceptions enabled (the default).

#if __has_include(<toml++/toml.hpp>)
#include <toml++/toml.hpp>
#else
#include <toml++/toml.h>
#endif

#include "common/config/config_error.h"

#include <filesystem>
#include <format>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace xenon::config::detail {

    namespace fs = std::filesystem;

    inline std::string_view type_name(const toml::node& node) {
        switch (node.type()) {
            case toml::node_type::table:         return "a table";
            case toml::node_type::array:         return "an array";
            case toml::node_type::string:        return "a string";
            case toml::node_type::integer:       return "an integer";
            case toml::node_type::floating_point: return "a float";
            case toml::node_type::boolean:       return "a boolean";
            default:                             return "a value of another type";
        }
    }

    inline std::string qualified(std::string_view context, std::string_view key) {
        return std::string(context) + std::string(key);
    }

    [[noreturn]] inline void fail(const fs::path& file, std::string message) {
        throw ConfigError(std::move(message), file);
    }

    [[noreturn]] inline void fail_at(const fs::path& file, const toml::source_region& where, std::string message) {
        throw ConfigError(std::move(message), file, where.begin.line, where.begin.column);
    }

    [[noreturn]] inline void fail_at(const fs::path& file, const toml::node& where, std::string message) {
        fail_at(file, where.source(), std::move(message));
    }

    // Parses a TOML file, converting toml++ parse errors into ConfigError with a
    // line and column.
    inline toml::table parse_file(const fs::path& path) {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) {
            fail(path, "file not found");
        }
        try {
            return toml::parse_file(path.string());
        } catch (const toml::parse_error& error) {
            throw ConfigError(std::string(error.description()), path,
                              error.source().begin.line, error.source().begin.column);
        }
    }

    inline void reject_unknown_keys(const toml::table& table,
                                    std::initializer_list<std::string_view> allowed,
                                    const fs::path& file,
                                    std::string_view context) {
        for (auto&& [key, value] : table) {
            (void)value;
            const std::string_view name = key.str();
            bool known = false;
            for (const std::string_view candidate : allowed) {
                if (candidate == name) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                fail_at(file, key.source(), std::format("unknown key '{}'", qualified(context, name)));
            }
        }
    }

    inline const toml::table* optional_table(const toml::table& parent, std::string_view key,
                                             const fs::path& file, std::string_view context) {
        const toml::node* node = parent.get(key);
        if (node == nullptr) {
            return nullptr;
        }
        const toml::table* table = node->as_table();
        if (table == nullptr) {
            fail_at(file, *node, std::format("'{}' must be a table, found {}",
                                             qualified(context, key), type_name(*node)));
        }
        return table;
    }

    inline std::optional<std::string> optional_string(const toml::table& parent, std::string_view key,
                                                      const fs::path& file, std::string_view context) {
        const toml::node* node = parent.get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto* value = node->as_string();
        if (value == nullptr) {
            fail_at(file, *node, std::format("'{}' must be a string, found {}",
                                             qualified(context, key), type_name(*node)));
        }
        return value->get();
    }

    inline std::string require_string(const toml::table& parent, std::string_view key,
                                      const fs::path& file, std::string_view context) {
        auto value = optional_string(parent, key, file, context);
        if (!value) {
            fail_at(file, parent, std::format("missing required key '{}'", qualified(context, key)));
        }
        return *value;
    }

    // Like optional_string, but the value must not be empty.
    inline std::optional<std::string> optional_nonempty_string(const toml::table& parent, std::string_view key,
                                                               const fs::path& file, std::string_view context) {
        auto value = optional_string(parent, key, file, context);
        if (value && value->empty()) {
            fail_at(file, *parent.get(key), std::format("'{}' must not be empty", qualified(context, key)));
        }
        return value;
    }

    inline std::string require_nonempty_string(const toml::table& parent, std::string_view key,
                                               const fs::path& file, std::string_view context) {
        auto value = optional_nonempty_string(parent, key, file, context);
        if (!value) {
            fail_at(file, parent, std::format("missing required key '{}'", qualified(context, key)));
        }
        return *value;
    }

    inline std::optional<std::int64_t> optional_integer(const toml::table& parent, std::string_view key,
                                                        const fs::path& file, std::string_view context) {
        const toml::node* node = parent.get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto* value = node->as_integer();
        if (value == nullptr) {
            fail_at(file, *node, std::format("'{}' must be an integer, found {}",
                                             qualified(context, key), type_name(*node)));
        }
        return value->get();
    }

    inline std::optional<std::vector<std::string>> optional_string_array(
        const toml::table& parent, std::string_view key, const fs::path& file, std::string_view context) {
        const toml::node* node = parent.get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const toml::array* array = node->as_array();
        if (array == nullptr) {
            fail_at(file, *node, std::format("'{}' must be an array of strings, found {}",
                                             qualified(context, key), type_name(*node)));
        }
        std::vector<std::string> out;
        out.reserve(array->size());
        for (const toml::node& element : *array) {
            const auto* value = element.as_string();
            if (value == nullptr) {
                fail_at(file, element, std::format("every entry of '{}' must be a string, found {}",
                                                   qualified(context, key), type_name(element)));
            }
            out.push_back(value->get());
        }
        return out;
    }

    // A table whose values must all be booleans (capabilities).
    inline std::map<std::string, bool> read_bool_table(const toml::table& table, const fs::path& file,
                                                       std::string_view context) {
        std::map<std::string, bool> out;
        for (auto&& [key, value] : table) {
            const auto* flag = value.as_boolean();
            if (flag == nullptr) {
                fail_at(file, value, std::format("'{}' must be true or false, found {}",
                                                 qualified(context, key.str()), type_name(value)));
            }
            out[std::string(key.str())] = flag->get();
        }
        return out;
    }

} // namespace xenon::config::detail
