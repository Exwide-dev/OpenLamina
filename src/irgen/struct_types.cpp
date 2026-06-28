#include "friend_function.hpp"

#include "struct_types.hpp"
#include "typing.hpp"

#include "exceptions.hpp"
#include "runtime_ast.hpp"
#include "../tools/error.hpp"

#include "friend_function.hpp"

#include "struct_types.hpp"
#include "typing.hpp"

#include "exceptions.hpp"
#include "runtime_ast.hpp"
#include "../tools/error.hpp"

namespace irgen {
namespace {

std::unordered_map<std::string, std::shared_ptr<StructTypeDef>>& registry_mut() {
    static std::unordered_map<std::string, std::shared_ptr<StructTypeDef>> reg;
    return reg;
}

const StructTypeDef* find_type_def_raw(const std::string& name) {
    const auto it = registry_mut().find(name);
    if (it == registry_mut().end()) {
        return nullptr;
    }
    return it->second.get();
}

bool struct_name_extends(const std::string& derived, const std::string& base) {
    const StructTypeDef* def = find_type_def_raw(derived);
    while (def != nullptr) {
        if (def->name == base) {
            return true;
        }
        if (def->base_name.empty()) {
            return false;
        }
        def = find_type_def_raw(def->base_name);
    }
    return false;
}

int struct_depth_from(const std::string& derived, const std::string& base) {
    int depth = 0;
    std::string current = derived;
    while (true) {
        if (current == base) {
            return depth;
        }
        const StructTypeDef* def = find_type_def_raw(current);
        if (def == nullptr || def->base_name.empty()) {
            return -1;
        }
        current = def->base_name;
        ++depth;
    }
}

Value store_field_value(VM& vm, const Value& value) {
    const Value& v = value.deref();
    if (v.isDictionary() || v.isVector()) {
        return Value::makeRef(vm.cell_pool.allocateCopy(v), vm.cell_pool);
    }
    return v;
}

Value convert_with_handlers(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& def,
    const Value& value
) {
    def->ensure_convert_func();
    const auto& handlers = def->convert_func->dispatch_handlers();
    const std::vector<Value> args = {make_type_value(def), value};
    const std::shared_ptr<FunctionObject> handler = find_convert_dispatch_handler(handlers, args);
    if (handler == nullptr) {
        throw RuntimeError(
            std::format(
                "cannot convert {} to {}",
                value.deref().type_name(),
                def->name
            )
        );
    }

    if (!handler->owner_vm) {
        handler->owner_vm = &vm;
    }
    return call_user_function_sync(
        vm,
        handler,
        resolve_user_function_args(vm, *handler, args, empty_kwargs_value())
    );
}

Value coerce_primitive(const std::shared_ptr<StructTypeDef>& def, const Value& value) {
    const Value& v = value.deref();

    if (def->name == "text") {
        if (v.isString()) {
            return Value(v.asString());
        }
        if (v.isNumber() || v.isRational() || v.isBool()) {
            return Value(v.printString());
        }
        if (v.isRuntimeAst()) {
            return Value(ast_to_source(v.asRuntimeAst()));
        }
    } else if (def->name == "num") {
        if (v.isNumber()) {
            return value;
        }
        if (v.isString()) {
            try {
                return Value(lang::lammp::Number(v.asString()));
            } catch (const std::exception&) {
                throw RuntimeError(
                    std::format("cannot convert \"{}\" to num", v.asString())
                );
            }
        }
    } else if (def->name == "bool") {
        if (v.isBool()) {
            return value;
        }
    }

    return Value();
}

std::shared_ptr<StructTypeDef> make_builtin_type(const std::string& name) {
    auto def = std::make_shared<StructTypeDef>();
    def->name = name;
    def->kind = TypeKind::Primitive;
    def->ensure_convert_func();
    return def;
}

std::vector<Value> resolve_init_args(
    VM& vm,
    const FunctionObject& init,
    const std::vector<Value>& positional,
    const Value& kwargs_value,
    const std::shared_ptr<Value>& self_cell
) {
    const size_t param_count = init.params.size();
    if (param_count == 0 || init.params[0] != "self") {
        throw RuntimeError("__init__ first parameter must be 'self'");
    }
    if (init.param_default_ir.size() != param_count) {
        throw RuntimeError("internal error: __init__ parameter metadata mismatch");
    }

    std::vector<Value> resolved(param_count);
    std::vector<bool> filled(param_count, false);
    resolved[0] = Value::makeRef(self_cell, vm.cell_pool);
    filled[0] = true;

    const size_t user_param_count = param_count - 1;
    if (positional.size() > user_param_count) {
        throw RuntimeError(
            std::format(
                "__init__ expects at most {} user argument(s), got {} positional",
                user_param_count,
                positional.size()
            )
        );
    }

    for (size_t i = 0; i < positional.size(); ++i) {
        resolved[i + 1] = positional[i];
        filled[i + 1] = true;
    }

    const Value& kwargs = kwargs_value.deref();
    if (kwargs.getType() == Value::Type::Dictionary) {
        for (const auto& [key_ptr, value_ptr] : kwargs.asDictionary()) {
            const Value& key = key_ptr->deref();
            if (key.getType() != Value::Type::String) {
                throw RuntimeError("keyword argument names must be strings");
            }
            const std::string& kw_name = key.asString();
            auto it = std::ranges::find(init.params, kw_name);
            if (it == init.params.end() || it == init.params.begin()) {
                throw RuntimeError(
                    std::format("__init__ has no parameter named '{}'", kw_name)
                );
            }
            const size_t index = static_cast<size_t>(std::distance(init.params.begin(), it));
            if (filled[index]) {
                throw RuntimeError(
                    std::format("multiple values for argument '{}'", kw_name)
                );
            }
            resolved[index] = *value_ptr;
            filled[index] = true;
        }
    }

    for (size_t i = 1; i < param_count; ++i) {
        if (filled[i]) {
            continue;
        }
        if (init.param_default_ir[i].empty()) {
            throw RuntimeError(
                std::format("__init__ missing required argument '{}'", init.params[i])
            );
        }
        resolved[i] = eval_param_default(vm, init.param_default_ir[i]);
    }

    return resolved;
}

} // namespace

const std::unordered_map<std::string, std::shared_ptr<StructTypeDef>>& type_registry() {
    return registry_mut();
}

std::shared_ptr<StructTypeDef> get_type_def(const std::string& name) {
    const auto it = registry_mut().find(name);
    if (it == registry_mut().end()) {
        throw RuntimeError("unknown type: " + name);
    }
    return it->second;
}

bool is_type_name(const std::string& name) {
    return registry_mut().contains(name);
}

void StructTypeDef::ensure_convert_func() {
    if (!convert_func) {
        convert_func = make_friend_function("__convert__");
    }
}

std::shared_ptr<StructTypeDef> register_type_def(StructTypeDef def) {
    auto ptr = std::make_shared<StructTypeDef>(std::move(def));
    ptr->ensure_convert_func();
    registry_mut()[ptr->name] = ptr;
    return ptr;
}

void register_builtin_types() {
    for (const char* name : {"text", "num", "bool", "nonetype", "vector", "table", "AST"}) {
        if (!registry_mut().contains(name)) {
            auto def = make_builtin_type(name);
            registry_mut()[def->name] = def;
        }
    }
    register_builtin_exceptions();
    register_ast_type_converters();
    register_ast_struct_types();
    register_typing_constructors();
}

Value make_type_value(const std::shared_ptr<StructTypeDef>& def) {
    return make_type_value(make_nominal_type(def));
}

Value make_type_value(const std::string& name) {
    return make_type_value(get_type_def(name));
}

Value runtime_type_of(const Value& value) {
    const Value& v = value.deref();
    switch (v.getType()) {
        case Value::Type::Reference: {
            const Ref& ref = v.asReference();
            if (ref.opaque) {
                throw RuntimeError("type() does not support opaque references");
            }
            return runtime_type_of(ref.get());
        }
        case Value::Type::TypeHandle:
            return Value(v.asTypeDesc());
        case Value::Type::StructObject:
            return make_type_value(v.asStruct()->type);
        case Value::Type::None:
            return make_type_value("nonetype");
        case Value::Type::String:
            return make_type_value("text");
        case Value::Type::Number:
        case Value::Type::Rational:
            return make_type_value("num");
        case Value::Type::Bool:
            return make_type_value("bool");
        case Value::Type::Vector:
            return make_type_value("vector");
        case Value::Type::Dictionary:
            return make_type_value("table");
        case Value::Type::RuntimeAst:
            return make_type_value("AST");
        default:
            throw RuntimeError(
                std::format("type() is not supported for {}", v.type_name())
            );
    }
}

std::optional<Value> type_get_attr(
    VM& vm,
    const std::shared_ptr<TypeDescriptor>& desc,
    const std::string& attr_name
) {
    if (attr_name == "__check_type__") {
        return Value(make_check_type_method(desc));
    }
    if (const StructTypeDef* def = desc->as_nominal_def()) {
        return type_get_attr(vm, get_type_def(def->name), attr_name);
    }
    (void)vm;
    return std::nullopt;
}

std::optional<Value> type_get_attr(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& def,
    const std::string& attr_name
) {
    if (attr_name == "__check_type__") {
        return Value(make_check_type_method(make_nominal_type(def)));
    }
    if (attr_name == "__convert__") {
        def->ensure_convert_func();
        return Value(def->convert_func);
    }
    return std::nullopt;
}

namespace {

void infer_type_var_from_field(
    const StructFieldDef& field,
    const Value& arg,
    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& inferred,
    VM& vm
) {
    if (!field.type_desc) {
        return;
    }
    infer_type_params_from_field_value(*field.type_desc, arg, inferred, vm);
}

std::shared_ptr<StructTypeDef> instantiate_generic_struct_def(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& template_def,
    const std::vector<Value>& positional,
    const Value& kwargs_value
) {
    (void)kwargs_value;
    const Value& kwargs = kwargs_value.deref();
    if (kwargs.isDictionary() && !kwargs.asDictionary().empty()) {
        throw RuntimeError("keyword arguments are not supported for generic struct instantiation");
    }

    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>> inferred;
    for (size_t i = 0; i < positional.size() && i < template_def->fields.size(); ++i) {
        infer_type_var_from_field(template_def->fields[i], positional[i], inferred, vm);
    }

    std::vector<std::shared_ptr<TypeDescriptor>> type_args;
    type_args.reserve(template_def->type_params.size());
    for (const StructTypeParamDef& param : template_def->type_params) {
        const auto it = inferred.find(param.name);
        if (it == inferred.end()) {
            throw RuntimeError(
                std::format("cannot infer type parameter '{}' for struct {}", param.name, template_def->name)
            );
        }
        for (size_t i = 0; i < template_def->fields.size() && i < positional.size(); ++i) {
            if (!template_def->fields[i].type_desc) {
                continue;
            }
            if (!type_descriptor_mentions_var(*template_def->fields[i].type_desc, param.name)) {
                continue;
            }
            Value sample = positional[i];
            const Value& sample_deref = sample.deref();
            if (sample_deref.isVector() && !sample_deref.asVector().empty() && sample_deref.asVector()[0]) {
                sample = *sample_deref.asVector()[0];
            }
            if (!check_inferred_type_for_param(
                    *it->second,
                    *param.bound,
                    param.variance,
                    sample,
                    vm
                )) {
                throw RuntimeError(
                    std::format(
                        "type parameter '{}' does not satisfy bound {} for struct {}",
                        param.name,
                        param.bound->repr(),
                        template_def->name
                    )
                );
            }
            break;
        }
        type_args.push_back(it->second);
    }

    const std::string mangled = mangle_generic_name(template_def->name, type_args);
    if (is_type_name(mangled)) {
        return get_type_def(mangled);
    }

    StructTypeDef instantiated;
    instantiated.name = mangled;
    instantiated.base_name = template_def->base_name;
    instantiated.kind = TypeKind::User;
    instantiated.typed = true;
    instantiated.is_generic = false;
    instantiated.methods = template_def->methods;

    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>> subst;
    for (size_t i = 0; i < template_def->type_params.size(); ++i) {
        subst.emplace(template_def->type_params[i].name, type_args[i]);
    }

    for (const StructFieldDef& field : template_def->fields) {
        StructFieldDef fd = field;
        if (field.type_desc) {
            fd.type_desc = substitute_type(*field.type_desc, subst);
            fd.type_name = fd.type_desc->repr();
        }
        instantiated.fields.push_back(std::move(fd));
    }

    return register_type_def(std::move(instantiated));
}

Value instantiate_generic_struct(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& template_def,
    const std::vector<Value>& positional,
    const Value& kwargs_value
) {
    const std::shared_ptr<StructTypeDef> concrete =
        instantiate_generic_struct_def(vm, template_def, positional, kwargs_value);
    return make_struct_instance(vm, concrete->name, positional, kwargs_value);
}

} // namespace

Value type_call(
    VM& vm,
    const std::shared_ptr<TypeDescriptor>& desc,
    const std::vector<Value>& positional,
    const Value& kwargs_value
) {
    if (desc->is_callable()) {
        const Value& kwargs = kwargs_value.deref();
        if (kwargs.isDictionary() && !kwargs.asDictionary().empty()) {
            throw RuntimeError("keyword arguments are not supported for type constructors");
        }
        const auto type_args = type_args_from_values(vm, positional);
        return make_type_value(desc->instantiate(type_args, vm));
    }

    if (const StructTypeDef* raw = desc->as_nominal_def()) {
        return type_call(vm, get_type_def(raw->name), positional, kwargs_value);
    }

    throw RuntimeError("type " + desc->repr() + " is not callable");
}

Value type_call(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& def,
    const std::vector<Value>& positional,
    const Value& kwargs_value
) {
    if (def->kind == TypeKind::Primitive) {
        if (positional.size() != 1) {
            throw RuntimeError(
                std::format("type {} expects 1 argument for conversion, got {}", def->name, positional.size())
            );
        }
        const Value& kwargs = kwargs_value.deref();
        if (kwargs.isDictionary() && !kwargs.asDictionary().empty()) {
            throw RuntimeError("keyword arguments are not supported for type conversion");
        }
        if (Value coerced = coerce_primitive(def, positional[0]); coerced.getType() != Value::Type::None) {
            return coerced;
        }
        return convert_with_handlers(vm, def, positional[0]);
    }

    if (def->is_generic) {
        return instantiate_generic_struct(vm, def, positional, kwargs_value);
    }

    return make_struct_instance(vm, def->name, positional, kwargs_value);
}

Value convert_to_type(VM& vm, const Value& type_val, const Value& value) {
    const Value& ty = type_val.deref();
    if (!ty.isTypeHandle()) {
        throw RuntimeError("convert() first argument must be a type");
    }
    const std::shared_ptr<TypeDescriptor>& desc = ty.asTypeDesc();
    if (const StructTypeDef* raw = desc->as_nominal_def()) {
        const std::shared_ptr<StructTypeDef>& def = get_type_def(raw->name);
        if (def->kind == TypeKind::Primitive) {
            if (Value coerced = coerce_primitive(def, value); coerced.getType() != Value::Type::None) {
                return coerced;
            }
        }
        return convert_with_handlers(vm, def, value);
    }
    throw RuntimeError(
        std::format("convert() does not support composite type {}", desc->repr())
    );
}

bool struct_instance_is_a(const Value& value, const std::string& type_name) {
    const Value& obj = value.deref();
    if (obj.getType() != Value::Type::StructObject) {
        return false;
    }
    const std::string& actual = obj.asStruct()->type->name;
    return actual == type_name || struct_name_extends(actual, type_name);
}

int struct_type_match_depth(const Value& value, const std::string& type_name) {
    const Value& obj = value.deref();
    if (type_name == "AST") {
        return obj.getType() == Value::Type::RuntimeAst ? 0 : -1;
    }
    if (obj.getType() != Value::Type::StructObject) {
        return -1;
    }
    return struct_depth_from(obj.asStruct()->type->name, type_name);
}

const std::shared_ptr<StructObject>& Value::asStruct() const {
    if (type == Type::Reference) {
        return asReference().get().asStruct();
    }
    if (type != Type::StructObject) {
        throw RuntimeError("Value is not a struct instance");
    }
    return std::get<std::shared_ptr<StructObject>>(data);
}

std::shared_ptr<StructObject>& Value::asStruct() {
    if (type == Type::Reference) {
        return const_cast<std::shared_ptr<StructObject>&>(asReference().get().asStruct());
    }
    if (type != Type::StructObject) {
        throw RuntimeError("Value is not a struct instance");
    }
    return std::get<std::shared_ptr<StructObject>>(data);
}

const std::shared_ptr<TypeDescriptor>& Value::asTypeDesc() const {
    if (type == Type::Reference) {
        return asReference().get().asTypeDesc();
    }
    if (type != Type::TypeHandle) {
        throw RuntimeError("Value is not a type handle");
    }
    return std::get<std::shared_ptr<TypeDescriptor>>(data);
}

std::shared_ptr<TypeDescriptor>& Value::asTypeDesc() {
    if (type == Type::Reference) {
        return const_cast<std::shared_ptr<TypeDescriptor>&>(asReference().get().asTypeDesc());
    }
    if (type != Type::TypeHandle) {
        throw RuntimeError("Value is not a type handle");
    }
    return std::get<std::shared_ptr<TypeDescriptor>>(data);
}

const std::shared_ptr<StructTypeDef>& Value::asTypeDef() const {
    if (type == Type::Reference) {
        return asReference().get().asTypeDef();
    }
    if (type != Type::TypeHandle) {
        throw RuntimeError("Value is not a type handle");
    }
    if (const std::shared_ptr<StructTypeDef>* storage = asTypeDesc()->nominal_def_storage()) {
        return *storage;
    }
    throw RuntimeError("type handle is not nominal: " + asTypeDesc()->repr());
}

std::shared_ptr<StructTypeDef>& Value::asTypeDef() {
    return const_cast<std::shared_ptr<StructTypeDef>&>(
        const_cast<const Value*>(this)->asTypeDef()
    );
}

void check_struct_field_type(const std::shared_ptr<TypeDescriptor>& type_desc, const Value& value, VM& vm) {
    check_value_matches_type(type_desc, value, vm);
}

void check_struct_field_type(const std::string& type_name, const Value& value) {
    if (is_type_name(type_name)) {
        static VM scratch{};
        check_struct_field_type(make_nominal_type(get_type_def(type_name)), value, scratch);
        return;
    }
    throw RuntimeError("expected " + type_name + ", got " + value.deref().type_name());
}

Value make_struct_instance(
    VM& vm,
    const std::string& name,
    std::vector<Value> positional,
    const Value& kwargs_value
) {
    const std::shared_ptr<StructTypeDef> def = get_type_def(name);
    if (def->kind == TypeKind::Primitive) {
        throw RuntimeError("cannot instantiate primitive type: " + name);
    }

    const auto init_it = def->methods.find("__init__");
    if (init_it != def->methods.end()) {
        auto instance = std::make_shared<StructObject>();
        instance->type = def;
        instance->slots.resize(def->fields.size());
        for (size_t i = 0; i < def->fields.size(); ++i) {
            if (def->fields[i].has_default) {
                instance->slots[i] = def->fields[i].default_value.deref();
            } else {
                instance->slots[i] = Value();
            }
        }

        const std::shared_ptr<Value> self_cell = vm.cell_pool.allocateCopy(Value(instance));
        const std::shared_ptr<FunctionObject>& init = init_it->second;
        if (!init->owner_vm) {
            init->owner_vm = &vm;
        }
        const std::vector<Value> init_args =
            resolve_init_args(vm, *init, positional, kwargs_value, self_cell);
        init->call(vm, init_args);
        return Value(instance);
    }

    const Value& kwargs = kwargs_value.deref();
    if (kwargs.getType() == Value::Type::Dictionary && !kwargs.asDictionary().empty()) {
        throw RuntimeError(
            std::format("struct {} does not define __init__ and cannot take keyword arguments", name)
        );
    }

    const size_t required = def->required_field_count();
    if (positional.size() < required || positional.size() > def->fields.size()) {
        throw RuntimeError(
            std::format(
                "struct {} expects between {} and {} argument(s), got {}",
                name,
                required,
                def->fields.size(),
                positional.size()
            )
        );
    }

    auto instance = std::make_shared<StructObject>();
    instance->type = def;
    instance->slots.resize(def->fields.size());

    for (size_t i = 0; i < def->fields.size(); ++i) {
        if (i < positional.size()) {
            if (def->typed && def->fields[i].has_type_annotation) {
                if (def->fields[i].type_desc) {
                    check_struct_field_type(def->fields[i].type_desc, positional[i], vm);
                } else {
                    check_struct_field_type(def->fields[i].type_name, positional[i]);
                }
            }
            instance->slots[i] = store_field_value(vm, positional[i]);
        } else if (def->fields[i].has_default) {
            instance->slots[i] = def->fields[i].default_value.deref();
        } else {
            throw RuntimeError(
                std::format("struct {} missing value for field {}", name, def->fields[i].name)
            );
        }
    }

    return Value(instance);
}

void struct_set_field(VM& vm, Value& struct_val, const std::string& field_name, const Value& value) {
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
                if (inst->type->fields[i].type_desc) {
                    check_struct_field_type(inst->type->fields[i].type_desc, value, vm);
                } else {
                    check_struct_field_type(inst->type->fields[i].type_name, value);
                }
            }
            inst->slots[i] = store_field_value(vm, value);
            return;
        }
    }
    if (inst->type->methods.contains(field_name)) {
        throw RuntimeError("cannot assign to method: " + field_name);
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

std::optional<Value> struct_try_bind_method(
    VM& vm,
    const Value& receiver,
    const std::string& method_name
) {
    const Value& obj = receiver.deref();
    if (obj.getType() != Value::Type::StructObject) {
        return std::nullopt;
    }

    const auto inst = obj.asStruct();
    const auto method_it = inst->type->methods.find(method_name);
    if (method_it == inst->type->methods.end()) {
        return std::nullopt;
    }

    const std::shared_ptr<FunctionObject>& method = method_it->second;
    std::shared_ptr<Value> self_cell;
    if (receiver.isReference()) {
        self_cell = receiver.asReference().value_ptr;
    } else {
        self_cell = vm.cell_pool.allocateCopy(obj);
    }

    return Value(FunctionType(
        [self_cell, method](VM& caller_vm, const std::vector<Value>& args) -> Value {
            if (!method->owner_vm) {
                method->owner_vm = &caller_vm;
            }

            const size_t user_argc = method->params.empty() || method->params[0] != "self"
                                         ? method->params.size()
                                         : method->params.size() - 1;
            if (args.size() != user_argc) {
                throw RuntimeError(
                    std::format(
                        "method {} expects {} argument(s), got {}",
                        method->name,
                        user_argc,
                        args.size()
                    )
                );
            }

            std::vector<Value> full_args;
            full_args.reserve(args.size() + 1);
            full_args.push_back(Value::makeRef(self_cell, caller_vm.cell_pool));
            full_args.insert(full_args.end(), args.begin(), args.end());
            return method->call(caller_vm, full_args);
        }
    ));
}

Value::Value(std::shared_ptr<TypeDescriptor> type_desc)
    : type(Type::TypeHandle), data(std::move(type_desc)) {
}

Value::Value(std::shared_ptr<StructTypeDef> type_def)
    : type(Type::TypeHandle), data(make_nominal_type(std::move(type_def))) {
}

} // namespace irgen
