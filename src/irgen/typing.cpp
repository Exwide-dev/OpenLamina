#include "typing.hpp"

#include "struct_types.hpp"

#include "../parser/ast.hpp"
#include "../tools/error.hpp"

#include <unordered_map>
#include <unordered_set>

namespace irgen {
namespace {

struct TypeVarType final : TypeDescriptor {
    std::string name;

    explicit TypeVarType(std::string n) : name(std::move(n)) {
    }

    [[nodiscard]] std::string repr() const override {
        return name;
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        (void)value;
        (void)vm;
        return false;
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        (void)other;
        (void)vm;
        return false;
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto other_name = other.type_var_name();
        return other_name.has_value() && other_name.value() == name;
    }

    [[nodiscard]] std::optional<std::string> type_var_name() const override {
        return name;
    }
};

struct NominalType final : TypeDescriptor {
    std::shared_ptr<StructTypeDef> def;

    explicit NominalType(std::shared_ptr<StructTypeDef> type_def)
        : def(std::move(type_def)) {
    }

    [[nodiscard]] std::string repr() const override {
        return def->name;
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        (void)vm;
        const Value& v = value.deref();
        if (def->kind == TypeKind::Primitive) {
            if (def->name == "num") {
                return v.isNumber() || v.isRational();
            }
            if (def->name == "text") {
                return v.isString();
            }
            if (def->name == "bool") {
                return v.isBool();
            }
            if (def->name == "nonetype") {
                return v.isNone();
            }
            if (def->name == "vector") {
                return v.isVector();
            }
            if (def->name == "table") {
                return v.isDictionary();
            }
            if (def->name == "AST") {
                return v.isRuntimeAst();
            }
            return false;
        }
        return struct_instance_is_a(value, def->name);
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        if (const auto* other_nominal = other.as_nominal_def()) {
            if (def->name == other_nominal->name) {
                return true;
            }
            if (other_nominal->kind == TypeKind::User && def->kind == TypeKind::User) {
                std::string current = def->name;
                while (true) {
                    if (current == other_nominal->name) {
                        return true;
                    }
                    const StructTypeDef* cur_def = get_type_def(current).get();
                    if (cur_def == nullptr || cur_def->base_name.empty()) {
                        return false;
                    }
                    current = cur_def->base_name;
                }
            }
            return false;
        }
        return other.accepts(make_type_value(std::make_shared<NominalType>(def)), vm);
    }

    [[nodiscard]] const StructTypeDef* as_nominal_def() const override {
        return def.get();
    }

    [[nodiscard]] const std::shared_ptr<StructTypeDef>* nominal_def_storage() const override {
        return &def;
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_nominal = other.as_nominal_def();
        return other_nominal != nullptr && def.get() == other_nominal;
    }
};

struct UnionType final : TypeDescriptor {
    std::vector<std::shared_ptr<TypeDescriptor>> members;

    explicit UnionType(std::vector<std::shared_ptr<TypeDescriptor>> ms)
        : members(std::move(ms)) {
        if (members.empty()) {
            throw RuntimeError("Union requires at least one type argument");
        }
    }

    [[nodiscard]] std::string repr() const override {
        std::string out = "Union[";
        for (size_t i = 0; i < members.size(); ++i) {
            if (i > 0) {
                out += ", ";
            }
            out += members[i]->repr();
        }
        out += "]";
        return out;
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        for (const auto& member : members) {
            if (member->accepts(value, vm)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        const auto* other_union = dynamic_cast<const UnionType*>(&other);
        if (other_union == nullptr) {
            return false;
        }
        for (const auto& member : members) {
            bool matched = false;
            for (const auto& other_member : other_union->members) {
                if (member->is_subtype_of(*other_member, vm)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_union = dynamic_cast<const UnionType*>(&other);
        if (other_union == nullptr || members.size() != other_union->members.size()) {
            return false;
        }
        for (size_t i = 0; i < members.size(); ++i) {
            if (!members[i]->structurally_equal(*other_union->members[i])) {
                return false;
            }
        }
        return true;
    }
};

struct MaybeType final : TypeDescriptor {
    std::shared_ptr<TypeDescriptor> inner;

    explicit MaybeType(std::shared_ptr<TypeDescriptor> t)
        : inner(std::move(t)) {
    }

    [[nodiscard]] std::string repr() const override {
        return "Maybe[" + inner->repr() + "]";
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        if (value.deref().isNone()) {
            return true;
        }
        return inner->accepts(value, vm);
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        const auto* other_maybe = dynamic_cast<const MaybeType*>(&other);
        if (other_maybe == nullptr) {
            return false;
        }
        return inner->is_subtype_of(*other_maybe->inner, vm);
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_maybe = dynamic_cast<const MaybeType*>(&other);
        return other_maybe != nullptr && inner->structurally_equal(*other_maybe->inner);
    }
};

struct VarianceType final : TypeDescriptor {
    VarianceMode mode;
    std::shared_ptr<TypeDescriptor> inner;

    VarianceType(const VarianceMode m, std::shared_ptr<TypeDescriptor> t)
        : mode(m), inner(std::move(t)) {
    }

    [[nodiscard]] std::string repr() const override {
        switch (mode) {
            case VarianceMode::Covariant:
                return "Covariant[" + inner->repr() + "]";
            case VarianceMode::Contravariant:
                return "Contravariant[" + inner->repr() + "]";
            case VarianceMode::Invariant:
                return "Invariant[" + inner->repr() + "]";
        }
        return inner->repr();
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        return inner->accepts(value, vm);
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        const auto* other_var = dynamic_cast<const VarianceType*>(&other);
        if (other_var == nullptr || other_var->mode != mode) {
            return false;
        }
        switch (mode) {
            case VarianceMode::Covariant:
                return inner->is_subtype_of(*other_var->inner, vm);
            case VarianceMode::Contravariant:
                return other_var->inner->is_subtype_of(*inner, vm);
            case VarianceMode::Invariant:
                return inner->is_subtype_of(*other_var->inner, vm)
                    && other_var->inner->is_subtype_of(*inner, vm);
        }
        return false;
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_var = dynamic_cast<const VarianceType*>(&other);
        return other_var != nullptr
            && other_var->mode == mode
            && inner->structurally_equal(*other_var->inner);
    }
};

struct VecType final : TypeDescriptor {
    std::shared_ptr<TypeDescriptor> elem;

    explicit VecType(std::shared_ptr<TypeDescriptor> element)
        : elem(std::move(element)) {
    }

    [[nodiscard]] std::string repr() const override {
        return "vec[" + elem->repr() + "]";
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        const Value& v = value.deref();
        if (!v.isVector()) {
            return false;
        }
        for (const auto& slot : v.asVector()) {
            if (!slot) {
                return false;
            }
            if (!elem->accepts(*slot, vm)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        const auto* other_vec = dynamic_cast<const VecType*>(&other);
        if (other_vec == nullptr) {
            return false;
        }
        return elem->is_subtype_of(*other_vec->elem, vm);
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_vec = dynamic_cast<const VecType*>(&other);
        return other_vec != nullptr && elem->structurally_equal(*other_vec->elem);
    }
};

struct ApplyType final : TypeDescriptor {
    std::shared_ptr<TypeDescriptor> head;
    std::vector<std::shared_ptr<TypeDescriptor>> args;

    ApplyType(
        std::shared_ptr<TypeDescriptor> h,
        std::vector<std::shared_ptr<TypeDescriptor>> a
    )
        : head(std::move(h)), args(std::move(a)) {
    }

    [[nodiscard]] std::string repr() const override {
        std::string out = head->repr() + "[";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                out += ", ";
            }
            out += args[i]->repr();
        }
        out += "]";
        return out;
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        if (head->is_callable()) {
            const auto resolved = head->instantiate(args, vm);
            return resolved->accepts(value, vm);
        }
        if (const auto* nominal = dynamic_cast<const NominalType*>(head.get())) {
            if (nominal->def->is_generic) {
                const Value& v = value.deref();
                if (v.getType() != Value::Type::StructObject) {
                    return false;
                }
                const std::string& actual = v.asStruct()->type->name;
                const std::string prefix = nominal->def->name + "[";
                return actual.starts_with(prefix) && actual.ends_with("]");
            }
        }
        return head->accepts(value, vm);
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        const auto* other_apply = dynamic_cast<const ApplyType*>(&other);
        if (other_apply == nullptr) {
            return false;
        }
        if (!type_descriptors_equal(*head, *other_apply->head)) {
            return false;
        }
        if (args.size() != other_apply->args.size()) {
            return false;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            if (!args[i]->is_subtype_of(*other_apply->args[i], vm)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_apply = dynamic_cast<const ApplyType*>(&other);
        if (other_apply == nullptr || args.size() != other_apply->args.size()) {
            return false;
        }
        if (!head->structurally_equal(*other_apply->head)) {
            return false;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            if (!args[i]->structurally_equal(*other_apply->args[i])) {
                return false;
            }
        }
        return true;
    }
};

struct TypeConstructor final : TypeDescriptor {
    std::string name;
    VarianceMode variance_mode = VarianceMode::Invariant;
    bool is_variance_wrapper = false;

    explicit TypeConstructor(std::string n) : name(std::move(n)) {
    }

    [[nodiscard]] std::string repr() const override {
        return name;
    }

    [[nodiscard]] bool accepts(const Value& value, VM& vm) const override {
        (void)value;
        (void)vm;
        return false;
    }

    [[nodiscard]] bool is_subtype_of(const TypeDescriptor& other, VM& vm) const override {
        (void)vm;
        const auto* other_ctor = dynamic_cast<const TypeConstructor*>(&other);
        return other_ctor != nullptr && other_ctor->name == name;
    }

    [[nodiscard]] bool is_callable() const override {
        return true;
    }

    [[nodiscard]] std::shared_ptr<TypeDescriptor> instantiate(
        const std::vector<std::shared_ptr<TypeDescriptor>>& type_args,
        VM& vm
    ) const override {
        (void)vm;
        if (name == "Union") {
            return std::make_shared<UnionType>(type_args);
        }
        if (name == "Maybe") {
            if (type_args.size() != 1) {
                throw RuntimeError("Maybe requires exactly 1 type argument");
            }
            return std::make_shared<MaybeType>(type_args[0]);
        }
        if (is_variance_wrapper) {
            if (type_args.size() != 1) {
                throw RuntimeError(name + " requires exactly 1 type argument");
            }
            return std::make_shared<VarianceType>(variance_mode, type_args[0]);
        }
        if (name == "vec") {
            if (type_args.size() != 1) {
                throw RuntimeError("vec requires exactly 1 type argument");
            }
            return std::make_shared<VecType>(type_args[0]);
        }
        return std::make_shared<ApplyType>(
            std::make_shared<TypeConstructor>(name),
            type_args
        );
    }

    [[nodiscard]] bool structurally_equal(const TypeDescriptor& other) const override {
        const auto* other_ctor = dynamic_cast<const TypeConstructor*>(&other);
        return other_ctor != nullptr && other_ctor->name == name;
    }
};

std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& constructor_registry() {
    static std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>> reg;
    return reg;
}

[[nodiscard]] std::string short_type_ctor_name(const std::string& name) {
    if (const size_t pos = name.rfind('.'); pos != std::string::npos) {
        return name.substr(pos + 1);
    }
    return name;
}

[[nodiscard]] bool is_known_type_constructor(const std::string& name) {
    const std::string short_name = short_type_ctor_name(name);
    return get_type_constructor(short_name) != nullptr
        || short_name == "Union" || short_name == "Maybe" || short_name == "vec"
        || short_name == "Covariant" || short_name == "Contravariant" || short_name == "Invariant";
}

[[nodiscard]] std::shared_ptr<TypeDescriptor> instantiate_composite_type(
    const std::string& name,
    std::vector<std::shared_ptr<TypeDescriptor>> args
) {
    const std::string short_name = short_type_ctor_name(name);
    if (short_name == "Union") {
        return std::make_shared<UnionType>(std::move(args));
    }
    if (short_name == "Maybe") {
        if (args.size() != 1) {
            throw RuntimeError("Maybe requires exactly 1 type argument");
        }
        return std::make_shared<MaybeType>(args[0]);
    }
    if (short_name == "Covariant") {
        if (args.size() != 1) {
            throw RuntimeError("Covariant requires exactly 1 type argument");
        }
        return std::make_shared<VarianceType>(VarianceMode::Covariant, args[0]);
    }
    if (short_name == "Contravariant") {
        if (args.size() != 1) {
            throw RuntimeError("Contravariant requires exactly 1 type argument");
        }
        return std::make_shared<VarianceType>(VarianceMode::Contravariant, args[0]);
    }
    if (short_name == "Invariant") {
        if (args.size() != 1) {
            throw RuntimeError("Invariant requires exactly 1 type argument");
        }
        return std::make_shared<VarianceType>(VarianceMode::Invariant, args[0]);
    }
    if (short_name == "vec") {
        if (args.size() != 1) {
            throw RuntimeError("vec requires exactly 1 type argument");
        }
        return std::make_shared<VecType>(args[0]);
    }
    if (const auto ctor = get_type_constructor(short_name)) {
        return std::make_shared<ApplyType>(ctor, std::move(args));
    }
    if (is_type_name(name)) {
        return std::make_shared<ApplyType>(
            make_nominal_type(get_type_def(name)),
            std::move(args)
        );
    }
    throw RuntimeError("unknown type constructor: " + name);
}

[[nodiscard]] std::shared_ptr<TypeDescriptor> materialize_type_node(
    const lmx::TypeNode* node,
    const std::unordered_set<std::string>& type_params
) {
    register_builtin_types();
    register_typing_constructors();
    if (node == nullptr) {
        throw RuntimeError("internal error: null type node");
    }

    if (node->kind == lmx::ASTNodeType::CompositeType) {
        const auto* composite = dynamic_cast<const lmx::CompositeTypeNode*>(node);
        std::vector<std::shared_ptr<TypeDescriptor>> args;
        args.reserve(composite->subtypes.size());
        for (const lmx::TypeNode* sub : composite->subtypes) {
            args.push_back(materialize_type_node(sub, type_params));
        }

        if (is_known_type_constructor(composite->name)) {
            return instantiate_composite_type(composite->name, std::move(args));
        }

        if (is_type_name(composite->name)) {
            return std::make_shared<ApplyType>(
                make_nominal_type(get_type_def(composite->name)),
                std::move(args)
            );
        }

        throw RuntimeError("unknown type constructor: " + composite->name);
    }

    if (type_params.contains(node->name)) {
        return std::make_shared<TypeVarType>(node->name);
    }

    if (is_type_name(node->name)) {
        return make_nominal_type(get_type_def(node->name));
    }

    if (const auto ctor = get_type_constructor(node->name)) {
        return ctor;
    }

    throw RuntimeError("unknown type: " + node->name);
}

[[nodiscard]] std::shared_ptr<TypeDescriptor> substitute_type_impl(
    const TypeDescriptor& desc,
    const std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& subst
) {
    if (const auto var_name = desc.type_var_name()) {
        const auto it = subst.find(var_name.value());
        if (it == subst.end()) {
            throw RuntimeError("unbound type variable: " + var_name.value());
        }
        return it->second;
    }

    if (const auto* union_type = dynamic_cast<const UnionType*>(&desc)) {
        std::vector<std::shared_ptr<TypeDescriptor>> members;
        members.reserve(union_type->members.size());
        for (const auto& member : union_type->members) {
            members.push_back(substitute_type_impl(*member, subst));
        }
        return std::make_shared<UnionType>(std::move(members));
    }

    if (const auto* maybe_type = dynamic_cast<const MaybeType*>(&desc)) {
        return std::make_shared<MaybeType>(substitute_type_impl(*maybe_type->inner, subst));
    }

    if (const auto* vec_type = dynamic_cast<const VecType*>(&desc)) {
        return std::make_shared<VecType>(substitute_type_impl(*vec_type->elem, subst));
    }

    if (const auto* variance_type = dynamic_cast<const VarianceType*>(&desc)) {
        return std::make_shared<VarianceType>(
            variance_type->mode,
            substitute_type_impl(*variance_type->inner, subst)
        );
    }

    if (const auto* apply_type = dynamic_cast<const ApplyType*>(&desc)) {
        std::vector<std::shared_ptr<TypeDescriptor>> args;
        args.reserve(apply_type->args.size());
        for (const auto& arg : apply_type->args) {
            args.push_back(substitute_type_impl(*arg, subst));
        }
        return std::make_shared<ApplyType>(
            substitute_type_impl(*apply_type->head, subst),
            std::move(args)
        );
    }

    if (const auto* nominal = dynamic_cast<const NominalType*>(&desc)) {
        return make_nominal_type(nominal->def);
    }

    if (const auto* ctor = dynamic_cast<const TypeConstructor*>(&desc)) {
        return std::make_shared<TypeConstructor>(ctor->name);
    }

    throw RuntimeError("cannot substitute in type: " + desc.repr());
}

[[nodiscard]] FunctionType make_check_type_method_impl(const std::shared_ptr<TypeDescriptor>& desc) {
    return FunctionType(
        [desc](VM& vm, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw RuntimeError("__check_type__ expects 1 argument");
            }
            return Value(desc->accepts(args[0], vm));
        }
    );
}

[[nodiscard]] bool type_descriptor_mentions_var_impl(
    const TypeDescriptor& desc,
    const std::string& var_name
) {
    if (const auto name = desc.type_var_name()) {
        return name.value() == var_name;
    }
    if (const auto* vec_type = dynamic_cast<const VecType*>(&desc)) {
        return type_descriptor_mentions_var_impl(*vec_type->elem, var_name);
    }
    if (const auto* apply_type = dynamic_cast<const ApplyType*>(&desc)) {
        for (const auto& arg : apply_type->args) {
            if (type_descriptor_mentions_var_impl(*arg, var_name)) {
                return true;
            }
        }
    }
    if (const auto* union_type = dynamic_cast<const UnionType*>(&desc)) {
        for (const auto& member : union_type->members) {
            if (type_descriptor_mentions_var_impl(*member, var_name)) {
                return true;
            }
        }
    }
    if (const auto* maybe_type = dynamic_cast<const MaybeType*>(&desc)) {
        return type_descriptor_mentions_var_impl(*maybe_type->inner, var_name);
    }
    if (const auto* variance_type = dynamic_cast<const VarianceType*>(&desc)) {
        return type_descriptor_mentions_var_impl(*variance_type->inner, var_name);
    }
    return false;
}

void unify_inferred_type(
    const std::string& var_name,
    const std::shared_ptr<TypeDescriptor>& value_type,
    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& inferred
) {
    const auto it = inferred.find(var_name);
    if (it != inferred.end()) {
        if (!it->second->structurally_equal(*value_type)) {
            throw RuntimeError(
                std::format("conflicting inferred types for type parameter '{}'", var_name)
            );
        }
        return;
    }
    inferred.emplace(var_name, value_type);
}

void infer_type_params_from_field_value_impl(
    const TypeDescriptor& field_type,
    const Value& arg,
    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& inferred,
    VM& vm
) {
    if (const auto var_name = field_type.type_var_name()) {
        unify_inferred_type(var_name.value(), value_type_descriptor(arg, vm), inferred);
        return;
    }
    if (const auto* vec_type = dynamic_cast<const VecType*>(&field_type)) {
        if (const auto elem_var = vec_type->elem->type_var_name()) {
            const Value& v = arg.deref();
            if (!v.isVector()) {
                return;
            }
            std::shared_ptr<TypeDescriptor> elem_type;
            for (const auto& slot : v.asVector()) {
                if (!slot) {
                    continue;
                }
                const std::shared_ptr<TypeDescriptor> current = value_type_descriptor(*slot, vm);
                if (!elem_type) {
                    elem_type = current;
                    continue;
                }
                if (!elem_type->structurally_equal(*current)) {
                    throw RuntimeError(
                        std::format(
                            "conflicting element types while inferring '{}'",
                            elem_var.value()
                        )
                    );
                }
            }
            if (elem_type) {
                unify_inferred_type(elem_var.value(), elem_type, inferred);
            }
        }
        return;
    }
}

} // namespace

FunctionType make_check_type_method(const std::shared_ptr<TypeDescriptor>& desc) {
    return make_check_type_method_impl(desc);
}

bool type_descriptor_mentions_var(const TypeDescriptor& desc, const std::string& var_name) {
    return type_descriptor_mentions_var_impl(desc, var_name);
}

void infer_type_params_from_field_value(
    const TypeDescriptor& field_type,
    const Value& arg,
    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& inferred,
    VM& vm
) {
    infer_type_params_from_field_value_impl(field_type, arg, inferred, vm);
}

std::shared_ptr<TypeDescriptor> TypeDescriptor::instantiate(
    const std::vector<std::shared_ptr<TypeDescriptor>>&,
    VM&
) const {
    throw RuntimeError("type " + repr() + " is not callable");
}

std::shared_ptr<TypeDescriptor> make_nominal_type(std::shared_ptr<StructTypeDef> def) {
    return std::make_shared<NominalType>(std::move(def));
}

std::shared_ptr<TypeDescriptor> make_union_type(std::vector<std::shared_ptr<TypeDescriptor>> members) {
    return std::make_shared<UnionType>(std::move(members));
}

std::shared_ptr<TypeDescriptor> make_maybe_type(std::shared_ptr<TypeDescriptor> inner) {
    return std::make_shared<MaybeType>(std::move(inner));
}

std::shared_ptr<TypeDescriptor> make_variance_type(
    const VarianceMode mode,
    std::shared_ptr<TypeDescriptor> inner
) {
    return std::make_shared<VarianceType>(mode, std::move(inner));
}

std::shared_ptr<TypeDescriptor> make_apply_type(
    std::shared_ptr<TypeDescriptor> head,
    std::vector<std::shared_ptr<TypeDescriptor>> args
) {
    return std::make_shared<ApplyType>(std::move(head), std::move(args));
}

std::shared_ptr<TypeDescriptor> get_type_constructor(const std::string& name) {
    const auto it = constructor_registry().find(name);
    if (it == constructor_registry().end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<TypeDescriptor> materialize_type(
    const lmx::TypeNode* node,
    const std::unordered_set<std::string>& type_params
) {
    return materialize_type_node(node, type_params);
}

void peel_param_bound(std::shared_ptr<TypeDescriptor>& bound, VarianceMode& variance) {
    if (const auto* variance_type = dynamic_cast<const VarianceType*>(bound.get())) {
        variance = variance_type->mode;
        bound = variance_type->inner;
    }
}

std::shared_ptr<TypeDescriptor> substitute_type(
    const TypeDescriptor& desc,
    const std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& subst
) {
    return substitute_type_impl(desc, subst);
}

std::shared_ptr<TypeDescriptor> value_type_descriptor(const Value& value, VM& vm) {
    return value_as_type_desc(runtime_type_of(value));
}

bool check_inferred_type_for_param(
    const TypeDescriptor& inferred,
    const TypeDescriptor& bound,
    const VarianceMode variance,
    const Value& sample_value,
    VM& vm
) {
    if (!bound.accepts(sample_value, vm)) {
        return false;
    }
    switch (variance) {
        case VarianceMode::Invariant:
            return true;
        case VarianceMode::Covariant:
            return inferred.is_subtype_of(bound, vm);
        case VarianceMode::Contravariant:
            return bound.is_subtype_of(inferred, vm);
    }
    return false;
}

std::string mangle_generic_name(
    const std::string& base,
    const std::vector<std::shared_ptr<TypeDescriptor>>& args
) {
    std::string out = base + "[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        out += args[i]->repr();
    }
    out += "]";
    return out;
}

Value make_type_value(const std::shared_ptr<TypeDescriptor>& desc) {
    return Value(desc);
}

const std::shared_ptr<TypeDescriptor>& value_as_type_desc(const Value& value) {
    return value.asTypeDesc();
}

bool type_descriptors_equal(const TypeDescriptor& a, const TypeDescriptor& b) {
    return a.structurally_equal(b);
}

bool type_handles_equal(
    const std::shared_ptr<TypeDescriptor>& a,
    const std::shared_ptr<TypeDescriptor>& b
) {
    if (!a || !b) {
        return a == b;
    }
    return a->structurally_equal(*b);
}

void check_value_matches_type(
    const std::shared_ptr<TypeDescriptor>& expected,
    const Value& value,
    VM& vm
) {
    if (!expected->accepts(value, vm)) {
        throw RuntimeError(
            std::format("expected {}, got {}", expected->repr(), value.deref().type_name())
        );
    }
}

std::vector<std::shared_ptr<TypeDescriptor>> type_args_from_values(
    VM& vm,
    const std::vector<Value>& args
) {
    (void)vm;
    std::vector<std::shared_ptr<TypeDescriptor>> out;
    out.reserve(args.size());
    for (const Value& arg : args) {
        const Value& v = arg.deref();
        if (!v.isTypeHandle()) {
            throw RuntimeError("type constructor arguments must be type objects");
        }
        out.push_back(v.asTypeDesc());
    }
    return out;
}

void register_typing_constructors() {
    auto& reg = constructor_registry();
    if (!reg.empty()) {
        return;
    }

    reg.emplace("Union", std::make_shared<TypeConstructor>("Union"));
    reg.emplace("Maybe", std::make_shared<TypeConstructor>("Maybe"));
    reg.emplace("vec", std::make_shared<TypeConstructor>("vec"));

    auto cov = std::make_shared<TypeConstructor>("Covariant");
    cov->variance_mode = VarianceMode::Covariant;
    cov->is_variance_wrapper = true;
    reg.emplace("Covariant", cov);

    auto contra = std::make_shared<TypeConstructor>("Contravariant");
    contra->variance_mode = VarianceMode::Contravariant;
    contra->is_variance_wrapper = true;
    reg.emplace("Contravariant", contra);

    auto inv = std::make_shared<TypeConstructor>("Invariant");
    inv->variance_mode = VarianceMode::Invariant;
    inv->is_variance_wrapper = true;
    reg.emplace("Invariant", inv);
}

} // namespace irgen
