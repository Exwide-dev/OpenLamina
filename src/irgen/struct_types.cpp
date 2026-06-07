#include "struct_types.hpp"

#include "../tools/error.hpp"

namespace irgen {
namespace {
std::unordered_map<std::string, StructTypeDef>& registry_mut() {
    static std::unordered_map<std::string, StructTypeDef> reg;
    return reg;
}
} // namespace

const std::unordered_map<std::string, StructTypeDef>& struct_registry() {
    return registry_mut();
}

void register_struct_type(StructTypeDef def) {
    registry_mut()[def.name] = std::move(def);
}

bool is_struct_type(const std::string& name) {
    return registry_mut().contains(name);
}

const std::shared_ptr<StructObject>& Value::asStruct() const {
    if (type == Type::Reference) {
        return asReference().get().asStruct();
    }
    if (type != Type::StructObject) {
        throw RuntimeError("Value is not a struct");
    }
    return std::get<std::shared_ptr<StructObject>>(data);
}

std::shared_ptr<StructObject>& Value::asStruct() {
    if (type == Type::Reference) {
        return const_cast<std::shared_ptr<StructObject>&>(asReference().get().asStruct());
    }
    if (type != Type::StructObject) {
        throw RuntimeError("Value is not a struct");
    }
    return std::get<std::shared_ptr<StructObject>>(data);
}

void check_struct_field_type(const std::string& type_name, const Value& value) {
    const Value& v = value.deref();

    if (type_name == "num") {
        if (!v.isNumber() && !v.isRational()) {
            throw RuntimeError("expected num, got " + v.type_name());
        }
        return;
    }
    if (type_name == "text") {
        if (!v.isString()) {
            throw RuntimeError("expected text, got " + v.type_name());
        }
        return;
    }
    if (type_name == "bool") {
        if (!v.isBool()) {
            throw RuntimeError("expected bool, got " + v.type_name());
        }
        return;
    }
    if (type_name == "nonetype") {
        if (!v.isNone()) {
            throw RuntimeError("expected nonetype, got " + v.type_name());
        }
        return;
    }
    if (type_name == "vector") {
        if (!v.isVector()) {
            throw RuntimeError("expected vector, got " + v.type_name());
        }
        return;
    }
    if (type_name == "table") {
        if (!v.isDictionary()) {
            throw RuntimeError("expected table, got " + v.type_name());
        }
        return;
    }

    throw RuntimeError("unknown type name: " + type_name);
}

Value make_struct_instance(const std::string& name, std::vector<Value> args) {
    const auto it = registry_mut().find(name);
    if (it == registry_mut().end()) {
        throw RuntimeError("unknown struct type: " + name);
    }

    const StructTypeDef& def = it->second;
    const size_t required = def.required_field_count();
    if (args.size() < required || args.size() > def.fields.size()) {
        throw RuntimeError(
            std::format(
                "struct {} expects between {} and {} argument(s), got {}",
                name,
                required,
                def.fields.size(),
                args.size()
            )
        );
    }

    auto instance = std::make_shared<StructObject>();
    instance->type = std::make_shared<StructTypeDef>(def);
    instance->slots.resize(def.fields.size());

    for (size_t i = 0; i < def.fields.size(); ++i) {
        if (i < args.size()) {
            if (def.typed && def.fields[i].has_type_annotation) {
                check_struct_field_type(def.fields[i].type_name, args[i]);
            }
            instance->slots[i] = args[i].deref();
        } else if (def.fields[i].has_default) {
            instance->slots[i] = def.fields[i].default_value.deref();
        } else {
            throw RuntimeError(
                std::format("struct {} missing value for field {}", name, def.fields[i].name)
            );
        }
    }

    return Value(instance);
}

void struct_set_field(Value& struct_val, const std::string& field_name, const Value& value) {
    Value& obj = struct_val.deref();
    if (obj.getType() != Value::Type::StructObject) {
        throw RuntimeError("field assignment requires a struct instance");
    }

    auto inst = obj.asStruct();
    for (size_t i = 0; i < inst->type->fields.size(); ++i) {
        if (inst->type->fields[i].name == field_name) {
            if (!inst->type->fields[i].mutable_field) {
                throw RuntimeError("cannot assign to immutable field: " + field_name);
            }
            if (inst->type->typed && inst->type->fields[i].has_type_annotation) {
                check_struct_field_type(inst->type->fields[i].type_name, value);
            }
            inst->slots[i] = value.deref();
            return;
        }
    }
    throw RuntimeError("struct has no field: " + field_name);
}

Value struct_get_field(const Value& struct_val, const std::string& field_name) {
    const Value& obj = struct_val.deref();
    if (obj.getType() != Value::Type::StructObject) {
        throw RuntimeError("field access requires a struct instance");
    }

    const auto inst = obj.asStruct();
    for (size_t i = 0; i < inst->type->fields.size(); ++i) {
        if (inst->type->fields[i].name == field_name) {
            return inst->slots[i];
        }
    }
    throw RuntimeError("struct has no field: " + field_name);
}
} // namespace irgen