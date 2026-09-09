#include "semantic/types.h"
#include "semantic/scope.h"

namespace xenon::semantic {

ByteSize calculate_type_size(Type* type) {
    if (type->size.has_value()) {
        return type->size.value();
    }

    switch (type->kind) {
        case TypeKind::PRIMITIVE: {
            auto* builtin_type = dynamic_cast<BuiltinType*>(type);
            if (builtin_type) {
                type->size = BuiltinType::builtin_size(builtin_type->kind);
                return type->size.value();
            }
            break;
        }
        case TypeKind::POINTER:
        case TypeKind::REFERENCE:
            type->size = WORD_SIZE;
            return type->size.value();
        case TypeKind::ARRAY: {
            auto* array_type = dynamic_cast<ArrayType*>(type);
            if (array_type && array_type->element_type) {
                if (array_type->size.has_value()) {
                    ByteSize element_size = calculate_type_size(array_type->element_type);
                    type->size = element_size * array_type->size.value();
                    return type->size.value();
                } else {
                    type->size = WORD_SIZE * 3;
                    return type->size.value();
                }
            }
            break;
        }
        case TypeKind::USER_DEFINED: {
            ByteSize total_size = 0;
            Alignment max_align = 1;
            for (const auto& field : type->fields) {
                ByteSize field_size = calculate_type_size(field.type);
                Alignment field_align = field.type->align;
                total_size = align_up(total_size, field_align);
                total_size += field_size;
                max_align = std::max(max_align, field_align);
            }
            total_size = align_up(total_size, max_align);
            type->size = total_size;
            return type->size.value();
        }
        default:
            break;
    }
    return type->size.value_or(0);
}

Alignment calculate_type_align(Type* type) {
    if (type->align != 0) {
        return type->align;
    }

    switch (type->kind) {
        case TypeKind::PRIMITIVE: {
            auto* builtin_type = dynamic_cast<BuiltinType*>(type);
            if (builtin_type) {
                type->align = BuiltinType::builtin_align(builtin_type->kind);
                return type->align;
            }
            break;
        }
        case TypeKind::POINTER:
        case TypeKind::REFERENCE:
            type->align = WORD_ALIGN;
            return type->align;
        case TypeKind::ARRAY: {
            auto* array_type = dynamic_cast<ArrayType*>(type);
            if (array_type && array_type->element_type) {
                Alignment element_align = calculate_type_align(array_type->element_type);
                type->align = element_align;
                return type->align;
            }
            break;
        }
        case TypeKind::USER_DEFINED: {
            Alignment max_align = 1;
            for (const auto& field : type->fields) {
                Alignment field_align = calculate_type_align(field.type);
                max_align = std::max(max_align, field_align);
            }
            type->align = max_align;
            return type->align;
        }
        default:
            break;
    }
    return type->align;
}

TypeLayout calculate_type_layout(Type* type) {
    TypeLayout layout;
    layout.size = calculate_type_size(type);
    layout.align = calculate_type_align(type);

    ByteSize offset = 0;
    for (auto& field : type->fields) {
        ByteSize field_size = calculate_type_size(field.type);
        Alignment field_align = calculate_type_align(field.type);
        offset = align_up(offset, field_align);

        FieldLayout field_layout{field, offset, field_size, field_align};
        layout.fields.push_back(field_layout);

        offset += field_size;
    }

    return layout;
}

Type* TypeRegistry::register_type(std::unique_ptr<Type> type) {
    auto kind = type->kind;
    auto name = type->name;
    types[name] = std::move(type);
    types_by_kind[kind].push_back(types[name].get());
    return types[name].get();
}

Type* TypeRegistry::register_builtin_type(BuiltinType::BuiltinKind kind) {
    auto type = std::make_unique<BuiltinType>(kind);
    Type* result = type.get();
    register_type(std::move(type));
    return result;
}

Type* TypeRegistry::register_pointer(Type* type, bool is_mut) {
    if (type == get_error_type())
        return get_error_type();

    std::unique_ptr<Type> pointer_type = std::make_unique<PointerType>(type, is_mut);
    auto name = pointer_type->name;
    auto kind = pointer_type->kind;
    types[name] = std::move(pointer_type);
    types_by_kind[kind].push_back(types[name].get());

    types[name]->add_operator(Operator(OperatorKind::DEREFERENCE, { Parameter("this", types[name].get(), false) }, type));
    types[name]->add_operator(Operator(OperatorKind::EQ, { Parameter("this", types[name].get(), false), Parameter("rhs", types[name].get(), false) }, get_builtin_type(BuiltinType::BuiltinKind::BOOL)));
    types[name]->add_operator(Operator(OperatorKind::NEQ, { Parameter("this", types[name].get(), false), Parameter("rhs", types[name].get(), false) }, get_builtin_type(BuiltinType::BuiltinKind::BOOL)));

    return types[name].get();
}

Type* TypeRegistry::register_reference(Type* type, bool is_mut) {
    if (type == get_error_type() || type == get_void_type())
        return get_error_type();

    std::unique_ptr<Type> reference_type = std::make_unique<ReferenceType>(type, is_mut);
    auto name = reference_type->name;
    auto kind = reference_type->kind;
    types[name] = std::move(reference_type);
    types_by_kind[kind].push_back(types[name].get());
    return types[name].get();
}

Type* TypeRegistry::register_array_type(Type* elem_type, std::optional<unsigned> arr_length) {
    if (elem_type == get_error_type() || elem_type == get_void_type())
        return get_error_type();

    std::unique_ptr<Type> array_type = std::make_unique<ArrayType>(elem_type, arr_length);
    auto name = array_type->name;
    auto kind = array_type->kind;
    types[name] = std::move(array_type);
    types_by_kind[kind].push_back(types[name].get());

    if (arr_length.has_value()) {
        types[name]->add_field(Field("data", register_pointer(elem_type), true));
        types[name]->add_field(Field("length", get_builtin_type(BuiltinType::BuiltinKind::SIZE), true));
        types[name]->add_field(Field("capacity", get_builtin_type(BuiltinType::BuiltinKind::SIZE), true));

        types[name]->add_method(Function("length", nullptr, false, {}, get_builtin_type(BuiltinType::BuiltinKind::SIZE)));
        types[name]->add_method(Function("capacity", nullptr, false, {}, get_builtin_type(BuiltinType::BuiltinKind::SIZE)));
        types[name]->add_method(Function("push", nullptr, false, { Parameter("value", elem_type, false) }, get_void_type()));
    }

    types[name]->add_operator(Operator(OperatorKind::IDX, { Parameter("this", types[name].get(), false), Parameter("index", get_builtin_type(BuiltinType::BuiltinKind::SIZE), false) }, register_reference(elem_type)));
    types[name]->add_operator(Operator(OperatorKind::IDX, { Parameter("this", types[name].get(), true), Parameter("index", get_builtin_type(BuiltinType::BuiltinKind::SIZE), false) }, register_reference(elem_type, true)));

    return types[name].get();
}

void TypeRegistry::initialise_builtin_types(Scope& global_scope) {
    for (const auto& kind : {
        BuiltinType::BuiltinKind::U8, BuiltinType::BuiltinKind::U16, BuiltinType::BuiltinKind::U32, BuiltinType::BuiltinKind::U64,
        BuiltinType::BuiltinKind::I8, BuiltinType::BuiltinKind::I16, BuiltinType::BuiltinKind::I32, BuiltinType::BuiltinKind::I64,
        BuiltinType::BuiltinKind::F32, BuiltinType::BuiltinKind::F64,
        BuiltinType::BuiltinKind::CPLX128,
        BuiltinType::BuiltinKind::BOOL,
        BuiltinType::BuiltinKind::CHAR,
        BuiltinType::BuiltinKind::SIZE,
        BuiltinType::BuiltinKind::STRING
    }) {
        Type* type = register_builtin_type(kind);
        global_scope.add_symbol(type);
    }

    auto* void_type = register_type(std::make_unique<VoidType>());
    auto* null_type = register_type(std::make_unique<NullType>());
    global_scope.add_symbol(void_type);
    global_scope.add_symbol(null_type);

    Type* string_type = get_builtin_type(BuiltinType::BuiltinKind::STRING);
    string_type->add_field(Field("data", get_builtin_type(BuiltinType::BuiltinKind::U8), true));
    string_type->add_field(Field("length", get_builtin_type(BuiltinType::BuiltinKind::SIZE), true));
    string_type->add_method(Function("length", nullptr, false, { Parameter("this", string_type, false) }, get_builtin_type(BuiltinType::BuiltinKind::SIZE)));
    string_type->add_operator(Operator(OperatorKind::IDX, { Parameter("this", string_type, false), Parameter("index", get_builtin_type(BuiltinType::BuiltinKind::SIZE), false) }, get_builtin_type(BuiltinType::BuiltinKind::CHAR)));

    Type* complex_type = get_builtin_type(BuiltinType::BuiltinKind::CPLX128);
    complex_type->add_field(Field("real", get_builtin_type(BuiltinType::BuiltinKind::F64), true));
    complex_type->add_field(Field("imag", get_builtin_type(BuiltinType::BuiltinKind::F64), true));
    complex_type->add_method(Function("magnitude", nullptr, false, { Parameter("this", complex_type, false) }, get_builtin_type(BuiltinType::BuiltinKind::F64)));
    complex_type->add_method(Function("phase", nullptr, false, { Parameter("this", complex_type, false) }, get_builtin_type(BuiltinType::BuiltinKind::F64)));
    complex_type->add_operator(Operator(OperatorKind::ADD, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::SUB, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::MUL, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::DIV, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::IADD, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::ISUB, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::IMUL, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::IDIV, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, complex_type));
    complex_type->add_operator(Operator(OperatorKind::EQ, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, get_builtin_type(BuiltinType::BuiltinKind::BOOL)));
    complex_type->add_operator(Operator(OperatorKind::NEQ, { Parameter("this", complex_type, false), Parameter("other", complex_type, false) }, get_builtin_type(BuiltinType::BuiltinKind::BOOL)));
    complex_type->add_operator(Operator(OperatorKind::NEG, { Parameter("this", string_type, false) }, complex_type));
}

Type* TypeRegistry::get_builtin_type(BuiltinType::BuiltinKind kind) {
    auto name = std::string(BuiltinType::builtin_to_string(kind));
    auto it = types.find(name);
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_type(const std::string& full_name) const {
    auto it = types.find(full_name);
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_pointer(Type* type, bool is_mutable) {
    if (type == nullptr || type == get_error_type() || type == get_void_type())
        return nullptr;

    auto name = std::format("*{}{}", (is_mutable ? "mut " : ""), type->name);
    auto it = types.find(name);
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_reference(Type* type, bool is_mutable) {
    if (type == nullptr || type == get_error_type() || type == get_void_type())
        return nullptr;

    auto name = std::format("&{}{}", (is_mutable ? "mut " : ""), type->name);
    auto it = types.find(name);
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_array(Type* element_type, std::optional<size_t> length) {
    if (element_type == nullptr || element_type == get_error_type() || element_type == get_void_type())
        return nullptr;

    auto name = std::format("[{}{}]", element_type->name, length.has_value() ? std::format("; {}", length.value()) : "");
    auto it = types.find(name);
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_void_type() {
    auto it = types.find("void");
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_null_type() {
    auto it = types.find("null");
    return it == types.end() ? nullptr : it->second.get();
}

Type* TypeRegistry::get_error_type() {
    return error_type.get();
}

bool TypeRegistry::is_signed_integer(const Type* type) const {
    if (!type || type->kind != TypeKind::PRIMITIVE) return false;
    switch(static_cast<const BuiltinType*>(type)->kind) {
        case BuiltinType::BuiltinKind::I8:
        case BuiltinType::BuiltinKind::I16:
        case BuiltinType::BuiltinKind::I32:
        case BuiltinType::BuiltinKind::I64:
            return true;
        default: 
            return false;
    }

}
bool TypeRegistry::is_unsigned_integer(const Type* type) const {
    if (!type || type->kind != TypeKind::PRIMITIVE) return false;
    switch(static_cast<const BuiltinType*>(type)->kind) {
        case BuiltinType::BuiltinKind::U8:
        case BuiltinType::BuiltinKind::U16:
        case BuiltinType::BuiltinKind::U32:
        case BuiltinType::BuiltinKind::U64:
        case BuiltinType::BuiltinKind::SIZE:
            return true;
        default: 
            return false;
    }

}
bool TypeRegistry::is_integer(const Type* type) const {
    return is_signed_integer(type) || is_unsigned_integer(type);
}
bool TypeRegistry::is_floating(const Type* type) const {
    if (!type || type->kind != TypeKind::PRIMITIVE) return false;
    switch(static_cast<const BuiltinType*>(type)->kind) {
        case BuiltinType::BuiltinKind::F32:
        case BuiltinType::BuiltinKind::F64:
            return true;
        default: 
            return false;
    }

}
bool TypeRegistry::is_complex(const Type* type) const {
    if (!type || type->kind != TypeKind::PRIMITIVE) return false;
    switch(static_cast<const BuiltinType*>(type)->kind) {
        case BuiltinType::BuiltinKind::CPLX128:
            return true;
        default: 
            return false;
    }
}
bool TypeRegistry::is_builtin_numeric(const Type* type) const {
    return is_integer(type) || is_floating(type) || is_complex(type);
}

bool TypeRegistry::can_implicitly_convert(const Type* from, const Type* to) const {
    if (from->kind == TypeKind::ERROR || to->kind == TypeKind::ERROR) {
        return true;
    }

    if (from->kind == TypeKind::NULLTYPE && to->kind == TypeKind::POINTER) {
        return true;
    }

    if (from == to) {
        return true;
    }

    if (is_unsigned_integer(from) && is_unsigned_integer(to)) {
        return from->size <= to->size;
    }
    if (is_signed_integer(from) && is_signed_integer(to)) {
        return from->size <= to->size;
    }
    if (is_floating(from) && is_floating(to)) {
        return from->size <= to->size;
    }
    if (is_complex(from) && is_complex(to)) {
        return from->size <= to->size;
    }

    return false;
}

} // namespace xenon::semantic
