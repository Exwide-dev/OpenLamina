#pragma once

#include "opcode.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace irgen {
[[nodiscard]] const std::unordered_map<std::string, StructTypeDef>& struct_registry();

void register_struct_type(StructTypeDef def);

[[nodiscard]] bool is_struct_type(const std::string& name);

void check_struct_field_type(const std::string& type_name, const Value& value);

[[nodiscard]] Value make_struct_instance(const std::string& name, std::vector<Value> args);

void struct_set_field(Value& struct_val, const std::string& field_name, const Value& value);

[[nodiscard]] Value struct_get_field(const Value& struct_val, const std::string& field_name);

[[nodiscard]] std::optional<Value> struct_try_bind_method(
    VM& vm,
    const Value& receiver,
    const std::string& method_name
);
} // namespace irgen