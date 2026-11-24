#include "ast/yaml_printer.h"

#include "ast/ast.h"
#include "parser/parser_result.h"

#include <ostream>
#include <string>
#include <string_view>

namespace xenon::ast {
namespace {

    void indent(std::ostream& output, std::size_t amount) {
        for (std::size_t i = 0; i < amount; ++i) output.put(' ');
    }

    void scalar(std::ostream& output, std::string_view value) {
        static constexpr char hex[] = "0123456789ABCDEF";
        output.put('"');
        for (const char byte : value) {
            const auto ch = static_cast<unsigned char>(byte);
            switch (ch) {
                case '"': output << "\\\""; break;
                case '\\': output << "\\\\"; break;
                case '\n': output << "\\n"; break;
                case '\r': output << "\\r"; break;
                case '\t': output << "\\t"; break;
                default:
                    if (ch < 0x20 || ch == 0x7F) {
                        output << "\\x" << hex[ch >> 4] << hex[ch & 0x0F];
                    } else {
                        output.put(static_cast<char>(ch));
                    }
            }
        }
        output.put('"');
    }

    const char* node_name(NodeKind kind) {
        switch (kind) {
            case NodeKind::LITERAL_INT: return "LiteralInt";
            case NodeKind::LITERAL_FLOAT: return "LiteralFloat";
            case NodeKind::LITERAL_STRING: return "LiteralString";
            case NodeKind::LITERAL_CHAR: return "LiteralChar";
            case NodeKind::LITERAL_BOOL: return "LiteralBool";
            case NodeKind::LITERAL_ARRAY: return "LiteralArray";
            case NodeKind::LITERAL_NULLPTR: return "LiteralNullptr";
            case NodeKind::LITERAL_STRUCT: return "LiteralStruct";
            case NodeKind::NAME: return "Name";
            case NodeKind::MEMBER_ACCESS_EXPR: return "MemberAccessExpr";
            case NodeKind::CALL_EXPR: return "CallExpr";
            case NodeKind::INDEX_EXPR: return "IndexExpr";
            case NodeKind::NAMED_TYPE: return "NamedTypeExpr";
            case NodeKind::POINTER_TYPE: return "PointerTypeExpr";
            case NodeKind::REFERENCE_TYPE: return "ReferenceTypeExpr";
            case NodeKind::ARRAY_TYPE: return "ArrayTypeExpr";
            case NodeKind::BINARY_OP_EXPR: return "BinaryOpExpr";
            case NodeKind::UNARY_OP_EXPR: return "UnaryOpExpr";
            case NodeKind::ASSIGNMENT_EXPR: return "AssignmentExpr";
            case NodeKind::TERNARY_EXPR: return "TernaryExpr";
            case NodeKind::BLOCK_STMT: return "BlockStmt";
            case NodeKind::EXPRESSION_STMT: return "ExpressionStmt";
            case NodeKind::IF_STMT: return "IfStmt";
            case NodeKind::WHILE_STMT: return "WhileStmt";
            case NodeKind::FOREACH_STMT: return "ForeachStmt";
            case NodeKind::RETURN_STMT: return "ReturnStmt";
            case NodeKind::BREAK_STMT: return "BreakStmt";
            case NodeKind::CONTINUE_STMT: return "ContinueStmt";
            case NodeKind::MODULE_DECL: return "ModuleDecl";
            case NodeKind::IMPORT_DECL: return "ImportDecl";
            case NodeKind::VARIABLE_DECL: return "VariableDecl";
            case NodeKind::FUNCTION_DECL: return "FunctionDecl";
            case NodeKind::STRUCT_FIELD_DECL: return "StructFieldDecl";
            case NodeKind::METHOD_DECL: return "MethodDecl";
            case NodeKind::STRUCT_DECL: return "StructDecl";
            case NodeKind::IMPL_DECL: return "ImplDecl";
            case NodeKind::EOF_STMT: return "EofStmt";
        }
        return "Unknown";
    }

    const char* numeric_suffix(NumericSuffix suffix) {
        switch (suffix) {
            case NumericSuffix::NONE: return "none";
            case NumericSuffix::I8: return "i8";
            case NumericSuffix::I16: return "i16";
            case NumericSuffix::I32: return "i32";
            case NumericSuffix::I64: return "i64";
            case NumericSuffix::U8: return "u8";
            case NumericSuffix::U16: return "u16";
            case NumericSuffix::U32: return "u32";
            case NumericSuffix::U64: return "u64";
            case NumericSuffix::SIZE: return "size";
            case NumericSuffix::F32: return "f32";
            case NumericSuffix::F64: return "f64";
        }
        return "unknown";
    }

    const char* unary_name(UnaryOperatorKind op) {
        switch (op) {
            case UnaryOperatorKind::PLUS: return "plus";
            case UnaryOperatorKind::NEGATE: return "negate";
            case UnaryOperatorKind::BITWISE_NOT: return "bitwise_not";
            case UnaryOperatorKind::LOGICAL_NOT: return "logical_not";
            case UnaryOperatorKind::BORROW: return "borrow";
            case UnaryOperatorKind::MUT_BORROW: return "mut_borrow";
            case UnaryOperatorKind::RAW_BORROW: return "raw_borrow";
            case UnaryOperatorKind::MUT_RAW_BORROW: return "mut_raw_borrow";
            case UnaryOperatorKind::DEREFERENCE: return "dereference";
        }
        return "unknown";
    }

    const char* binary_name(BinaryOperatorKind op) {
        switch (op) {
            case BinaryOperatorKind::ADD: return "add";
            case BinaryOperatorKind::SUBTRACT: return "subtract";
            case BinaryOperatorKind::MULTIPLY: return "multiply";
            case BinaryOperatorKind::DIVIDE: return "divide";
            case BinaryOperatorKind::MODULO: return "modulo";
            case BinaryOperatorKind::EQUAL: return "equal";
            case BinaryOperatorKind::NOT_EQUAL: return "not_equal";
            case BinaryOperatorKind::LESS_THAN: return "less_than";
            case BinaryOperatorKind::LESS_EQUAL: return "less_equal";
            case BinaryOperatorKind::GREATER_THAN: return "greater_than";
            case BinaryOperatorKind::GREATER_EQUAL: return "greater_equal";
            case BinaryOperatorKind::LOGICAL_AND: return "logical_and";
            case BinaryOperatorKind::LOGICAL_OR: return "logical_or";
            case BinaryOperatorKind::BITWISE_AND: return "bitwise_and";
            case BinaryOperatorKind::BITWISE_OR: return "bitwise_or";
            case BinaryOperatorKind::BITWISE_XOR: return "bitwise_xor";
            case BinaryOperatorKind::SHIFT_LEFT: return "shift_left";
            case BinaryOperatorKind::SHIFT_RIGHT: return "shift_right";
        }
        return "unknown";
    }

    const char* assignment_name(AssignmentOperatorKind op) {
        switch (op) {
            case AssignmentOperatorKind::ASSIGN: return "assign";
            case AssignmentOperatorKind::ADD_ASSIGN: return "add_assign";
            case AssignmentOperatorKind::SUBTRACT_ASSIGN: return "subtract_assign";
            case AssignmentOperatorKind::MULTIPLY_ASSIGN: return "multiply_assign";
            case AssignmentOperatorKind::DIVIDE_ASSIGN: return "divide_assign";
            case AssignmentOperatorKind::MODULO_ASSIGN: return "modulo_assign";
            case AssignmentOperatorKind::BITWISE_AND_ASSIGN: return "bitwise_and_assign";
            case AssignmentOperatorKind::BITWISE_OR_ASSIGN: return "bitwise_or_assign";
            case AssignmentOperatorKind::BITWISE_XOR_ASSIGN: return "bitwise_xor_assign";
            case AssignmentOperatorKind::SHIFT_LEFT_ASSIGN: return "shift_left_assign";
            case AssignmentOperatorKind::SHIFT_RIGHT_ASSIGN: return "shift_right_assign";
        }
        return "unknown";
    }

    void emit_span(std::ostream& output, std::size_t level, source::Span span) {
        indent(output, level);
        output << "span:\n";
        indent(output, level + 2);
        output << "file: " << span.file.value_ << '\n';
        indent(output, level + 2);
        output << "start: " << span.start << '\n';
        indent(output, level + 2);
        output << "end: " << span.end << '\n';
    }

    void emit_node(std::ostream& output, std::size_t level, const ASTNode& node, bool list_item = false);

    void emit_node_field(
        std::ostream& output,
        std::size_t level,
        std::string_view key,
        const ASTNode* node
    ) {
        indent(output, level);
        output << key << ':';
        if (!node) {
            output << " null\n";
            return;
        }
        output << '\n';
        emit_node(output, level + 2, *node);
    }

    template<typename Nodes>
    void emit_nodes(std::ostream& output, std::size_t level, std::string_view key, const Nodes& nodes) {
        indent(output, level);
        output << key << ':';
        if (nodes.empty()) {
            output << " []\n";
            return;
        }
        output << '\n';
        for (const auto& node : nodes) {
            if (node) emit_node(output, level + 2, *node, true);
        }
    }

    void emit_attributes(std::ostream& output, std::size_t level, const Declaration& declaration) {
        indent(output, level);
        output << "doc: ";
        scalar(output, declaration.doc);
        output << '\n';
        indent(output, level);
        output << "attributes:";
        if (declaration.attributes.empty()) {
            output << " []\n";
            return;
        }
        output << '\n';
        for (const auto& attribute : declaration.attributes) {
            indent(output, level + 2);
            output << "- name: ";
            scalar(output, attribute.name ? attribute.name->to_string() : "");
            output << '\n';
            indent(output, level + 4);
            output << "args:";
            if (attribute.args.empty()) {
                output << " []\n";
            } else {
                output << '\n';
                for (const auto& argument : attribute.args) {
                    if (argument) emit_node(output, level + 6, *argument, true);
                }
            }
        }
    }

    void emit_node(std::ostream& output, std::size_t level, const ASTNode& node, bool list_item) {
        indent(output, level);
        if (list_item) output << "- ";
        output << "kind: ";
        scalar(output, node_name(node.kind_));
        output << '\n';
        const std::size_t fields = level + (list_item ? 2 : 0);
        emit_span(output, fields, node.span_);

#define XENON_NODE(Type) (node.kind_ == Type::node_kind ? ast::as<Type>(const_cast<ASTNode*>(&node)) : nullptr)
#define XENON_FIELD(key, ptr) emit_node_field(output, fields, key, ptr)
#define XENON_STRING(key, value) do { indent(output, fields); output << key << ": "; scalar(output, value); output << '\n'; } while (false)
#define XENON_BOOL(key, value) do { indent(output, fields); output << key << ": " << ((value) ? "true" : "false") << '\n'; } while (false)

        if (const auto* int_literal = XENON_NODE(LiteralInt)) {
            XENON_STRING("value", int_literal->lexeme);
            XENON_STRING("suffix", numeric_suffix(int_literal->suffix));
        } else if (const auto* float_literal = XENON_NODE(LiteralFloat)) {
            XENON_STRING("value", float_literal->lexeme);
            XENON_STRING("suffix", numeric_suffix(float_literal->suffix));
        } else if (const auto* string_literal = XENON_NODE(LiteralString)) {
            XENON_STRING("value", string_literal->value);
        } else if (const auto* char_literal = XENON_NODE(LiteralChar)) {
            indent(output, fields); output << "value: " << char_literal->value << '\n';
        } else if (const auto* bool_literal = XENON_NODE(LiteralBool)) {
            XENON_BOOL("value", bool_literal->value);
        } else if (const auto* array_literal = XENON_NODE(LiteralArray)) {
            XENON_FIELD("element_type", array_literal->element_type.get());
            XENON_FIELD("size", array_literal->size_expr.get());
            indent(output, fields); output << "elements:";
            if (array_literal->elements.empty()) output << " []\n";
            else {
                output << '\n';
                for (const auto& element : array_literal->elements) {
                    if (element) emit_node(output, fields + 2, *element, true);
                }
            }
        } else if (node.kind_ == NodeKind::LITERAL_NULLPTR) {
            indent(output, fields); output << "value: null\n";
        } else if (const auto* struct_literal = XENON_NODE(LiteralStruct)) {
            XENON_FIELD("type", struct_literal->type_expr.get());
            XENON_BOOL("named_fields", struct_literal->is_named);
            indent(output, fields); output << "fields:";
            if (struct_literal->fields.empty()) output << " []\n";
            else {
                output << '\n';
                for (const auto& field : struct_literal->fields) {
                    indent(output, fields + 2); output << "- name: "; scalar(output, field.name); output << '\n';
                    emit_node_field(output, fields + 4, "value", field.value.get());
                }
            }
        } else if (const auto* name = XENON_NODE(Name)) {
            XENON_STRING("value", name->to_string());
        } else if (const auto* member_access = XENON_NODE(MemberAccessExpr)) {
            XENON_FIELD("object", member_access->object.get());
            XENON_FIELD("member", member_access->member.get());
        } else if (const auto* call = XENON_NODE(CallExpr)) {
            XENON_FIELD("callee", call->callee.get());
            indent(output, fields); output << "arguments:";
            if (call->arguments.empty()) output << " []\n";
            else {
                output << '\n';
                for (const auto& argument : call->arguments) {
                    if (argument) emit_node(output, fields + 2, *argument, true);
                }
            }
        } else if (const auto* index = XENON_NODE(IndexExpr)) {
            XENON_FIELD("object", index->object.get());
            XENON_FIELD("index", index->index.get());
        } else if (const auto* named_type = XENON_NODE(NamedTypeExpr)) {
            XENON_FIELD("name", named_type->name.get());
        } else if (const auto* pointer_type = XENON_NODE(PointerTypeExpr)) {
            XENON_BOOL("mutable", pointer_type->is_mut);
            XENON_FIELD("element_type", pointer_type->element_type.get());
        } else if (const auto* reference_type = XENON_NODE(ReferenceTypeExpr)) {
            XENON_BOOL("mutable", reference_type->is_mut);
            XENON_FIELD("element_type", reference_type->element_type.get());
        } else if (const auto* array_type = XENON_NODE(ArrayTypeExpr)) {
            XENON_FIELD("element_type", array_type->element_type.get());
            XENON_FIELD("size", array_type->size_expr.get());
        } else if (const auto* binary = XENON_NODE(BinaryOpExpr)) {
            XENON_STRING("operator", binary_name(binary->op));
            XENON_FIELD("left", binary->lhs.get());
            XENON_FIELD("right", binary->rhs.get());
        } else if (const auto* unary = XENON_NODE(UnaryOpExpr)) {
            XENON_STRING("operator", unary_name(unary->op));
            XENON_FIELD("operand", unary->operand.get());
        } else if (const auto* assignment = XENON_NODE(AssignmentExpr)) {
            XENON_STRING("operator", assignment_name(assignment->op));
            XENON_FIELD("left", assignment->lhs.get());
            XENON_FIELD("right", assignment->rhs.get());
        } else if (const auto* ternary = XENON_NODE(TernaryExpr)) {
            XENON_FIELD("condition", ternary->condition.get());
            XENON_FIELD("then", ternary->then_branch.get());
            XENON_FIELD("else", ternary->else_branch.get());
        } else if (const auto* block = XENON_NODE(BlockStmt)) {
            emit_nodes(output, fields, "statements", block->statements);
        } else if (const auto* expression_stmt = XENON_NODE(ExpressionStmt)) {
            XENON_FIELD("expression", expression_stmt->expr.get());
        } else if (const auto* if_stmt = XENON_NODE(IfStmt)) {
            XENON_FIELD("condition", if_stmt->condition.get());
            XENON_FIELD("then", if_stmt->then_branch.get());
            XENON_FIELD("else", if_stmt->else_branch.get());
        } else if (const auto* while_stmt = XENON_NODE(WhileStmt)) {
            XENON_FIELD("condition", while_stmt->condition.get());
            XENON_FIELD("body", while_stmt->body.get());
        } else if (const auto* return_stmt = XENON_NODE(ReturnStmt)) {
            XENON_FIELD("value", return_stmt->return_value.get());
        } else if (node.kind_ == NodeKind::BREAK_STMT || node.kind_ == NodeKind::CONTINUE_STMT
                   || node.kind_ == NodeKind::EOF_STMT || node.kind_ == NodeKind::FOREACH_STMT) {
        } else if (const auto* module_decl = XENON_NODE(ModuleDecl)) {
            emit_attributes(output, fields, *module_decl);
            XENON_FIELD("name", module_decl->name.get());
        } else if (const auto* import_decl = XENON_NODE(ImportDecl)) {
            emit_attributes(output, fields, *import_decl);
            XENON_FIELD("name", import_decl->name.get());
        } else if (const auto* variable = XENON_NODE(VariableDecl)) {
            emit_attributes(output, fields, *variable);
            XENON_STRING("name", variable->name);
            XENON_BOOL("mutable", variable->is_mut);
            XENON_BOOL("public", variable->is_public);
            XENON_FIELD("type", variable->type_expr.get());
            XENON_FIELD("initializer", variable->init_expr.get());
        } else if (const auto* function = XENON_NODE(FunctionDecl)) {
            emit_attributes(output, fields, *function);
            XENON_STRING("name", function->name);
            XENON_BOOL("public", function->is_public);
            emit_nodes(output, fields, "parameters", function->parameters);
            XENON_FIELD("return_type", function->return_type.get());
            XENON_FIELD("body", function->body.get());
        } else if (const auto* struct_field = XENON_NODE(StructFieldDecl)) {
            emit_attributes(output, fields, *struct_field);
            XENON_STRING("name", struct_field->name);
            XENON_BOOL("public", struct_field->is_public);
            XENON_FIELD("type", struct_field->type_expr.get());
        } else if (const auto* method = XENON_NODE(MethodDecl)) {
            emit_attributes(output, fields, *method);
            XENON_STRING("name", method->name);
            XENON_BOOL("public", method->is_public);
            XENON_BOOL("static", method->is_static);
            indent(output, fields); output << "receiver: ";
            if (!method->receiver) output << "null\n";
            else {
                output << '\n';
                indent(output, fields + 2);
                output << "kind: " << (method->receiver->type == SelfReceiverType::VALUE ? "value" : "reference") << '\n';
                indent(output, fields + 2);
                output << "mutable: " << (method->receiver->is_mut ? "true" : "false") << '\n';
            }
            emit_nodes(output, fields, "parameters", method->parameters);
            XENON_FIELD("return_type", method->return_type.get());
            XENON_FIELD("body", method->body.get());
        } else if (const auto* struct_decl = XENON_NODE(StructDecl)) {
            emit_attributes(output, fields, *struct_decl);
            XENON_STRING("name", struct_decl->name);
            XENON_BOOL("public", struct_decl->is_public);
            emit_nodes(output, fields, "fields", struct_decl->fields);
        } else if (const auto* impl_decl = XENON_NODE(ImplDecl)) {
            emit_attributes(output, fields, *impl_decl);
            XENON_FIELD("type", impl_decl->type_name.get());
            emit_nodes(output, fields, "static_variables", impl_decl->static_vars);
            emit_nodes(output, fields, "methods", impl_decl->methods);
        }

#undef XENON_NODE
#undef XENON_FIELD
#undef XENON_STRING
#undef XENON_BOOL
    }

} // namespace

void print_yaml(
    std::ostream& output,
    const std::filesystem::path& source_path,
    const parser::ParserOutput& result,
    bool first_source
) {
    if (first_source) output << "sources:\n";
    output << "  - path: ";
    scalar(output, source_path.generic_string());
    output << "\n    success: " << (result.success() ? "true" : "false") << '\n';
    const auto& parsed = result.value();
    if (!parsed.ast) {
        output << "    ast: null\n";
        return;
    }
    output << "    ast:\n";
    output << "      module_doc: ";
    scalar(output, parsed.ast->module_doc);
    output << '\n';
    output << "      module:";
    if (!parsed.header.module_decl) {
        output << " null\n";
    } else {
        output << '\n';
        emit_node(output, 8, *parsed.header.module_decl);
    }
    output << "      imports:";
    if (parsed.header.imports.empty()) output << " []\n";
    else {
        output << '\n';
        for (const auto& import : parsed.header.imports) emit_node(output, 8, import, true);
    }
    output << "      declarations:";
    if (parsed.ast->declarations().empty()) output << " []\n";
    else {
        output << '\n';
        for (const auto& declaration : parsed.ast->declarations()) {
            if (declaration) emit_node(output, 8, *declaration, true);
        }
    }
}

} // namespace xenon::ast