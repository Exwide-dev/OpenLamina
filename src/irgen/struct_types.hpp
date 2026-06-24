#pragma once

#include "opcode.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace irgen {

[[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<StructTypeDef>>& type_registry();

[[nodiscard]] std::shared_ptr<StructTypeDef> get_type_def(const std::string& name);

[[nodiscard]] bool is_type_name(const std::string& name);

[[nodiscard]] std::shared_ptr<StructTypeDef> register_type_def(StructTypeDef def);

void register_builtin_types();

[[nodiscard]] Value make_type_value(const std::shared_ptr<StructTypeDef>& def);

[[nodiscard]] Value make_type_value(const std::string& name);

/** @brief Runtime type handle for a value (for type(obj) == SomeType). */
[[nodiscard]] Value runtime_type_of(const Value& value);

[[nodiscard]] std::optional<Value> type_get_attr(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& def,
    const std::string& attr_name
);

[[nodiscard]] Value type_call(
    VM& vm,
    const std::shared_ptr<StructTypeDef>& def,
    const std::vector<Value>& positional,
    const Value& kwargs = Value(std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>{})
);

/** @brief 按 type.__convert__.__dispatch__ 找最匹配 handler 并调用（不走 struct 构造） */
[[nodiscard]] Value convert_to_type(VM& vm, const Value& type_val, const Value& value);

[[nodiscard]] bool struct_instance_is_a(const Value& value, const std::string& type_name);

[[nodiscard]] int struct_type_match_depth(const Value& value, const std::string& type_name);

void check_struct_field_type(const std::string& type_name, const Value& value);

[[nodiscard]] Value make_struct_instance(
    VM& vm,
    const std::string& name,
    std::vector<Value> positional,
    const Value& kwargs = Value(std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>{})
);

void struct_set_field(VM& vm, Value& struct_val, const std::string& field_name, const Value& value);

[[nodiscard]] Value struct_get_field(const Value& struct_val, const std::string& field_name);

[[nodiscard]] std::optional<Value> struct_try_bind_method(
    VM& vm,
    const Value& receiver,
    const std::string& method_name
);

// 兼容旧名
[[nodiscard]] inline bool is_struct_type(const std::string& name) {
    return is_type_name(name);
}

[[nodiscard]] inline const std::unordered_map<std::string, std::shared_ptr<StructTypeDef>>& struct_registry() {
    return type_registry();
}

[[nodiscard]] inline std::shared_ptr<StructTypeDef> register_struct_type(StructTypeDef def) {
    return register_type_def(std::move(def));
}

} // namespace irgen
