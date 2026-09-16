#include "semantic/analyser.h"
#include "common/diagnostics.h"

namespace xenon::semantic {

    SemanticAnalyser::SemanticAnalyser(const driver::ModuleNamespaceTree& namespace_tree, const config::CompilerConfig& options)
        : namespace_tree_(namespace_tree), options_(options), type_registry_(), global_scope_("global") {
        (void)namespace_tree;

        const std::string& triple = options.target_triple;
        if (triple.find("64") != std::string::npos
            || triple.find("aarch64") != std::string::npos
            || triple.find("arm64") != std::string::npos
            || triple.find("wasm64") != std::string::npos) {
            WORD_SIZE = 8;
            WORD_ALIGN = 8;
        } else if (triple.find("32") != std::string::npos
                || triple.find("i386") != std::string::npos
                || triple.find("i686") != std::string::npos
                || triple.find("wasm32") != std::string::npos) {
            WORD_SIZE = 4;
            WORD_ALIGN = 4;
        } else {
            warn(std::format("Unrecognized target triple '{}', defaulting to 64-bit word size and alignment", triple),
                SourceLocation{0, 0, "xec"});
            WORD_SIZE = 8;
            WORD_ALIGN = 8;
        }

        type_registry_.initialise_builtin_types(global_scope_);
        current_scope_ = &global_scope_;
    }

    Scope* SemanticAnalyser::enter_scope(const std::string& name) {
        Scope* previous_scope = current_scope_;
        current_scope_ = current_scope_->add_child(name);
        return previous_scope;
    }

    void SemanticAnalyser::leave_scope(Scope* previous_scope) {
        current_scope_ = previous_scope;
    }

    void SemanticAnalyser::warn(const std::string& message, const SourceLocation& loc) const {
        if (options_.warning_level == WarningLevel::ERROR) {
            g_diagnostics.error(message, loc);
        } else {
            g_diagnostics.warning(message, loc);
        }
    }

    void SemanticAnalyser::error(const std::string& message, const SourceLocation& loc) const {
        g_diagnostics.error(message, loc);
    }

    namespace {
        Symbol* resolve_name_reference(Scope* current_scope, Scope* global_scope,
            const std::vector<std::string>& parts, bool is_global) {
            if (parts.empty()) {
                return nullptr;
            }

            if (parts.size() == 1) {
                if (current_scope != nullptr) {
                    if (Symbol* symbol = current_scope->lookup(parts[0])) {
                        return symbol;
                    }
                }
                if (global_scope != nullptr) {
                    return global_scope->lookup(parts[0]);
                }
                return nullptr;
            }

            if (is_global && global_scope != nullptr) {
                return global_scope->get_qualified(parts);
            }

            if (current_scope != nullptr) {
                if (Symbol* symbol = current_scope->get_qualified(parts)) {
                    return symbol;
                }
            }

            if (global_scope != nullptr) {
                return global_scope->get_qualified(parts);
            }
            return nullptr;
        }

        // Maps a binary AST operator onto the overloadable OperatorKind used
        // to search a type's `operators` table. Only the kinds that are
        // actually overloadable reach here (LOGICAL_AND/LOGICAL_OR are
        // handled separately, since types.h marks them as not overloadable).
        OperatorKind to_operator_kind(ast::BinaryOperatorKind op) {
            switch (op) {
                case ast::BinaryOperatorKind::ADD:            return OperatorKind::ADD;
                case ast::BinaryOperatorKind::SUBTRACT:       return OperatorKind::SUB;
                case ast::BinaryOperatorKind::MULTIPLY:       return OperatorKind::MUL;
                case ast::BinaryOperatorKind::DIVIDE:         return OperatorKind::DIV;
                case ast::BinaryOperatorKind::MODULO:         return OperatorKind::MOD;
                case ast::BinaryOperatorKind::BITWISE_AND:    return OperatorKind::BADD;
                case ast::BinaryOperatorKind::BITWISE_OR:     return OperatorKind::BOR;
                case ast::BinaryOperatorKind::BITWISE_XOR:    return OperatorKind::BXOR;
                case ast::BinaryOperatorKind::SHIFT_LEFT:     return OperatorKind::BSHFTL;
                case ast::BinaryOperatorKind::SHIFT_RIGHT:    return OperatorKind::BSHFTR;
                case ast::BinaryOperatorKind::EQUAL:          return OperatorKind::EQ;
                case ast::BinaryOperatorKind::NOT_EQUAL:      return OperatorKind::NEQ;
                case ast::BinaryOperatorKind::LESS_THAN:      return OperatorKind::LT;
                case ast::BinaryOperatorKind::LESS_EQUAL:     return OperatorKind::LTE;
                case ast::BinaryOperatorKind::GREATER_THAN:   return OperatorKind::GT;
                case ast::BinaryOperatorKind::GREATER_EQUAL:  return OperatorKind::GTE;
                default:
                    unreachable();
            }
        }

        const char* binary_operator_symbol(ast::BinaryOperatorKind op) {
            switch (op) {
                case ast::BinaryOperatorKind::ADD:            return "+";
                case ast::BinaryOperatorKind::SUBTRACT:       return "-";
                case ast::BinaryOperatorKind::MULTIPLY:       return "*";
                case ast::BinaryOperatorKind::DIVIDE:         return "/";
                case ast::BinaryOperatorKind::MODULO:         return "%";
                case ast::BinaryOperatorKind::BITWISE_AND:    return "&";
                case ast::BinaryOperatorKind::BITWISE_OR:     return "|";
                case ast::BinaryOperatorKind::BITWISE_XOR:    return "^";
                case ast::BinaryOperatorKind::SHIFT_LEFT:     return "<<";
                case ast::BinaryOperatorKind::SHIFT_RIGHT:    return ">>";
                case ast::BinaryOperatorKind::EQUAL:          return "==";
                case ast::BinaryOperatorKind::NOT_EQUAL:      return "!=";
                case ast::BinaryOperatorKind::LESS_THAN:      return "<";
                case ast::BinaryOperatorKind::LESS_EQUAL:     return "<=";
                case ast::BinaryOperatorKind::GREATER_THAN:   return ">";
                case ast::BinaryOperatorKind::GREATER_EQUAL:  return ">=";
                case ast::BinaryOperatorKind::LOGICAL_AND:    return "&&";
                case ast::BinaryOperatorKind::LOGICAL_OR:     return "||";
                default:
                    unreachable();
            }
        }

        // Maps an OperatorOverloadDecl's lexeme + real operand count
        // (excluding 'self') onto the overloadable OperatorKind it
        // implements. Some lexemes are ambiguous without the arity ('-' is
        // both NEGATE and SUBTRACT), which is exactly why arity is part of
        // the lookup. Compound-assignment lexemes ("+=" etc.) aren't
        // supported here - see the accompanying notes.
        std::optional<OperatorKind> operator_kind_from_lexeme(const std::string& lexeme, size_t operand_count) {
            if (operand_count == 1) {
                if (lexeme == "+")  return OperatorKind::ADD;
                if (lexeme == "-")  return OperatorKind::SUB;
                if (lexeme == "*")  return OperatorKind::MUL;
                if (lexeme == "/")  return OperatorKind::DIV;
                if (lexeme == "%")  return OperatorKind::MOD;
                if (lexeme == "==") return OperatorKind::EQ;
                if (lexeme == "!=") return OperatorKind::NEQ;
                if (lexeme == "<")  return OperatorKind::LT;
                if (lexeme == "<=") return OperatorKind::LTE;
                if (lexeme == ">")  return OperatorKind::GT;
                if (lexeme == ">=") return OperatorKind::GTE;
                if (lexeme == "&")  return OperatorKind::BADD;
                if (lexeme == "|")  return OperatorKind::BOR;
                if (lexeme == "^")  return OperatorKind::BXOR;
                if (lexeme == "<<") return OperatorKind::BSHFTL;
                if (lexeme == ">>") return OperatorKind::BSHFTR;
                if (lexeme == "[]") return OperatorKind::IDX;
            } else if (operand_count == 0) {
                if (lexeme == "-") return OperatorKind::NEG;
                if (lexeme == "~") return OperatorKind::BNOT;
            }
            return std::nullopt;
        }

        // For the receiver ('self') parameter specifically, "mutable" means
        // "&mut T" access was declared, not whether the 'self' binding
        // itself could be reassigned (VariableDecl::is_mut) - those are
        // different questions, and get_operator_result_type's tie-break
        // (see above) needs the former to match the built-in convention.
        bool self_parameter_is_mutable(Type* self_type) {
            return self_type != nullptr && self_type->kind == TypeKind::REFERENCE
                && static_cast<ReferenceType*>(self_type)->is_mutable;
        }

        // Shared by validate_class_method/validate_operator_overload: an
        // exact (no-conversion) parameter-type match against an existing
        // overload counts as a duplicate/conflicting declaration; anything
        // else is a legitimately different overload.
        template <typename Overload>
        bool has_matching_signature(const std::vector<Overload>& overloads, const std::vector<Parameter>& params) {
            for (const auto& existing : overloads) {
                if (existing.parameters.size() != params.size()) {
                    continue;
                }
                bool same = true;
                for (size_t i = 0; i < params.size(); ++i) {
                    if (existing.parameters[i].type != params[i].type) {
                        same = false;
                        break;
                    }
                }
                if (same) {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    Type* SemanticAnalyser::resolve_type_expression(const ast::TypeExprPtr& type_expr) {
        if (!type_expr) {
            // No annotation was written (e.g. `let x = ...;`) — the caller
            // is expected to infer the type from the initialiser instead.
            return nullptr;
        }

        switch (type_expr->kind) {
            case ast::ASTNode::NodeKind::NAMED_TYPE: {
                const auto* named = static_cast<const ast::NamedTypeExpr*>(type_expr.get());

                std::vector<std::string> parts;
                for (const ast::Name* part = named->name.get(); part != nullptr; part = part->next.get())
                    parts.push_back(part->identifier);

                Symbol* symbol = resolve_name_reference(current_scope_, &global_scope_, parts, named->name->is_global);

                if (symbol == nullptr) {
                    error(std::format("Unknown type '{}'", named->name->to_string()), type_expr->location);
                    return type_registry_.get_error_type();
                }

                if (symbol->kind != SymbolKind::TYPE) {
                    error(std::format("'{}' is not a type", named->name->to_string()), type_expr->location);
                    return type_registry_.get_error_type();
                }

                return static_cast<Type*>(symbol);
            }
            case ast::ASTNode::NodeKind::POINTER_TYPE: {
                const auto* ptr_expr = static_cast<const ast::PointerTypeExpr*>(type_expr.get());

                // TODO: `is_box` isn't represented distinctly by PointerType
                // yet, so box T and ptr T resolve to the same pointer type
                // for now.
                Type* pointee = resolve_type_expression(ptr_expr->element_type);
                if (!pointee || pointee->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                if (Type* cached = type_registry_.get_pointer(pointee, ptr_expr->is_mut)) {
                    return cached;
                }
                return type_registry_.register_pointer(pointee, ptr_expr->is_mut);
            }
            case ast::ASTNode::NodeKind::REFERENCE_TYPE: {
                const auto* ref_expr = static_cast<const ast::ReferenceTypeExpr*>(type_expr.get());

                Type* referent = resolve_type_expression(ref_expr->element_type);
                if (!referent || referent->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                if (Type* cached = type_registry_.get_reference(referent, ref_expr->is_mut)) {
                    return cached;
                }
                return type_registry_.register_reference(referent, ref_expr->is_mut);
            }
            case ast::ASTNode::NodeKind::ARRAY_TYPE: {
                const auto* arr_expr = static_cast<const ast::ArrayTypeExpr*>(type_expr.get());

                Type* element = resolve_type_expression(arr_expr->element_type);
                if (!element || element->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                if (arr_expr->size_expr == nullptr) {
                    if (Type* cached = type_registry_.get_array(element, std::nullopt)) {
                        return cached;
                    }
                    return type_registry_.register_array_type(element, std::nullopt);
                }

                // No constant-expression evaluator yet, so a fixed-length
                // array's size has to be written as a literal integer.
                if (arr_expr->size_expr->kind != ast::ASTNode::NodeKind::LITERAL_INT) {
                    error("Array size must be a constant integer expression", arr_expr->size_expr->location);
                    return type_registry_.get_error_type();
                }

                const auto* size_literal = static_cast<const ast::LiteralInt*>(arr_expr->size_expr.get());
                const std::string& raw = size_literal->value;

                unsigned length = 0;
                try {
                    length = (raw.size() > 2 && raw[0] == '0' && (raw[1] == 'b' || raw[1] == 'B'))
                        ? static_cast<unsigned>(std::stoul(raw.substr(2), nullptr, 2))
                        : static_cast<unsigned>(std::stoul(raw, nullptr, 0));
                } catch (const std::exception&) {
                    error(std::format("Invalid array size '{}'", raw), size_literal->location);
                    return type_registry_.get_error_type();
                }

                if (Type* cached = type_registry_.get_array(element, length)) {
                    return cached;
                }
                return type_registry_.register_array_type(element, length);
            }
            default:
                unreachable();
        }
    }

    Type* SemanticAnalyser::get_operator_result_type(const Type* type, OperatorKind op_kind, const std::vector<const Type*>& arg_types, const SourceLocation& loc) {
        auto it = type->operators.find(op_kind);
        if (it == type->operators.end() || it->second.empty()) {
            return nullptr;
        }

        // Every registered operator's first parameter is the implicit
        // `this`, so a real (non-"this") argument list of size N matches
        // an overload with N + 1 parameters.
        struct Candidate {
            const Operator* op;
            int exact_matches;
        };

        std::vector<Candidate> viable;

        for (const auto& overload : it->second) {
            if (overload.parameters.empty() || overload.parameters.size() - 1 != arg_types.size()) {
                continue;
            }

            bool ok = true;
            int exact_matches = 0;

            for (size_t i = 0; i < arg_types.size(); ++i) {
                const Type* param_type = overload.parameters[i + 1].type;
                if (arg_types[i] == param_type) {
                    ++exact_matches;
                } else if (!type_registry_.can_implicitly_convert(arg_types[i], param_type)) {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                viable.push_back({ &overload, exact_matches });
            }
        }

        if (viable.empty()) {
            return nullptr;
        }

        // Prefer whichever candidate(s) matched the most parameters exactly
        // (no implicit conversion needed) over ones that only matched via
        // implicit conversion.
        int best_score = -1;
        for (const auto& candidate : viable) {
            best_score = std::max(best_score, candidate.exact_matches);
        }

        std::vector<const Operator*> best;
        for (const auto& candidate : viable) {
            if (candidate.exact_matches == best_score) {
                best.push_back(candidate.op);
            }
        }

        if (best.size() == 1) {
            return best.front()->return_type;
        }

        // We don't track whether `this` is being used in a mutable context
        // yet, so a tie between overloads that differ only in the
        // mutability of `this` (e.g. the built-in array/string index
        // operators, which register both a const and a mut overload) can't
        // be broken on real usage information. Default to the immutable one.
        std::vector<const Operator*> immutable_this;
        for (const auto* candidate : best) {
            if (!candidate->parameters[0].is_mutable) {
                immutable_this.push_back(candidate);
            }
        }

        if (immutable_this.size() == 1) {
            return immutable_this.front()->return_type;
        }

        error(std::format("Ambiguous overload found for operator on type '{}'", type->name), loc);
        return type_registry_.get_error_type();
    }

    Type* SemanticAnalyser::get_member_type(Type* type, const std::string& member_name, const SourceLocation& loc) {
        for (const auto& field : type->fields) {
            if (field.name == member_name) {
                return field.type;
            }
        }

        // Methods aren't representable as first-class values yet (there's
        // no callable Type in this system), so a bare `obj.method` without
        // a call can't resolve to a Type here. CALL_EXPR will need to
        // special-case a MemberAccessExpr callee and look methods up
        // directly once overload resolution against argument types exists.
        if (type->methods.find(member_name) != type->methods.end()) {
            error(std::format("'{}' cannot be used as a value; call it instead", member_name), loc);
            return type_registry_.get_error_type();
        }

        error(std::format("Type '{}' has no member '{}'", type->name, member_name), loc);
        return type_registry_.get_error_type();
    }

    bool SemanticAnalyser::is_assignable_expression(const ast::Expression* expr, Type*& out_type, bool& is_mutable) {
        if (!expr) {
            out_type = type_registry_.get_error_type();
            is_mutable = false;
            return false;
        }

        switch (expr->kind) {
            case ast::ASTNode::NodeKind::NAME: {
                const auto* name_expr = static_cast<const ast::Name*>(expr);
                std::vector<std::string> parts;
                for (const ast::Name* part = name_expr; part != nullptr; part = part->next.get())
                    parts.push_back(part->identifier);

                Symbol* symbol = resolve_name_reference(current_scope_, &global_scope_, parts, name_expr->is_global);
                if (symbol == nullptr) {
                    error(std::format("Unknown symbol '{}'", name_expr->to_string()), expr->location);
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                if (symbol->kind == SymbolKind::VARIABLE) {
                    auto* variable = static_cast<Variable*>(symbol);
                    out_type = variable->type;
                    is_mutable = variable->is_mutable;
                    return true;
                }
                error(std::format("'{}' is not assignable", name_expr->to_string()), expr->location);
                out_type = type_registry_.get_error_type();
                is_mutable = false;
                return false;
            }
            case ast::ASTNode::NodeKind::MEMBER_ACCESS_EXPR: {
                const auto* member_access = static_cast<const ast::MemberAccessExpr*>(expr);
                Type* object_type = evaluate_expression(member_access->object.get());
                if (!object_type || object_type->kind == TypeKind::ERROR) {
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                for (const auto& field : object_type->fields) {
                    if (field.name == member_access->member->identifier) {
                        out_type = field.type;
                        is_mutable = true;
                        return true;
                    }
                }
                error(std::format("Type '{}' has no assignable member '{}'", object_type->name, member_access->member->identifier), expr->location);
                out_type = type_registry_.get_error_type();
                is_mutable = false;
                return false;
            }
            case ast::ASTNode::NodeKind::INDEX_EXPR: {
                const auto* index_expr = static_cast<const ast::IndexExpr*>(expr);
                Type* object_type = evaluate_expression(index_expr->object.get());
                if (!object_type || object_type->kind == TypeKind::ERROR) {
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                Type* index_type = evaluate_expression(index_expr->index.get());
                if (!index_type || index_type->kind == TypeKind::ERROR) {
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                Type* result_type = get_operator_result_type(object_type, OperatorKind::IDX, { index_type }, expr->location);
                if (!result_type) {
                    error(std::format("Type '{}' cannot be indexed for assignment", object_type->name), expr->location);
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                out_type = result_type;
                is_mutable = true;
                return true;
            }
            case ast::ASTNode::NodeKind::UNARY_OP_EXPR: {
                const auto* unary = static_cast<const ast::UnaryOpExpr*>(expr);
                if (unary->op != ast::UnaryOperatorKind::DEREFERENCE) {
                    error("Only pointer dereference is assignable in this form", expr->location);
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                Type* operand_type = evaluate_expression(unary->operand.get());
                if (!operand_type || operand_type->kind == TypeKind::ERROR) {
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                if (operand_type->kind != TypeKind::POINTER) {
                    error(std::format("Cannot dereference non-pointer type '{}' for assignment", operand_type->name), expr->location);
                    out_type = type_registry_.get_error_type();
                    is_mutable = false;
                    return false;
                }
                auto* pointer_type = static_cast<PointerType*>(operand_type);
                out_type = pointer_type->pointee_type;
                is_mutable = pointer_type->is_mutable;
                return true;
            }
            default:
                error("Expression is not assignable", expr->location);
                out_type = type_registry_.get_error_type();
                is_mutable = false;
                return false;
        }
    }

    Type* SemanticAnalyser::resolve_call(const Function& overload, const std::vector<Type*>& argument_types,
            bool skip_first_parameter, const std::string& callee_description, const SourceLocation& loc) {
        FunctionVariants variants(overload.name);
        variants.overloads.push_back(const_cast<Function*>(&overload));
        return resolve_call(variants, argument_types, skip_first_parameter, callee_description, loc);
    }

    Type* SemanticAnalyser::resolve_call(const FunctionVariants& overloads, const std::vector<Type*>& argument_types,
            bool skip_first_parameter, const std::string& callee_description, const SourceLocation& loc) {
        struct Candidate {
            const Function* fn;
            int exact_matches;
            int conversion_cost;
        };

        std::vector<Candidate> viable;

        for (const auto* overload : overloads.overloads) {
            size_t param_offset = skip_first_parameter ? 1 : 0;
            if (overload->parameters.size() < param_offset
                || overload->parameters.size() - param_offset != argument_types.size()) {
                continue;
            }

            bool ok = true;
            int exact_matches = 0;
            int conversion_cost = 0;

            for (size_t i = 0; i < argument_types.size(); ++i) {
                const Type* arg_type = argument_types[i];
                const Type* param_type = overload->parameters[i + param_offset].type;
                if (arg_type == param_type) {
                    ++exact_matches;
                } else {
                    if (!type_registry_.can_implicitly_convert(arg_type, param_type)) {
                        ok = false;
                        break;
                    }
                    const auto size_delta = static_cast<int64_t>(param_type->size.value_or(0)) - static_cast<int64_t>(arg_type->size.value_or(0));
                    conversion_cost += size_delta > 0 ? static_cast<int>(size_delta) : 1;
                }
            }

            if (ok) {
                viable.push_back({ overload, exact_matches, conversion_cost });
            }
        }

        if (viable.empty()) {
            error(
                std::format("No overload of '{}' accepts {} argument(s) of the given types",
                    callee_description, argument_types.size()),
                loc
            );
            return type_registry_.get_error_type();
        }

        int best_exact = -1;
        int best_cost = std::numeric_limits<int>::max();
        for (const auto& candidate : viable) {
            best_exact = std::max(best_exact, candidate.exact_matches);
        }

        std::vector<const Function*> best;
        for (const auto& candidate : viable) {
            if (candidate.exact_matches != best_exact) {
                continue;
            }
            if (candidate.conversion_cost < best_cost) {
                best.clear();
                best.push_back(candidate.fn);
                best_cost = candidate.conversion_cost;
            } else if (candidate.conversion_cost == best_cost) {
                best.push_back(candidate.fn);
            }
        }

        if (best.size() > 1) {
            error(std::format("Ambiguous call to '{}'", callee_description), loc);
            return type_registry_.get_error_type();
        }

        return best.front()->return_type;
    }

    Type* SemanticAnalyser::evaluate_expression(const ast::ASTNode* ast, Type* expected_type) {
        switch (ast->kind) {
            case ast::ASTNode::NodeKind::LITERAL_INT: {
                const ast::LiteralInt* int_literal = static_cast<const ast::LiteralInt*>(ast);
                Type* type;
                switch (int_literal->suffix) {
                    case ast::NumericSuffix::I8: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::I8); break;
                    case ast::NumericSuffix::I16: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::I16); break;
                    case ast::NumericSuffix::I32: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::I32); break;
                    case ast::NumericSuffix::I64: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::I64); break;
                    case ast::NumericSuffix::U8: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::U8); break;
                    case ast::NumericSuffix::U16: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::U16); break;
                    case ast::NumericSuffix::U32: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::U32); break;
                    case ast::NumericSuffix::U64: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::U64); break;
                    case ast::NumericSuffix::SIZE: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::SIZE); break;
                    default: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::I32);
                }
                return type;
            }
            case ast::ASTNode::NodeKind::LITERAL_FLOAT: {
                const ast::LiteralFloat* float_literal = static_cast<const ast::LiteralFloat*>(ast);
                Type* type;
                switch (float_literal->suffix) {
                    case ast::NumericSuffix::F32: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::F32); break;
                    case ast::NumericSuffix::F64: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::F64); break;
                    default: type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::F32);
                }
                return type;
            }
            case ast::ASTNode::NodeKind::LITERAL_STRING: {
                Type* type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::STRING);
                return type;
            }
            case ast::ASTNode::NodeKind::LITERAL_CHAR: {
                Type* type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::CHAR);
                return type;
            }
            case ast::ASTNode::NodeKind::LITERAL_BOOL: {
                Type* type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::BOOL);
                return type;
            }
            case ast::ASTNode::NodeKind::LITERAL_NULLPTR: {
                Type* type = type_registry_.get_null_type();
                return type;
            }
            case ast::ASTNode::NodeKind::LITERAL_ARRAY: {
                const auto* array_literal = static_cast<const ast::LiteralArray*>(ast);

                if (array_literal->elements.empty()) {
                    if (expected_type &&
                        expected_type->kind == TypeKind::ARRAY) {
                        return expected_type;
                    }

                    error(
                        "Cannot infer the type of an empty array literal",
                        ast->location
                    );

                    return type_registry_.get_error_type();
                }

                Type* element_type =
                    evaluate_expression(array_literal->elements[0].get());

                for (const auto& elem : array_literal->elements) {
                    Type* actual_type = evaluate_expression(elem.get());

                    if (actual_type != element_type) {
                        error(
                            std::format(
                                "Array elements have incompatible types (expected '{}', got '{}')",
                                element_type->name,
                                actual_type->name
                            ),
                            elem->location
                        );

                        return type_registry_.get_error_type();
                    }
                }

                return type_registry_.get_array(
                    element_type,
                    static_cast<unsigned>(array_literal->elements.size())
                );
            }
            case ast::ASTNode::NodeKind::LITERAL_CLASS: {
                const auto* literal = static_cast<const ast::LiteralClass*>(ast);

                Type* literal_type = resolve_type_expression(literal->type_expr);
                if (!literal_type || literal_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                const auto& initialisers = literal->args;

                // Values of void cannot be initialised.
                if (literal_type->kind == TypeKind::VOID) {
                    error(
                        std::format("Cannot initialise a value of type '{}'", literal_type->name),
                        literal->location

                    );
                    return type_registry_.get_error_type();
                }

                // Scalar type handling
                if (type_registry_.is_integer(literal_type) ||
                    type_registry_.is_floating(literal_type) ||
                    literal_type == type_registry_.get_builtin_type(
                        BuiltinType::BuiltinKind::CHAR)) {

                    if (initialisers.empty()) {
                        error(
                            std::format(
                                "Missing initialiser for value of type '{}'",
                                literal_type->name
                            ),
                            literal->location
                        );
                        return type_registry_.get_error_type();
                    }

                    if (initialisers.size() > 1) {
                        error(
                            std::format(
                                "Too many initialisers for '{}' (expected 1, got {})",
                                literal_type->name,
                                initialisers.size()
                            ),
                            literal->location
                        );
                        return type_registry_.get_error_type();
                    }

                    Type* initialiser_type = evaluate_expression(initialisers[0].get());

                    // Don't produce a second error if the expression already failed.
                    if (initialiser_type->kind == TypeKind::ERROR) {
                        return type_registry_.get_error_type();
                    }

                    if (!type_registry_.can_implicitly_convert(
                            initialiser_type,
                            literal_type)) {

                        error(
                            std::format(
                                "Incompatible initialiser for '{}' (expected '{}', got '{}')",
                                literal_type->name,
                                literal_type->name,
                                initialiser_type->name
                            ),
                            literal->location
                        );

                        return type_registry_.get_error_type();
                    }

                    return literal_type;
                }

                // Field size checking
                if (initialisers.size() < literal_type->fields.size()) {
                    error(
                        std::format(
                            "Too few initialisers for '{}' (expected {}, got {})",
                            literal_type->name,
                            literal_type->fields.size(),
                            initialisers.size()
                        ),
                        initialisers.front()->location
                    );
                    return type_registry_.get_error_type();
                }

                // Field size checking
                if (initialisers.size() > literal_type->fields.size()) {
                    error(
                        std::format(
                            "Too many initialisers for '{}' (expected {}, got {})",
                            literal_type->name,
                            literal_type->fields.size(),
                            initialisers.size()
                        ),
                        literal->location
                    );
                    return type_registry_.get_error_type();
                }

                // Field type checking
                for (size_t i = 0; i < initialisers.size(); ++i) {
                    const auto& initialiser = initialisers[i];
                    const Field& field = literal_type->fields[i];

                    Type* initialiser_type = evaluate_expression(initialiser.get());

                    // Avoid cascading errors.
                    if (initialiser_type->kind == TypeKind::ERROR) {
                        continue;
                    }

                    if (!type_registry_.can_implicitly_convert(
                            initialiser_type,
                            field.type)) {

                        error(
                            std::format(
                                "Incompatible initialiser for field '{}' (expected '{}', got '{}')",
                                field.name,
                                field.type->name,
                                initialiser_type->name
                            ),
                            initialiser->location
                        );

                        continue;
                    }
                }

                return literal_type;
            }
            case ast::ASTNode::NodeKind::NEW_EXPR: {
                const auto* heap_alloc_literal = static_cast<const ast::NewExpr*>(ast);

                Type* heap_alloc_literal_type = resolve_type_expression(heap_alloc_literal->alloc_type);
                if (!heap_alloc_literal_type || heap_alloc_literal_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                const auto& initialisers = heap_alloc_literal->initialiser_args;

                // Scalar type handling
                if (type_registry_.is_integer(heap_alloc_literal_type) ||
                    type_registry_.is_floating(heap_alloc_literal_type) ||
                    heap_alloc_literal_type == type_registry_.get_builtin_type(
                        BuiltinType::BuiltinKind::CHAR)) {

                    if (initialisers.empty()) {
                        error(
                            std::format(
                                "Missing initialiser for value of type '{}'",
                                heap_alloc_literal_type->name
                            ),
                            heap_alloc_literal->location
                        );
                        return type_registry_.get_error_type();
                    }

                    if (initialisers.size() > 1) {
                        error(
                            std::format(
                                "Too many initialisers for '{}' (expected 1, got {})",
                                heap_alloc_literal_type->name,
                                initialisers.size()
                            ),
                            heap_alloc_literal->location
                        );
                        return type_registry_.get_error_type();
                    }

                    Type* initialiser_type = evaluate_expression(initialisers[0].get());

                    // Don't produce a second error if the expression already failed.
                    if (initialiser_type->kind == TypeKind::ERROR) {
                        return type_registry_.get_error_type();
                    }

                    if (!type_registry_.can_implicitly_convert(
                            initialiser_type,
                            heap_alloc_literal_type)) {

                        error(
                            std::format(
                                "Incompatible initialiser for '{}' (expected '{}', got '{}')",
                                heap_alloc_literal_type->name,
                                heap_alloc_literal_type->name,
                                initialiser_type->name
                            ),
                            heap_alloc_literal->location
                        );

                        return type_registry_.get_error_type();
                    }

                    return heap_alloc_literal_type;
                }

                // Field size checking
                if (initialisers.size() < heap_alloc_literal_type->fields.size()) {
                    error(
                        std::format(
                            "Too few initialisers for '{}' (expected {}, got {})",
                            heap_alloc_literal_type->name,
                            heap_alloc_literal_type->fields.size(),
                            initialisers.size()
                        ),
                        initialisers.front()->location
                    );
                    return type_registry_.get_error_type();
                }

                // Field size checking
                if (initialisers.size() > heap_alloc_literal_type->fields.size()) {
                    error(
                        std::format(
                            "Too many initialisers for '{}' (expected {}, got {})",
                            heap_alloc_literal_type->name,
                            heap_alloc_literal_type->fields.size(),
                            initialisers.size()
                        ),
                        heap_alloc_literal->location
                    );
                    return type_registry_.get_error_type();
                }

                // Field type checking
                for (size_t i = 0; i < initialisers.size(); ++i) {
                    const auto& initialiser = initialisers[i];
                    const Field& field = heap_alloc_literal_type->fields[i];

                    Type* initialiser_type = evaluate_expression(initialiser.get());

                    // Avoid cascading errors.
                    if (initialiser_type->kind == TypeKind::ERROR) {
                        continue;
                    }

                    if (!type_registry_.can_implicitly_convert(
                            initialiser_type,
                            field.type)) {

                        error(
                            std::format(
                                "Incompatible initialiser for field '{}' (expected '{}', got '{}')",
                                field.name,
                                field.type->name,
                                initialiser_type->name
                            ),
                            initialiser->location
                        );

                        continue;
                    }
                }

                return heap_alloc_literal_type;

            }
            case ast::ASTNode::NodeKind::MEMBER_ACCESS_EXPR: {
                const auto* member_access = static_cast<const ast::MemberAccessExpr*>(ast);

                Type* object_type = evaluate_expression(member_access->object.get());
                if (!object_type || object_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                return get_member_type(object_type, member_access->member->identifier, member_access->location);
            }
            case ast::ASTNode::NodeKind::CALL_EXPR: {
                const auto* call_expr = static_cast<const ast::CallExpr*>(ast);

                std::vector<Type*> argument_types;
                argument_types.reserve(call_expr->args.size());
                bool arguments_ok = true;
                for (const auto& arg : call_expr->args) {
                    Type* arg_type = evaluate_expression(arg.get());
                    if (!arg_type || arg_type->kind == TypeKind::ERROR) {
                        arguments_ok = false;
                    }
                    argument_types.push_back(arg_type);
                }

                if (!arguments_ok) {
                    return type_registry_.get_error_type();
                }

                // A function/method symbol is not an ordinary value (see
                // the NAME case above), so the callee is resolved directly
                // here rather than through evaluate_expression.
                if (call_expr->callee->kind == ast::ASTNode::NodeKind::NAME) {
                    const auto* name_expr = static_cast<const ast::Name*>(call_expr->callee.get());

                    std::vector<std::string> parts;
                    for (const ast::Name* part = name_expr; part != nullptr; part = part->next.get())
                        parts.push_back(part->identifier);

                    if (parts.size() > 1) {
                        // A qualified callee might be `Class::static_method`.
                        // Classes are Type symbols, not child Scopes, so
                        // their statics can't be reached through
                        // Scope::get_qualified - check the class's own
                        // static_methods table directly when the qualifying
                        // prefix names a class.
                        std::vector<std::string> prefix(parts.begin(), parts.end() - 1);
                        Symbol* prefix_symbol = prefix.size() == 1
                            ? global_scope_.lookup(prefix[0])
                            : global_scope_.get_qualified(prefix);

                        if (prefix_symbol != nullptr && prefix_symbol->kind == SymbolKind::TYPE) {
                            Type* owner_type = static_cast<Type*>(prefix_symbol);
                            const std::string& member_name = parts.back();

                                    auto method_it = owner_type->static_methods.find(member_name);
                            if (method_it == owner_type->static_methods.end() || method_it->second.empty()) {
                                error(std::format("Type '{}' has no static method '{}'", owner_type->name, member_name), name_expr->location);
                                return type_registry_.get_error_type();
                            }

                            return resolve_call(method_it->second, argument_types, false,
                                std::format("{}::{}", owner_type->name, member_name), call_expr->location);
                        }
                    }

                    Symbol* symbol = resolve_name_reference(current_scope_, &global_scope_, parts, name_expr->is_global);

                    if (symbol == nullptr) {
                        error(std::format("Unknown symbol '{}'", name_expr->to_string()), name_expr->location);
                        return type_registry_.get_error_type();
                    }

                    if (symbol->kind != SymbolKind::FUNCTION && symbol->kind != SymbolKind::FUNCTION_VARIANTS) {
                        error(std::format("'{}' is not callable", name_expr->to_string()), name_expr->location);
                        return type_registry_.get_error_type();
                    }

                    if (symbol->kind == SymbolKind::FUNCTION) {
                        return resolve_call(*static_cast<Function*>(symbol), argument_types, false,
                            name_expr->to_string(), call_expr->location);
                    }

                    return resolve_call(*static_cast<FunctionVariants*>(symbol), argument_types, false,
                        name_expr->to_string(), call_expr->location);
                }

                if (call_expr->callee->kind == ast::ASTNode::NodeKind::MEMBER_ACCESS_EXPR) {
                    const auto* member_access = static_cast<const ast::MemberAccessExpr*>(call_expr->callee.get());

                    Type* object_type = evaluate_expression(member_access->object.get());
                    if (!object_type || object_type->kind == TypeKind::ERROR) {
                        return type_registry_.get_error_type();
                    }

                    const std::string& method_name = member_access->member->identifier;
                    auto method_it = object_type->methods.find(method_name);
                    if (method_it == object_type->methods.end() || method_it->second.empty()) {
                        error(std::format("Type '{}' has no method '{}'", object_type->name, method_name), member_access->location);
                        return type_registry_.get_error_type();
                    }

                    return resolve_call(method_it->second, argument_types, true,
                        std::format("{}::{}", object_type->name, method_name), call_expr->location);
                }

                error("Expression is not callable", call_expr->callee->location);
                return type_registry_.get_error_type();
            }
            case ast::ASTNode::NodeKind::INDEX_EXPR: {
                const auto* idx_expr = static_cast<const ast::IndexExpr*>(ast);

                Type* object_type = evaluate_expression(idx_expr->object.get());
                if (!object_type || object_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                Type* index_type = evaluate_expression(idx_expr->index.get());
                if (!index_type || index_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                Type* result_type = get_operator_result_type(object_type, OperatorKind::IDX, { index_type }, idx_expr->location);
                if (!result_type) {
                    error(std::format("Type '{}' cannot be indexed", object_type->name), idx_expr->location);
                    return type_registry_.get_error_type();
                }

                return result_type;
            }
            case ast::ASTNode::NodeKind::UNARY_OP_EXPR: {
                const auto* unary_op_expr = static_cast<const ast::UnaryOpExpr*>(ast);

                Type* operand_type = evaluate_expression(unary_op_expr->operand.get());
                if (!operand_type || operand_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                Type* result_type = type_registry_.get_error_type();

                switch (unary_op_expr->op) {
                    case ast::UnaryOperatorKind::ADDRESS_OF: {
                        result_type = type_registry_.register_pointer(operand_type, true);
                        break;
                    }
                    case ast::UnaryOperatorKind::DEREFERENCE: {
                        if (operand_type->kind == TypeKind::POINTER) {
                            result_type = static_cast<PointerType*>(operand_type)->pointee_type;
                        } else {
                            error(std::format("Cannot dereference non-pointer type '{}'", operand_type->name), unary_op_expr->location);
                        }
                        break;
                    }
                    case ast::UnaryOperatorKind::LOGICAL_NOT: {
                        Type* bool_type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::BOOL);
                        if (operand_type == bool_type) {
                            result_type = bool_type;
                        } else {
                            error(std::format("Logical NOT requires a bool operand, got '{}'", operand_type->name), unary_op_expr->location);
                        }
                        break;
                    }
                    case ast::UnaryOperatorKind::BITWISE_NOT: {
                        if (type_registry_.is_integer(operand_type) ||
                            operand_type == type_registry_.get_builtin_type(BuiltinType::BuiltinKind::CHAR)) {
                            // Integer/char bitwise is okay
                            result_type = operand_type;
                        } else {
                            Type* op_result = get_operator_result_type(operand_type, OperatorKind::BNOT, {}, unary_op_expr->location);
                            if (op_result) {
                                result_type = op_result;
                            } else {
                                error(std::format("Cannot apply bitwise NOT to type '{}'", operand_type->name), unary_op_expr->location);
                            }
                        }
                        break;
                    }
                    case ast::UnaryOperatorKind::NEGATE: {
                        if (type_registry_.is_builtin_numeric(operand_type)) {
                            // Numeric negation is okay
                            result_type = operand_type;
                        } else {
                            Type* op_result = get_operator_result_type(operand_type, OperatorKind::NEG, {}, unary_op_expr->location);
                            if (op_result) {
                                result_type = op_result;
                            } else {
                                error(std::format("Cannot negate type '{}'", operand_type->name), unary_op_expr->location);
                            }
                        }
                        break;
                    }
                    default:
                        unreachable();
                }

                return result_type;
            }
            case ast::ASTNode::NodeKind::ASSIGNMENT_EXPR: {
                const auto* assign = static_cast<const ast::AssignmentExpr*>(ast);
                Type* lhs_type = nullptr;
                bool lhs_mutable = false;
                if (!is_assignable_expression(assign->lhs.get(), lhs_type, lhs_mutable)) {
                    return type_registry_.get_error_type();
                }

                if (!lhs_mutable) {
                    error(std::format("Cannot assign to immutable value of type '{}'", lhs_type->name), assign->location);
                    return type_registry_.get_error_type();
                }

                Type* rhs_type = evaluate_expression(assign->rhs.get());
                if (!rhs_type || rhs_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                if (!type_registry_.can_implicitly_convert(rhs_type, lhs_type)) {
                    error(std::format("Cannot assign value of type '{}' to an lvalue of type '{}'",
                        rhs_type->name, lhs_type->name), assign->location);
                    return type_registry_.get_error_type();
                }

                if (assign->op == ast::AssignmentOperatorKind::ASSIGN) {
                    return lhs_type;
                }

                Type* compound_result = nullptr;
                switch (assign->op) {
                    case ast::AssignmentOperatorKind::ADD_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::ADD, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::SUBTRACT_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::SUB, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::MULTIPLY_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::MUL, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::DIVIDE_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::DIV, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::MODULO_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::MOD, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::BITWISE_AND_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::BADD, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::BITWISE_OR_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::BOR, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::BITWISE_XOR_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::BXOR, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::SHIFT_LEFT_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::BSHFTL, { rhs_type }, assign->location); break;
                    case ast::AssignmentOperatorKind::SHIFT_RIGHT_ASSIGN: compound_result = get_operator_result_type(lhs_type, OperatorKind::BSHFTR, { rhs_type }, assign->location); break;
                    default: break;
                }

                if (!compound_result || compound_result->kind == TypeKind::ERROR) {
                    error(std::format("Operator '{}' not valid for assignment to type '{}'", static_cast<int>(assign->op), lhs_type->name), assign->location);
                    return type_registry_.get_error_type();
                }
                return lhs_type;
            }
            case ast::ASTNode::NodeKind::BINARY_OP_EXPR: {
                const auto* binary_op_expr = static_cast<const ast::BinaryOpExpr*>(ast);

                Type* lhs_type = evaluate_expression(binary_op_expr->lhs.get());
                if (!lhs_type || lhs_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                Type* rhs_type = evaluate_expression(binary_op_expr->rhs.get());
                if (!rhs_type || rhs_type->kind == TypeKind::ERROR) {
                    return type_registry_.get_error_type();
                }

                Type* result_type = type_registry_.get_error_type();

                switch (binary_op_expr->op) {
                    case ast::BinaryOperatorKind::ADD:
                    case ast::BinaryOperatorKind::SUBTRACT:
                    case ast::BinaryOperatorKind::MULTIPLY:
                    case ast::BinaryOperatorKind::DIVIDE: {
                        if (type_registry_.is_builtin_numeric(lhs_type) && type_registry_.can_implicitly_convert(rhs_type, lhs_type)) {
                            // Numeric arithmetic
                            result_type = lhs_type;
                        } else {
                            Type* op_result = get_operator_result_type(lhs_type, to_operator_kind(binary_op_expr->op), { rhs_type }, binary_op_expr->location);
                            if (op_result) {
                                result_type = op_result;
                            } else {
                                error(
                                    std::format("No operator '{}' found for types '{}' and '{}'",
                                        binary_operator_symbol(binary_op_expr->op), lhs_type->name, rhs_type->name),
                                    binary_op_expr->location
                                );
                            }
                        }
                        break;
                    }
                    case ast::BinaryOperatorKind::MODULO:
                    case ast::BinaryOperatorKind::BITWISE_AND:
                    case ast::BinaryOperatorKind::BITWISE_OR:
                    case ast::BinaryOperatorKind::BITWISE_XOR:
                    case ast::BinaryOperatorKind::SHIFT_LEFT:
                    case ast::BinaryOperatorKind::SHIFT_RIGHT: {
                        if (type_registry_.is_integer(lhs_type) && type_registry_.can_implicitly_convert(rhs_type, lhs_type)) {
                            result_type = lhs_type;
                        } else {
                            Type* op_result = get_operator_result_type(lhs_type, to_operator_kind(binary_op_expr->op), { rhs_type }, binary_op_expr->location);
                            if (op_result) {
                                result_type = op_result;
                            } else {
                                error(
                                    std::format("No operator '{}' found for types '{}' and '{}'",
                                        binary_operator_symbol(binary_op_expr->op), lhs_type->name, rhs_type->name),
                                    binary_op_expr->location
                                );
                            }
                        }
                        break;
                    }
                    case ast::BinaryOperatorKind::LOGICAL_AND:
                    case ast::BinaryOperatorKind::LOGICAL_OR: {
                        // Not overloadable (see types.h), so there's no
                        // operator-lookup fallback here - just bool && bool.
                        Type* bool_type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::BOOL);
                        if (lhs_type == bool_type && rhs_type == bool_type) {
                            result_type = bool_type;
                        } else {
                            error(
                                std::format("Logical '{}' requires bool operands, got '{}' and '{}'",
                                    binary_operator_symbol(binary_op_expr->op), lhs_type->name, rhs_type->name),
                                binary_op_expr->location
                            );
                        }
                        break;
                    }
                    case ast::BinaryOperatorKind::EQUAL:
                    case ast::BinaryOperatorKind::NOT_EQUAL:
                    case ast::BinaryOperatorKind::LESS_THAN:
                    case ast::BinaryOperatorKind::LESS_EQUAL:
                    case ast::BinaryOperatorKind::GREATER_THAN:
                    case ast::BinaryOperatorKind::GREATER_EQUAL: {
                        Type* bool_type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::BOOL);
                        if (type_registry_.is_builtin_numeric(lhs_type) && type_registry_.can_implicitly_convert(rhs_type, lhs_type)) {
                            result_type = bool_type;
                        } else {
                            Type* op_result = get_operator_result_type(lhs_type, to_operator_kind(binary_op_expr->op), { rhs_type }, binary_op_expr->location);
                            if (op_result) {
                                result_type = op_result;
                            } else {
                                error(
                                    std::format("No operator '{}' found for types '{}' and '{}'",
                                        binary_operator_symbol(binary_op_expr->op), lhs_type->name, rhs_type->name),
                                    binary_op_expr->location
                                );
                            }
                        }
                        break;
                    }
                    default:
                        unreachable();
                }
                return result_type;
            }
            case ast::ASTNode::NodeKind::NAME: {
                const auto* name_expr = static_cast<const ast::Name*>(ast);

                std::vector<std::string> parts;
                for (const ast::Name* part = name_expr; part != nullptr; part = part->next.get())
                    parts.push_back(part->identifier);

                Symbol* symbol = resolve_name_reference(current_scope_, &global_scope_, parts, name_expr->is_global);

                if (symbol == nullptr) {
                    error(std::format("Unknown symbol '{}'", name_expr->to_string()), name_expr->location);
                    return type_registry_.get_error_type();
                }

                        if (symbol->kind == SymbolKind::VARIABLE) {
                    return static_cast<Variable*>(symbol)->type;
                }

                if (symbol->kind == SymbolKind::TYPE) {
                    return static_cast<Type*>(symbol);
                }

                if (symbol->kind == SymbolKind::FUNCTION || symbol->kind == SymbolKind::FUNCTION_VARIANTS) {
                    error(std::format("'{}' cannot be used as a value", name_expr->to_string()), name_expr->location);
                    return type_registry_.get_error_type();
                }

                error(std::format("'{}' cannot be used as a value", name_expr->to_string()), name_expr->location);
                return type_registry_.get_error_type();
            }
            default:
                return type_registry_.get_error_type();
        }
    }



    Type* SemanticAnalyser::resolve_let_binding_type(const std::string& name, const ast::TypeExprPtr& type_expr,
            const ast::ExpressionPtr& init_expr, const SourceLocation& loc, const char* what) {
        // resolve_type_expression returns nullptr only when there was no
        // annotation at all (`let x = ...;`); an annotation that failed to
        // resolve (unknown type name, etc.) comes back as get_error_type(),
        // which is non-null - that distinction matters below.
        Type* declared_type = resolve_type_expression(type_expr);

        Type* init_type = nullptr;
        if (init_expr) {
            init_type = evaluate_expression(init_expr.get(), declared_type);
        }

        if (declared_type == nullptr && init_type == nullptr) {
            error(
                std::format("Cannot infer the type of {} '{}' without a type annotation or an initialiser", what, name),
                loc
            );
            return nullptr;
        }

        // Whichever side already produced <error> already reported its own
        // root cause (unknown type, bad expression, etc.) - don't pile a
        // "cannot convert" diagnostic on top of that.
        if ((declared_type && declared_type->kind == TypeKind::ERROR) ||
            (init_type && init_type->kind == TypeKind::ERROR)) {
            return nullptr;
        }

        if (declared_type && init_type) {
            if (!type_registry_.can_implicitly_convert(init_type, declared_type)) {
                error(
                    std::format("Cannot initialise {} '{}' of type '{}' with a value of type '{}'",
                        what, name, declared_type->name, init_type->name),
                    loc
                );
                return nullptr;
            }
            return declared_type;
        }

        // `let x: i32;` - declared but not initialised - or
        // `let x = 5;` - no annotation, infer from the initialiser.
        return declared_type ? declared_type : init_type;
    }

    bool SemanticAnalyser::validate_variable_decl(const ast::VariableDecl* var_decl) {
        Type* final_type = resolve_let_binding_type(var_decl->name, var_decl->type_expr, var_decl->init_expr, var_decl->location, "variable");
        if (final_type == nullptr) {
            return false;
        }

        // Registration pass owns variable binding insertion; validation only
        // checks the declared type/initialiser rules once the symbol is known.
        if (current_scope_->lookup_local(var_decl->name) == nullptr) {
            auto variable = std::make_unique<Variable>(var_decl->name, var_decl, var_decl->is_public, final_type, var_decl->is_mut);
            Variable* variable_ptr = variable.get();
            symbols_.push_back(std::move(variable));

            if (!current_scope_->add_symbol(variable_ptr)) {
                error(
                    std::format("'{}' is already declared in this scope", var_decl->name),
                    var_decl->location
                );
                return false;
            }
        }

        return true;
    }

    ControlFlowResult SemanticAnalyser::validate_statement(const ast::Statement* stmt) {
        switch (stmt->kind) {
            case ast::ASTNode::NodeKind::BLOCK_STMT:
                return validate_block(static_cast<const ast::BlockStmt*>(stmt));

            case ast::ASTNode::NodeKind::IF_STMT:
                return validate_if_statement(static_cast<const ast::IfStmt*>(stmt));

            case ast::ASTNode::NodeKind::WHILE_STMT:
                return validate_while_statement(static_cast<const ast::WhileStmt*>(stmt));

            case ast::ASTNode::NodeKind::RETURN_STMT: {
                const auto* return_stmt = static_cast<const ast::ReturnStmt*>(stmt);

                // current_function_return_type_ is only null outside of any
                // function validation; validate_function always sets it to
                // at least the void type before validating a body.
                bool is_void_function = current_function_return_type_ == nullptr
                    || current_function_return_type_->kind == TypeKind::VOID;

                if (return_stmt->return_value) {
                    Type* return_value_type = evaluate_expression(return_stmt->return_value.get(), current_function_return_type_);

                    if (is_void_function) {
                        error("A void function cannot return a value", return_stmt->location);
                    } else if (return_value_type->kind != TypeKind::ERROR
                        && !type_registry_.can_implicitly_convert(return_value_type, current_function_return_type_)) {
                        error(
                            std::format("Cannot return a value of type '{}' from a function returning '{}'",
                                return_value_type->name, current_function_return_type_->name),
                            return_stmt->location
                        );
                    }
                } else if (!is_void_function) {
                    error(
                        std::format("Missing return value; function returns '{}'", current_function_return_type_->name),
                        return_stmt->location
                    );
                }

                return ControlFlowResult::RETURNS;
            }

            case ast::ASTNode::NodeKind::BREAK_STMT: {
                if (loop_depth_ == 0) {
                    error("'break' used outside of a loop", stmt->location);
                }
                // Temporary handling strategy: break is not a return, full
                // stop. Proper loop-control-flow tracking (e.g. treating a
                // loop as "returning" only if every path either returns or
                // breaks) is future work.
                return ControlFlowResult::FALLS_THROUGH;
            }

            case ast::ASTNode::NodeKind::CONTINUE_STMT: {
                if (loop_depth_ == 0) {
                    error("'continue' used outside of a loop", stmt->location);
                }
                return ControlFlowResult::FALLS_THROUGH;
            }

            case ast::ASTNode::NodeKind::VARIABLE_DECL:
                validate_variable_decl(static_cast<const ast::VariableDecl*>(stmt));
                return ControlFlowResult::FALLS_THROUGH;

            case ast::ASTNode::NodeKind::DELETE_STMT:
                validate_delete(static_cast<const ast::DeleteStmt*>(stmt));
                return ControlFlowResult::FALLS_THROUGH;

            case ast::ASTNode::NodeKind::EXPRESSION_STMT: {
                const auto* expr_stmt = static_cast<const ast::ExpressionStmt*>(stmt);
                evaluate_expression(expr_stmt->expr.get());
                return ControlFlowResult::FALLS_THROUGH;
            }

            default:
                // Other statement kinds (e.g. DELETE_STMT, FOREACH_STMT)
                // have no control-flow significance implemented yet - they
                // simply fall through for now.
                return ControlFlowResult::FALLS_THROUGH;
        }
    }

    ControlFlowResult SemanticAnalyser::validate_block(const ast::BlockStmt* block) {
        Scope* previous_scope = enter_scope(std::format("block@{}:{}", block->location.line, block->location.column));

        ControlFlowResult result = ControlFlowResult::FALLS_THROUGH;

        for (const auto& statement : block->statements) {
            if (validate_statement(statement.get()) == ControlFlowResult::RETURNS) {
                result = ControlFlowResult::RETURNS;
                // Do not overcomplicate unreachable-code analysis yet: once
                // a statement definitely returns, later statements in this
                // block are simply not reachable through normal control
                // flow, so there's nothing further to check here.
                break;
            }
        }

        leave_scope(previous_scope);
        return result;
    }

    ControlFlowResult SemanticAnalyser::validate_if_statement(const ast::IfStmt* if_stmt) {
        Type* condition_type = evaluate_expression(if_stmt->condition.get());
        Type* bool_type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::BOOL);

        if (condition_type->kind != TypeKind::ERROR && condition_type != bool_type) {
            error(
                std::format("'if' condition must be of type 'bool', got '{}'", condition_type->name),
                if_stmt->condition->location
            );
        }

        ControlFlowResult then_result = validate_statement(if_stmt->then_branch.get());

        if (!if_stmt->else_branch) {
            // The condition might be false, so the `if` might fall through
            // no matter what the then-branch does.
            return ControlFlowResult::FALLS_THROUGH;
        }

        ControlFlowResult else_result = validate_statement(if_stmt->else_branch.get());

        if (then_result == ControlFlowResult::RETURNS && else_result == ControlFlowResult::RETURNS) {
            return ControlFlowResult::RETURNS;
        }

        return ControlFlowResult::FALLS_THROUGH;
    }

    ControlFlowResult SemanticAnalyser::validate_while_statement(const ast::WhileStmt* while_stmt) {
        Type* condition_type = evaluate_expression(while_stmt->condition.get());
        Type* bool_type = type_registry_.get_builtin_type(BuiltinType::BuiltinKind::BOOL);

        if (condition_type->kind != TypeKind::ERROR && condition_type != bool_type) {
            error(
                std::format("'while' condition must be of type 'bool', got '{}'", condition_type->name),
                while_stmt->condition->location
            );
        }

        ++loop_depth_;
        validate_statement(while_stmt->body.get());
        --loop_depth_;

        // The body might run zero times, so a `while` never counts as a
        // guaranteed return yet, regardless of what its body does. No
        // constant-condition or infinite-loop analysis for now.
        return ControlFlowResult::FALLS_THROUGH;
    }

    ControlFlowResult SemanticAnalyser::validate_callable_body(const std::string& scope_name,
            const std::vector<ast::VariableDeclPtr>& parameters, Type* return_type,
            const ast::BlockStmt* body, bool& parameters_ok) {
        Scope* previous_scope = enter_scope(scope_name);

        for (const auto& parameter : parameters) {
            if (!validate_variable_decl(parameter.get())) {
                parameters_ok = false;
            }
        }

        Type* previous_function_return_type = current_function_return_type_;
        current_function_return_type_ = return_type;

        ControlFlowResult result = validate_block(body);

        current_function_return_type_ = previous_function_return_type;
        leave_scope(previous_scope);

        return result;
    }

    bool SemanticAnalyser::validate_function(const ast::FunctionDecl& function_decl) {
        Type* return_type = resolve_type_expression(function_decl.return_type);
        if (return_type == nullptr) {
            return_type = type_registry_.get_void_type();
        }

        bool parameters_ok = true;
        std::vector<Parameter> semantic_parameters;
        semantic_parameters.reserve(function_decl.parameters.size());
        for (const auto& param_decl : function_decl.parameters) {
            Type* param_type = resolve_type_expression(param_decl->type_expr);
            if (param_type == nullptr || param_type->kind == TypeKind::ERROR) {
                parameters_ok = false;
            }
            semantic_parameters.emplace_back(param_decl->name, param_type, param_decl->is_mut);
        }

        if (!function_decl.body) {
            return parameters_ok && return_type->kind != TypeKind::ERROR;
        }

        ControlFlowResult result = validate_callable_body(
            std::format("fn {}@{}:{}", function_decl.name, function_decl.location.line, function_decl.location.column),
            function_decl.parameters,
            return_type,
            function_decl.body.get(),
            parameters_ok
        );

        if (!parameters_ok || return_type->kind == TypeKind::ERROR) {
            return false;
        }

        if (return_type->kind != TypeKind::VOID && result != ControlFlowResult::RETURNS) {
            error(
                std::format("Function '{}' does not return a value of type '{}' on every control-flow path",
                    function_decl.name, return_type->name),
                function_decl.location
            );
            return false;
        }

        // The registration pass owns adding this function to the module's
        // symbol table. Validation is only responsible for checking the
        // body/signature semantics after the declaration is already visible.
        if (current_scope_->lookup_local(function_decl.name) == nullptr) {
            auto fn = std::make_unique<Function>(function_decl.name, &function_decl, function_decl.is_public, std::move(semantic_parameters), return_type);
            Function* fn_ptr = fn.get();
            symbols_.push_back(std::move(fn));

            if (!current_scope_->add_symbol(fn_ptr)) {
                error(
                    std::format("'{}' is already declared with this parameter signature", function_decl.name),
                    function_decl.location
                );
                return false;
            }
        }

        return true;
    }

    bool SemanticAnalyser::type_depends_on_by_value(const Type* from, const Type* target, std::unordered_set<const Type*>& visited) const {
        if (from == target) {
            return true;
        }

        if (!visited.insert(from).second) {
            // Already walked this type on this search - either it's part
            // of a different (already-rejected) cycle, or a harmless
            // diamond dependency; either way there's no new ground to
            // cover by walking it again.
            return false;
        }

        if (from->kind == TypeKind::USER_DEFINED) {
            for (const auto& field : from->fields) {
                if (type_depends_on_by_value(field.type, target, visited)) {
                    return true;
                }
            }
        } else if (from->kind == TypeKind::ARRAY) {
            // A fixed-length array embeds its elements inline; an unsized
            // array is a fat-pointer-style descriptor (see ArrayType's own
            // size calculation) and doesn't embed anything by value.
            const auto* array_type = static_cast<const ArrayType*>(from);
            if (array_type->length.has_value() && type_depends_on_by_value(array_type->element_type, target, visited)) {
                return true;
            }
        }

        return false;
    }

    bool SemanticAnalyser::validate_class_structure(const ast::ClassStructureDecl& class_decl) {
        // Pass-1-ish: make sure this class's Type exists before resolving
        // its own fields, so a sibling class's field referencing this one
        // can find it - once a future module-level driver runs a
        // registration pass over every class before a field-resolution
        // pass. Without that driver, a forward reference to a class that
        // hasn't been validated yet still won't resolve; see the
        // accompanying notes.
        Type* class_type = nullptr;
        if (Symbol* existing = current_scope_->lookup_local(class_decl.name)) {
            if (existing->kind != SymbolKind::TYPE) {
                error(std::format("'{}' is already declared and is not a type", class_decl.name), class_decl.location);
                return false;
            }
            class_type = static_cast<Type*>(existing);
        } else {
            auto new_type = std::make_unique<Type>(TypeKind::USER_DEFINED, class_decl.name);
            class_type = type_registry_.register_type(std::move(new_type));
            current_scope_->add_symbol(class_type);
        }

        bool ok = true;

        for (const auto& field_decl : class_decl.fields) {
            Type* field_type = resolve_type_expression(field_decl->type_expr);

            if (field_type == nullptr) {
                error(
                    std::format("Field '{}' of class '{}' must have an explicit type", field_decl->name, class_decl.name),
                    field_decl->location
                );
                ok = false;
                continue;
            }

            if (field_type->kind == TypeKind::ERROR) {
                // resolve_type_expression already reported the root cause.
                ok = false;
                continue;
            }

            if (field_type->kind == TypeKind::VOID) {
                error(
                    std::format("Field '{}' of class '{}' cannot have type 'void'", field_decl->name, class_decl.name),
                    field_decl->location
                );
                ok = false;
                continue;
            }

            std::unordered_set<const Type*> visited;
            if (type_depends_on_by_value(field_type, class_type, visited)) {
                error(
                    std::format("Field '{}' of class '{}' creates a recursive by-value type dependency via '{}'",
                        field_decl->name, class_decl.name, field_type->name),
                    field_decl->location
                );
                ok = false;
                continue;
            }

            bool duplicate = false;
            for (const auto& existing_field : class_type->fields) {
                if (existing_field.name == field_decl->name) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                error(
                    std::format("Field '{}' is already declared in class '{}'", field_decl->name, class_decl.name),
                    field_decl->location
                );
                ok = false;
                continue;
            }

            class_type->add_field(Field(field_decl->name, field_type, field_decl->is_public));
        }

        if (!ok) {
            return false;
        }

        return true;
    }

    bool SemanticAnalyser::validate_recursive_value_layout_cycles() {
        bool ok = true;
        for (Type* type : type_registry_.all_types()) {
            if (type->kind != TypeKind::USER_DEFINED) {
                continue;
            }

            for (const auto& field : type->fields) {
                if (field.type == nullptr) {
                    continue;
                }
                if (field.type->kind == TypeKind::POINTER || field.type->kind == TypeKind::REFERENCE) {
                    continue;
                }

                std::unordered_set<const Type*> visited;
                if (type_depends_on_by_value(field.type, type, visited)) {
                    error(
                        std::format("Recursive by-value layout cycle detected: '{}' contains '{}' by value, which eventually contains '{}' by value again",
                            type->name, field.type->name, type->name),
                        SourceLocation{0, 0, "xec"}
                    );
                    ok = false;
                }
            }
        }

        if (!ok) {
            return false;
        }

        for (Type* type : type_registry_.all_types()) {
            if (type->kind == TypeKind::USER_DEFINED) {
                calculate_type_layout(type);
            }
        }

        return true;
    }

    bool SemanticAnalyser::validate_class_static_var(Type* class_type, const ast::VariableDecl* static_var_decl) {
        Type* final_type = resolve_let_binding_type(
            static_var_decl->name, static_var_decl->type_expr, static_var_decl->init_expr,
            static_var_decl->location, "static variable"
        );
        if (final_type == nullptr) {
            return false;
        }

        if (class_type->static_fields.find(static_var_decl->name) != class_type->static_fields.end()) {
            error(
                std::format("Static variable '{}' is already declared in class '{}'", static_var_decl->name, class_type->name),
                static_var_decl->location
            );
            return false;
        }

        class_type->add_static_field(Field(static_var_decl->name, final_type, static_var_decl->is_public));
        return true;
    }

    bool SemanticAnalyser::validate_class_method(Type* class_type, const ast::ClassMethodDecl* method_decl) {
        Type* return_type = resolve_type_expression(method_decl->return_type);
        if (return_type == nullptr) {
            return_type = type_registry_.get_void_type();
        }

        bool parameters_ok = true;
        std::vector<Parameter> semantic_parameters;
        semantic_parameters.reserve(method_decl->parameters.size());

        for (size_t i = 0; i < method_decl->parameters.size(); ++i) {
            const auto& param_decl = method_decl->parameters[i];
            Type* param_type = resolve_type_expression(param_decl->type_expr);

            if (param_type == nullptr || param_type->kind == TypeKind::ERROR) {
                parameters_ok = false;
            }

            // self is not special syntax - it's an ordinary explicit first
            // parameter - but for an instance method it must still name
            // itself 'self' and actually be a reference to this class.
            if (!method_decl->is_static && i == 0) {
                bool is_valid_self = param_decl->name == "self"
                    && param_type != nullptr
                    && param_type->kind == TypeKind::REFERENCE
                    && static_cast<ReferenceType*>(param_type)->referent_type == class_type;

                if (!is_valid_self) {
                    error(
                        std::format("Instance method '{}' must take 'self: &{}' or 'self: &mut {}' as its first parameter",
                            method_decl->name, class_type->name, class_type->name),
                        param_decl->location
                    );
                    parameters_ok = false;
                }
            }

            bool is_mutable = (i == 0 && !method_decl->is_static)
                ? self_parameter_is_mutable(param_type)
                : param_decl->is_mut;

            semantic_parameters.emplace_back(param_decl->name, param_type, is_mutable);
        }

        if (!method_decl->is_static && method_decl->parameters.empty()) {
            error(
                std::format("Instance method '{}' must take 'self' as its first parameter", method_decl->name),
                method_decl->location
            );
            parameters_ok = false;
        }

        ControlFlowResult result = ControlFlowResult::FALLS_THROUGH;
        if (method_decl->body) {
            result = validate_callable_body(
                std::format("method {}::{}@{}:{}", class_type->name, method_decl->name,
                    method_decl->location.line, method_decl->location.column),
                method_decl->parameters,
                return_type,
                method_decl->body.get(),
                parameters_ok
            );
        }

        if (!parameters_ok || return_type->kind == TypeKind::ERROR) {
            return false;
        }

        if (method_decl->body && return_type->kind != TypeKind::VOID && result != ControlFlowResult::RETURNS) {
            error(
                std::format("Method '{}' does not return a value of type '{}' on every control-flow path",
                    method_decl->name, return_type->name),
                method_decl->location
            );
            return false;
        }

        auto& overload_table = method_decl->is_static ? class_type->static_methods : class_type->methods;
        auto& overloads = overload_table[method_decl->name];

        for (const auto* existing : overloads.overloads) {
            if (existing->parameters.size() != semantic_parameters.size()) {
                continue;
            }
            bool same = true;
            for (size_t i = 0; i < semantic_parameters.size(); ++i) {
                if (existing->parameters[i].type != semantic_parameters[i].type) {
                    same = false;
                    break;
                }
            }
            if (same) {
                error(
                    std::format("'{}' is already declared with this parameter signature in class '{}'",
                        method_decl->name, class_type->name),
                    method_decl->location
                );
                return false;
            }
        }

        auto fn = std::make_unique<Function>(method_decl->name, method_decl, method_decl->is_public, std::move(semantic_parameters), return_type);
        Function* raw = fn.get();
        overloads.overloads.push_back(raw);
        class_type->owned_functions.push_back(std::move(fn));
        return true;
    }

    bool SemanticAnalyser::validate_operator_overload(Type* class_type, const ast::OperatorOverloadDecl* op_decl) {
        Type* return_type = resolve_type_expression(op_decl->return_type);
        if (return_type == nullptr) {
            return_type = type_registry_.get_void_type();
        }

        bool parameters_ok = true;
        std::vector<Parameter> semantic_parameters;
        semantic_parameters.reserve(op_decl->parameters.size());

        for (size_t i = 0; i < op_decl->parameters.size(); ++i) {
            const auto& param_decl = op_decl->parameters[i];
            Type* param_type = resolve_type_expression(param_decl->type_expr);

            if (param_type == nullptr || param_type->kind == TypeKind::ERROR) {
                parameters_ok = false;
            }

            if (i == 0) {
                bool is_valid_self = param_decl->name == "self"
                    && param_type != nullptr
                    && param_type->kind == TypeKind::REFERENCE
                    && static_cast<ReferenceType*>(param_type)->referent_type == class_type;

                if (!is_valid_self) {
                    error(
                        std::format("Operator overload for '{}' must take 'self: &{}' or 'self: &mut {}' as its first parameter",
                            op_decl->op_lexeme, class_type->name, class_type->name),
                        param_decl->location
                    );
                    parameters_ok = false;
                }
            }

            bool is_mutable = (i == 0) ? self_parameter_is_mutable(param_type) : param_decl->is_mut;
            semantic_parameters.emplace_back(param_decl->name, param_type, is_mutable);
        }

        if (op_decl->parameters.empty()) {
            error(
                std::format("Operator overload for '{}' must take 'self' as its first parameter", op_decl->op_lexeme),
                op_decl->location
            );
            parameters_ok = false;
        }

        if (!parameters_ok) {
            return false;
        }

        size_t operand_count = semantic_parameters.size() - 1;
        std::optional<OperatorKind> operator_kind = operator_kind_from_lexeme(op_decl->op_lexeme, operand_count);
        if (!operator_kind) {
            error(
                std::format("'{}' is not a valid overloadable operator for {} operand(s)", op_decl->op_lexeme, operand_count),
                op_decl->location
            );
            return false;
        }

        ControlFlowResult result = ControlFlowResult::FALLS_THROUGH;
        if (op_decl->body) {
            result = validate_callable_body(
                std::format("operator{} {}@{}:{}", op_decl->op_lexeme, class_type->name,
                    op_decl->location.line, op_decl->location.column),
                op_decl->parameters,
                return_type,
                op_decl->body.get(),
                parameters_ok
            );
        }

        if (!parameters_ok || return_type->kind == TypeKind::ERROR) {
            return false;
        }

        if (op_decl->body && return_type->kind != TypeKind::VOID && result != ControlFlowResult::RETURNS) {
            error(
                std::format("Operator overload for '{}' does not return a value of type '{}' on every control-flow path",
                    op_decl->op_lexeme, return_type->name),
                op_decl->location
            );
            return false;
        }

        std::vector<Operator>& overloads = class_type->operators[*operator_kind];
        if (has_matching_signature(overloads, semantic_parameters)) {
            error(
                std::format("Operator '{}' is already declared with this parameter signature in class '{}'",
                    op_decl->op_lexeme, class_type->name),
                op_decl->location
            );
            return false;
        }

        overloads.emplace_back(*operator_kind, std::move(semantic_parameters), return_type);
        return true;
    }

    bool SemanticAnalyser::validate_class_implementation(const ast::ClassImplementationDecl& impl_decl) {
        std::vector<std::string> parts;
        for (const ast::Name* part = impl_decl.class_name.get(); part != nullptr; part = part->next.get())
            parts.push_back(part->identifier);

        Symbol* symbol = parts.size() == 1
            ? global_scope_.lookup(parts[0])
            : global_scope_.get_qualified(parts);

        if (symbol == nullptr) {
            error(std::format("Cannot implement unknown class '{}'", impl_decl.class_name->to_string()), impl_decl.location);
            return false;
        }

        if (symbol->kind != SymbolKind::TYPE || static_cast<Type*>(symbol)->kind != TypeKind::USER_DEFINED) {
            error(std::format("'{}' is not a class", impl_decl.class_name->to_string()), impl_decl.location);
            return false;
        }

        Type* class_type = static_cast<Type*>(symbol);
        bool ok = true;

        for (const auto& static_var : impl_decl.static_vars) {
            if (!validate_class_static_var(class_type, static_var.get())) {
                ok = false;
            }
        }

        for (const auto& method : impl_decl.methods) {
            if (!validate_class_method(class_type, method.get())) {
                ok = false;
            }
        }

        for (const auto& op_overload : impl_decl.operator_overloads) {
            if (!validate_operator_overload(class_type, op_overload.get())) {
                ok = false;
            }
        }

        // Xenon has no constructors (see the AST/language notes) - a
        // "constructor" is just an ordinary ClassMethodDecl, typically
        // static (e.g. `init`), never invoked implicitly. It's validated
        // exactly like any other method, nothing special.
        for (const auto& ctor : impl_decl.constructors) {
            if (!validate_class_method(class_type, ctor.get())) {
                ok = false;
            }
        }

        return ok;
    }

    bool SemanticAnalyser::validate_delete(const ast::DeleteStmt* delete_stmt) {
        Type* target_type = evaluate_expression(delete_stmt->target.get());

        if (!target_type || target_type->kind == TypeKind::ERROR) {
            // Already reported by whatever failed inside the target expression.
            return false;
        }

        if (target_type->kind != TypeKind::POINTER) {
            error(std::format("delete requires a pointer, got '{}'", target_type->name), delete_stmt->location);
            return false;
        }

        return true;
    }

    Scope* SemanticAnalyser::module_scope_for_name(const std::string& name, const driver::Module& module) {
        const auto& module_name = module.ast && !module.ast->module_name.empty()
            ? module.ast->module_name.components
            : std::vector<std::string>{};

        if (!module_name.empty()) {
            return global_scope_.get_or_create_path(module_name);
        }

        std::vector<std::string> parts;
        std::string current;
        for (char ch : name) {
            if (ch == ':' && !current.empty()) {
                parts.push_back(current);
                current.clear();
                continue;
            }
            if (ch == ':') {
                continue;
            }
            current.push_back(ch);
        }
        if (!current.empty()) {
            parts.push_back(current);
        }

        return global_scope_.get_or_create_path(parts);
    }

    bool SemanticAnalyser::discover_module(const std::string& name, const driver::Module& module) {
        if (!module.ast.has_value()) {
            return true;
        }

        Scope* previous_scope = current_scope_;
        Scope* module_scope = module_scope_for_name(name, module);
        current_scope_ = module_scope;

        bool ok = true;
        for (const auto& decl : module.ast->root.declarations) {
            switch (decl->kind) {
                case ast::ASTNode::NodeKind::CLASS_STRUCTURE_DECL: {
                    const auto* class_decl = static_cast<const ast::ClassStructureDecl*>(decl.get());
                    if (current_scope_->lookup_local(class_decl->name) == nullptr) {
                        auto new_type = std::make_unique<Type>(TypeKind::USER_DEFINED, class_decl->name);
                        Type* class_type = type_registry_.register_type(std::move(new_type));
                        current_scope_->add_symbol(class_type);
                    }
                    break;
                }
                case ast::ASTNode::NodeKind::FUNCTION_DECL: {
                    const auto* fn_decl = static_cast<const ast::FunctionDecl*>(decl.get());
                    auto fn = std::make_unique<Function>(fn_decl->name, fn_decl, fn_decl->is_public,
                        std::vector<Parameter>{}, type_registry_.get_void_type());
                    if (!current_scope_->add_symbol(fn.get())) {
                        Symbol* existing = current_scope_->lookup_local(fn_decl->name);
                        if (existing && existing->kind == SymbolKind::FUNCTION) {
                            auto variants = std::make_unique<FunctionVariants>(fn_decl->name);
                            auto* first = static_cast<Function*>(existing);
                            variants->overloads.push_back(first);
                            variants->overloads.push_back(fn.get());
                            current_scope_->symbols[fn_decl->name] = variants.get();
                            current_scope_->owned_symbols.push_back(std::move(variants));
                        } else {
                            ok = false;
                        }
                    } else {
                        current_scope_->owned_symbols.push_back(std::move(fn));
                    }
                    break;
                }
                default:
                    break;
            }
        }

        current_scope_ = previous_scope;
        return ok;
    }

    bool SemanticAnalyser::validate_module(const std::string& name, const driver::Module& module) {
        if (!module.ast.has_value()) {
            return true;
        }

        Scope* previous_scope = current_scope_;
        current_scope_ = module_scope_for_name(name, module);

        bool ok = true;
        for (const auto& decl : module.ast->root.declarations) {
            switch (decl->kind) {
                case ast::ASTNode::NodeKind::FUNCTION_DECL:
                    if (!validate_function(*static_cast<const ast::FunctionDecl*>(decl.get()))) {
                        ok = false;
                    }
                    break;
                case ast::ASTNode::NodeKind::CLASS_STRUCTURE_DECL:
                    if (!validate_class_structure(*static_cast<const ast::ClassStructureDecl*>(decl.get()))) {
                        ok = false;
                    }
                    break;
                case ast::ASTNode::NodeKind::CLASS_IMPLEMENTATION_DECL:
                    if (!validate_class_implementation(*static_cast<const ast::ClassImplementationDecl*>(decl.get()))) {
                        ok = false;
                    }
                    break;
                case ast::ASTNode::NodeKind::VARIABLE_DECL:
                    if (!validate_variable_decl(static_cast<const ast::VariableDecl*>(decl.get())))  {
                        ok = false;
                    }
                    break;
                default:
                    break;
            }
        }

        current_scope_ = previous_scope;
        return ok;
    }

    bool SemanticAnalyser::validate_modules() {
        const auto module_names = namespace_tree_.all_module_names();
        for (const auto& name : module_names) {
            const auto* module = namespace_tree_.get_module(name);
            if (module == nullptr || !module->ast.has_value()) {
                continue;
            }
            if (!discover_module(name, *module)) {
                return false;
            }
        }

        for (const auto& name : module_names) {
            const auto* module = namespace_tree_.get_module(name);
            if (module == nullptr || !module->ast.has_value()) {
                continue;
            }
            if (!validate_module(name, *module)) {
                return false;
            }
        }

        if (!validate_recursive_value_layout_cycles()) {
            return false;
        }

        return !g_diagnostics.has_errors();
    }

    bool SemanticAnalyser::validate(const driver::ModuleNamespaceTree& namespace_tree,
        const config::CompilerConfig& options) {
        return SemanticAnalyser(namespace_tree, options).validate_modules();
    }

} // namespace xenon::semantic