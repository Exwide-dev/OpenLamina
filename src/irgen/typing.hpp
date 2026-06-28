#pragma once

#include "opcode.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lmx {
struct TypeNode;
}

namespace irgen {

class VM;

/** @brief 运行时类型对象基类：值匹配与子类型关系由具体类型自己实现。 */
struct TypeDescriptor {
    virtual ~TypeDescriptor() = default;

    [[nodiscard]] virtual std::string repr() const = 0;

    [[nodiscard]] virtual bool accepts(const Value& value, VM& vm) const = 0;

    [[nodiscard]] virtual bool is_subtype_of(const TypeDescriptor& other, VM& vm) const = 0;

    [[nodiscard]] virtual bool structurally_equal(const TypeDescriptor& other) const = 0;

    [[nodiscard]] virtual bool is_callable() const {
        return false;
    }

    /** @brief 泛型构造：TypeConstructor[num, text] 或 Union(num, text) */
    [[nodiscard]] virtual std::shared_ptr<TypeDescriptor> instantiate(
        const std::vector<std::shared_ptr<TypeDescriptor>>& args,
        VM& vm
    ) const;

    [[nodiscard]] virtual const StructTypeDef* as_nominal_def() const {
        return nullptr;
    }

    [[nodiscard]] virtual const std::shared_ptr<StructTypeDef>* nominal_def_storage() const {
        return nullptr;
    }

    [[nodiscard]] virtual std::optional<std::string> type_var_name() const {
        return std::nullopt;
    }
};

enum class VarianceMode {
    Covariant,
    Contravariant,
    Invariant,
};

[[nodiscard]] std::shared_ptr<TypeDescriptor> make_nominal_type(std::shared_ptr<StructTypeDef> def);

[[nodiscard]] std::shared_ptr<TypeDescriptor> make_union_type(
    std::vector<std::shared_ptr<TypeDescriptor>> members
);

[[nodiscard]] std::shared_ptr<TypeDescriptor> make_maybe_type(std::shared_ptr<TypeDescriptor> inner);

[[nodiscard]] std::shared_ptr<TypeDescriptor> make_variance_type(
    VarianceMode mode,
    std::shared_ptr<TypeDescriptor> inner
);

[[nodiscard]] std::shared_ptr<TypeDescriptor> make_apply_type(
    std::shared_ptr<TypeDescriptor> head,
    std::vector<std::shared_ptr<TypeDescriptor>> args
);

/** @brief std.typing 导出的类型构造器（Union / Maybe / Covariant / …） */
[[nodiscard]] std::shared_ptr<TypeDescriptor> get_type_constructor(const std::string& name);

[[nodiscard]] std::shared_ptr<TypeDescriptor> materialize_type(
    const lmx::TypeNode* node,
    const std::unordered_set<std::string>& type_params = {}
);

void peel_param_bound(std::shared_ptr<TypeDescriptor>& bound, VarianceMode& variance);

[[nodiscard]] std::shared_ptr<TypeDescriptor> substitute_type(
    const TypeDescriptor& desc,
    const std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& subst
);

[[nodiscard]] std::shared_ptr<TypeDescriptor> value_type_descriptor(const Value& value, VM& vm);

[[nodiscard]] bool check_inferred_type_for_param(
    const TypeDescriptor& inferred,
    const TypeDescriptor& bound,
    VarianceMode variance,
    const Value& sample_value,
    VM& vm
);

[[nodiscard]] std::string mangle_generic_name(
    const std::string& base,
    const std::vector<std::shared_ptr<TypeDescriptor>>& args
);

[[nodiscard]] Value make_type_value(const std::shared_ptr<TypeDescriptor>& desc);

[[nodiscard]] const std::shared_ptr<TypeDescriptor>& value_as_type_desc(const Value& value);

[[nodiscard]] bool type_descriptors_equal(const TypeDescriptor& a, const TypeDescriptor& b);

[[nodiscard]] bool type_handles_equal(
    const std::shared_ptr<TypeDescriptor>& a,
    const std::shared_ptr<TypeDescriptor>& b
);

void check_value_matches_type(const std::shared_ptr<TypeDescriptor>& expected, const Value& value, VM& vm);

[[nodiscard]] FunctionType make_check_type_method(const std::shared_ptr<TypeDescriptor>& desc);

[[nodiscard]] bool type_descriptor_mentions_var(
    const TypeDescriptor& desc,
    const std::string& var_name
);

void infer_type_params_from_field_value(
    const TypeDescriptor& field_type,
    const Value& arg,
    std::unordered_map<std::string, std::shared_ptr<TypeDescriptor>>& inferred,
    VM& vm
);

[[nodiscard]] std::vector<std::shared_ptr<TypeDescriptor>> type_args_from_values(
    VM& vm,
    const std::vector<Value>& args
);

void register_typing_constructors();

} // namespace irgen
