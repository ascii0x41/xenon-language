#include "toml/toml_parser.h"
#include "common/diagnostics.h"

#include <fstream>
#include <functional>
#include <sstream>
#include <format>

#include <toml++/toml.h>

namespace xenon::toml {
    
    // Helper function to flatten TOML table
    static TOMLResult flatten_toml_table(const ::toml::table& tbl) {
        try {
            TOMLMap result;

            // Recursively convert TOML table to flat key-value map
            std::function<void(const ::toml::table&, const std::string&)> flatten =
                [&](const ::toml::table& table, const std::string& prefix) {
                    for (auto& [key, node] : table) {
                        std::string full_key = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);
                        
                        if (node.is_table()) {
                            flatten(*node.as_table(), full_key);
                        } else if (node.is_array()) {
                            // Convert array to string representation
                            std::string array_str = "[";
                            auto arr = node.as_array();
                            for (size_t i = 0; i < arr->size(); ++i) {
                                if (i > 0) array_str += ", ";
                                auto& elem = (*arr)[i];
                                if (elem.is_string()) {
                                    array_str += "\"" + std::string(elem.as_string()->get()) + "\"";
                                } else if (elem.is_integer()) {
                                    array_str += std::to_string(elem.as_integer()->get());
                                } else if (elem.is_floating_point()) {
                                    array_str += std::to_string(elem.as_floating_point()->get());
                                } else if (elem.is_boolean()) {
                                    array_str += elem.as_boolean()->get() ? "true" : "false";
                                }
                            }
                            array_str += "]";
                            result[full_key] = array_str;
                        } else if (node.is_string()) {
                            result[full_key] = std::string(node.as_string()->get());
                        } else if (node.is_integer()) {
                            result[full_key] = std::to_string(node.as_integer()->get());
                        } else if (node.is_floating_point()) {
                            result[full_key] = std::to_string(node.as_floating_point()->get());
                        } else if (node.is_boolean()) {
                            result[full_key] = node.as_boolean()->get() ? "true" : "false";
                        }
                    }
                };

            flatten(tbl, "");
            return result;
        } catch (const ::toml::parse_error& e) {
            // Re-throw to be caught by caller
            throw;
        }
    }

    TOMLResult parse_toml_file(const fs::path& path) {
        try {
            auto tbl = ::toml::parse_file(path.string());
            return flatten_toml_table(tbl);
        } catch (const ::toml::parse_error& e) {
            g_diagnostics.error(std::format("{}:{}: {}", path.string(), e.source().begin.line, e.description()));
            return std::nullopt;
        }
    }

    TOMLResult parse_toml_string(const std::string& source) {
        try {
            auto tbl = ::toml::parse(source);
            return flatten_toml_table(tbl);
        } catch (const ::toml::parse_error& e) {
            g_diagnostics.error(std::format("Parse error: {}", e.description()));
            return std::nullopt;
        }
    }

} // namespace xenon::toml