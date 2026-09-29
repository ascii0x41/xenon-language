#include "semantic/scope.h"

namespace xenon::semantic {

Scope::Scope(std::string name_, Scope* parent)
    : name(std::move(name_)), parent_scope(parent) {}

bool Scope::add_symbol(Symbol* symbol) {
    if (symbol == nullptr)
        return false;

    auto existing_it = symbols.find(symbol->name);
    if (existing_it == symbols.end()) {
        symbols.emplace(symbol->name, symbol);
        return true;
    }

    Symbol* existing = existing_it->second;
    if (existing->kind == SymbolKind::FUNCTION && symbol->kind == SymbolKind::FUNCTION) {
        auto* first = static_cast<Function*>(existing);
        auto* second = static_cast<Function*>(symbol);

        for (const auto& overload : first->parameters) {
            (void)overload;
        }

        auto variants = std::make_unique<FunctionVariants>(existing->name);
        variants->overloads.push_back(first);
        variants->overloads.push_back(second);

        for (const auto* overload : variants->overloads) {
            if (overload->parameters.size() != second->parameters.size()) {
                continue;
            }

            bool same = true;
            for (size_t i = 0; i < overload->parameters.size(); ++i) {
                if (overload->parameters[i].type != second->parameters[i].type) {
                    same = false;
                    break;
                }
            }

            if (same) {
                return false;
            }
        }

        symbols[symbol->name] = variants.get();
        owned_symbols.push_back(std::move(variants));
        return true;
    }

    if (existing->kind == SymbolKind::FUNCTION_VARIANTS && symbol->kind == SymbolKind::FUNCTION) {
        auto* variants = static_cast<FunctionVariants*>(existing);
        auto* fn = static_cast<Function*>(symbol);

        for (const auto* overload : variants->overloads) {
            if (overload->parameters.size() != fn->parameters.size()) {
                continue;
            }

            bool same = true;
            for (size_t i = 0; i < overload->parameters.size(); ++i) {
                if (overload->parameters[i].type != fn->parameters[i].type) {
                    same = false;
                    break;
                }
            }

            if (same) {
                return false;
            }
        }

        variants->overloads.push_back(fn);
        return true;
    }

    return false;
}

Symbol* Scope::lookup_local(std::string_view symbol_name) const {
    auto it = symbols.find(std::string(symbol_name));
    if (it == symbols.end())
        return nullptr;
    return it->second;
}

Symbol* Scope::lookup(std::string_view symbol_name) const {
    for (const Scope* scope = this; scope != nullptr; scope = scope->parent_scope) {
        if (auto* symbol = scope->lookup_local(symbol_name))
            return symbol;
    }
    return nullptr;
}

Scope* Scope::add_child(std::string child_name) {
    auto it = child_scopes.find(child_name);
    if (it != child_scopes.end())
        return it->second.get();

    auto child = std::make_unique<Scope>(std::move(child_name), this);
    Scope* result = child.get();
    child_scopes.emplace(result->name, std::move(child));
    return result;
}

Scope* Scope::get_child(std::string_view child_name) const {
    auto it = child_scopes.find(std::string(child_name));
    if (it == child_scopes.end())
        return nullptr;
    return it->second.get();
}

Scope* Scope::get_or_create_path(const std::vector<std::string>& parts) {
    Scope* scope = this;
    for (const auto& part : parts)
        scope = scope->add_child(part);
    return scope;
}

Symbol* Scope::get_qualified(const std::vector<std::string>& parts) const {
    if (parts.empty())
        return nullptr;

    const Scope* scope = this;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        scope = scope->get_child(parts[i]);
        if (scope == nullptr)
            return nullptr;
    }

    return scope->lookup_local(parts.back());
}

} // namespace xenon::semantic
