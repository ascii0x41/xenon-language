// validation/validator.h
#pragma once

#include "common/dataclasses.h"
#include "ast/astlib.h"
#include "validation/types.h" // Your current type definitions
#include <memory>
#include <vector>
#include <string>

namespace xenon::validation {

class ValidationContext {
public:
    struct Scope {
        std::unordered_map<std::string, std::shared_ptr<RuntimeObject>> variables;
        std::unordered_map<std::string, std::shared_ptr<TypeInfo>> types;
        std::shared_ptr<Scope> parent;
    };

    void push_scope();
    void pop_scope();
    
    // Type registration and lookup
    void register_type(const std::string& name, std::shared_ptr<TypeInfo> type);
    std::shared_ptr<TypeInfo> lookup_type(const std::string& name) const;
    
    // Variable registration and lookup
    void register_variable(const std::string& name, std::shared_ptr<RuntimeObject> obj);
    std::shared_ptr<RuntimeObject> lookup_variable(const std::string& name) const;

    // Import/Export management
    void add_imported_module(const std::string& module_name, 
                            std::shared_ptr<ExportTable> exports);
    
    // Error handling
    void report_error(const std::string& message, ASTNodeViewPtr node = nullptr);
    bool has_errors() const { return !errors_.empty(); }
    const std::vector<std::string>& get_errors() const { return errors_; }

private:
    std::shared_ptr<Scope> current_scope_;
    std::vector<std::string> errors_;
    std::unordered_map<std::string, std::shared_ptr<ExportTable>> imported_modules_;
};

class ModuleValidator {
public:
    explicit ModuleValidator(std::shared_ptr<GlobalContext> global_ctx);
    
    bool validate_module(const ASTNode* module_node);
    
private:
    // Phase 1: Declaration collection
    void collect_declarations(const ASTNode* node);
    void collect_function_decl(const ASTNode* node);
    void collect_class_decl(const ASTNode* node);
    void collect_trait_decl(const ASTNode* node);
    
    // Phase 2: Type checking
    void type_check_module(const ASTNode* node);
    std::shared_ptr<TypeInfo> evaluate_expression(const ASTNode* expr);
    std::shared_ptr<TypeInfo> evaluate_literal(const ASTNode* literal);
    std::shared_ptr<TypeInfo> evaluate_binary_op(const ASTNode* binop);
    std::shared_ptr<TypeInfo> evaluate_function_call(const ASTNode* call);
    
    // Phase 3: Export validation
    void validate_exports(const std::vector<std::string>& exports);
    
    // Type compatibility
    bool is_type_compatible(std::shared_ptr<TypeInfo> expected, 
                           std::shared_ptr<TypeInfo> actual);
    bool is_assignable(std::shared_ptr<RuntimeObject> target,
                      std::shared_ptr<TypeInfo> value_type);
    
    // Error recovery
    std::shared_ptr<TypeInfo> get_error_type();
    
    ValidationContext context_;
    std::shared_ptr<GlobalContext> global_context_;
    std::shared_ptr<ExportTable> module_exports_;
};

class GlobalContext {
public:
    void register_module(const std::string& name, std::shared_ptr<ExportTable> exports);
    std::shared_ptr<ExportTable> get_module_exports(const std::string& name) const;
    std::shared_ptr<ExportTable> get_standard_library() const;
    
private:
    std::unordered_map<std::string, std::shared_ptr<ExportTable>> modules_;
    std::shared_ptr<ExportTable> std_lib_;
};

} // namespace xenon::validation