#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "semantic/types.h"

namespace xenon::semantic {

struct Scope {
    std::string name;
    Scope* parent_scope = nullptr;
    std::unordered_map<std::string, Symbol*> symbols;
    std::vector<std::unique_ptr<Symbol>> owned_symbols;
    std::unordered_map<std::string, std::unique_ptr<Scope>> child_scopes;

    explicit Scope(std::string name_, Scope* parent = nullptr);
    ~Scope() = default;

    bool add_symbol(Symbol* symbol);
    Symbol* lookup_local(std::string_view name) const;
    Symbol* lookup(std::string_view name) const;
    Scope* add_child(std::string child_name);
    Scope* get_child(std::string_view child_name) const;
    Scope* get_or_create_path(const std::vector<std::string>& parts);
    Symbol* get_qualified(const std::vector<std::string>& parts) const;
};

} // namespace xenon::semantic