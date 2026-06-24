#pragma once

#include "runtime_ast.hpp"
#include "opcode.hpp"

#include <vector>

namespace irgen {

[[nodiscard]] Value eval_ast_value(VM& vm, const RuntimeAstNode& node);

[[nodiscard]] RuntimeAstNode quote_ast(
    const std::vector<std::string>& hygienic_names,
    const std::vector<std::pair<std::string, RuntimeAstNode>>& captured_bindings,
    RuntimeAstNode body
);

[[nodiscard]] std::vector<Value> resolve_call_args_with_splat(
    VM& vm,
    const std::vector<Value>& stack_args,
    uint64_t splat_mask
);

[[nodiscard]] std::string binding_var_name_for_quote(const RuntimeAstNode& expr);

[[nodiscard]] Value capture_quote_binding_value(VM& vm, const RuntimeAstNode& expr);

[[nodiscard]] RuntimeAstNode value_to_quote_binding_ast(const Value& value);

[[nodiscard]] Value clone_ast_value(const Value& value);

[[nodiscard]] Value compose_ast_type_convert(const Value& type_ast, const Value& value_ast);

[[nodiscard]] Value compose_ast_func_call(const Value& callee_ast, const Value& args_vec);

[[nodiscard]] Value compose_ast_macro_call(const Value& callee_ast, const Value& args_vec);

[[nodiscard]] Value ast_struct_value(VM& vm, const Value& value);

} // namespace irgen
