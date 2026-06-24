#include "macro_ops.hpp"

#include "../irgen/generator.hpp"
#include "../tools/debug.hpp"

#include <unordered_map>

namespace irgen {
namespace {

std::string rename_hygienic(const std::string& name, const size_t index) {
    return std::format("__q_{}_{}", name, index);
}

void rename_in_node(RuntimeAstNode& node, const std::unordered_map<std::string, std::string>& rename_map) {
    if (node.kind == lmx::ASTNodeType::VarRef) {
        if (const auto it = rename_map.find(node.text); it != rename_map.end()) {
            node.text = it->second;
        }
    } else if (node.kind == lmx::ASTNodeType::VarDecl) {
        if (const auto it = rename_map.find(node.text); it != rename_map.end()) {
            node.text = it->second;
        }
    }
    for (auto& stmt : node.stmts) {
        rename_in_node(stmt, rename_map);
    }
    for (auto& child : node.children) {
        rename_in_node(child, rename_map);
    }
    for (auto& arg : node.call_args) {
        if (arg.value != nullptr) {
            rename_in_node(*arg.value, rename_map);
        }
    }
    if (node.slot_a != nullptr) {
        rename_in_node(*node.slot_a, rename_map);
    }
    if (node.slot_b != nullptr) {
        rename_in_node(*node.slot_b, rename_map);
    }
    if (node.slot_c != nullptr) {
        rename_in_node(*node.slot_c, rename_map);
    }
}

struct EvalScopeGuard {
    VM& vm;
    std::vector<SymbolTable> saved_scope;
    bool pushed_bindings = false;

    explicit EvalScopeGuard(VM& target) : vm(target) {
        LOG("[eval_scope] ctor sym=" << vm.symbol_stack.size()
            << " macro_eval_depth=" << vm.macro_eval_scope_stack.size());
        if (!vm.macro_eval_scope_stack.empty()) {
            saved_scope = std::move(vm.symbol_stack);
            vm.symbol_stack = vm.macro_eval_scope_stack.back();
            LOG("[eval_scope] swapped to caller sym=" << vm.symbol_stack.size());
        }
    }

    void push_quote_bindings(const RuntimeAstNode& quote_node) {
        if (quote_node.binding_names.empty()) {
            return;
        }
        vm.symbol_stack.emplace_back();
        vm.cache.enter_scope();
        pushed_bindings = true;
        LOG("[eval_scope] push bindings count=" << quote_node.binding_names.size()
            << " sym=" << vm.symbol_stack.size());
        for (size_t i = 0; i < quote_node.binding_names.size(); ++i) {
            vm.symbol_stack.back().set(
                g_string_pool.add(quote_node.binding_names[i]),
                make_ast_value(quote_node.bindings[i].clone()),
                vm.cell_pool
            );
        }
    }

    ~EvalScopeGuard() {
        LOG("[eval_scope] dtor begin sym=" << vm.symbol_stack.size()
            << " pushed_bindings=" << pushed_bindings
            << " saved=" << saved_scope.size()
            << " cache=" << vm.cache.scope_depth());
        if (pushed_bindings) {
            LOG("[eval_scope] dtor step1 pop binding scope sym=" << vm.symbol_stack.size());
            if (!vm.symbol_stack.empty()) {
                vm.symbol_stack.pop_back();
            }
            LOG("[eval_scope] dtor step2 cache.leave_scope depth=" << vm.cache.scope_depth());
            vm.cache.leave_scope();
            LOG("[eval_scope] dtor step2 done cache=" << vm.cache.scope_depth());
        }
        if (!saved_scope.empty()) {
            LOG("[eval_scope] dtor step3 restore saved_scope size=" << saved_scope.size());
            vm.symbol_stack = std::move(saved_scope);
            LOG("[eval_scope] dtor step3 done sym=" << vm.symbol_stack.size());
        }
        LOG("[eval_scope] dtor end");
    }
};

} // namespace

std::string binding_var_name_for_quote(const RuntimeAstNode& expr) {
    if (expr.kind == lmx::ASTNodeType::VarRef) {
        return expr.text;
    }
    throw RuntimeError("quote binding must be a simple identifier");
}

Value capture_quote_binding_value(VM& vm, const RuntimeAstNode& expr) {
    if (expr.kind != lmx::ASTNodeType::VarRef) {
        throw RuntimeError("quote binding must be a simple identifier");
    }
    const std::string& name = expr.text;

    if (!vm.locals_stack.empty() && !vm.call_func_stack.empty()) {
        const auto& func = vm.call_func_stack.back();
        for (size_t i = 0; i < func->params.size(); ++i) {
            if (func->params[i] == name && i < vm.locals_stack.back().size()) {
                return vm.locals_stack.back()[i];
            }
        }
    }

    if (const auto sym = vm.get_symbol(name)) {
        return *sym;
    }

    throw RuntimeError(std::format("quote: binding '{}' not found", name));
}

RuntimeAstNode value_to_quote_binding_ast(const Value& value) {
    const Value& v = value.deref();
    if (v.isRuntimeAst()) {
        return v.asRuntimeAst().clone();
    }
    if (v.isNumber()) {
        RuntimeAstNode out;
        out.kind = lmx::ASTNodeType::Number;
        out.text = v.asNumber().toString();
        return out;
    }
    if (v.isString()) {
        RuntimeAstNode out;
        out.kind = lmx::ASTNodeType::String;
        out.text = v.asString();
        return out;
    }
    if (v.isBool()) {
        RuntimeAstNode out;
        out.kind = lmx::ASTNodeType::Bool;
        out.bool_val = v.asBool();
        return out;
    }
    throw RuntimeError("quote binding value must be AST or literal");
}

std::vector<Value> resolve_call_args_with_splat(
    VM& vm,
    const std::vector<Value>& stack_args,
    const uint64_t splat_mask
) {
    (void)vm;
    std::vector<Value> expanded;
    expanded.reserve(stack_args.size());
    for (size_t i = 0; i < stack_args.size(); ++i) {
        const bool splat = i < 64 && ((splat_mask >> i) & 1ULL) != 0;
        if (!splat) {
            expanded.push_back(stack_args[i]);
            continue;
        }
        const Value& vec_val = stack_args[i].deref();
        if (vec_val.getType() != Value::Type::Vector) {
            throw RuntimeError("* argument unpacking requires a vec");
        }
        for (const auto& elem : vec_val.asVector()) {
            expanded.push_back(elem != nullptr ? *elem : Value());
        }
    }
    return expanded;
}

namespace {

[[nodiscard]] bool is_text_type_ast(const RuntimeAstNode& type_node) {
    return type_node.kind == lmx::ASTNodeType::VarRef && type_node.text == "text";
}

[[nodiscard]] bool is_type_convert_to_text(const RuntimeAstNode& node) {
    return node.kind == lmx::ASTNodeType::TypeConvert &&
           node.slot_a != nullptr &&
           is_text_type_ast(*node.slot_a);
}

[[nodiscard]] Value eval_type_convert_text(const RuntimeAstNode& node) {
    if (node.slot_b == nullptr) {
        throw RuntimeError("text.(...) missing value expression");
    }
    return Value(ast_to_source(*node.slot_b));
}

struct EvalAstTrace {
    const char* label;
    explicit EvalAstTrace(const char* l) : label(l) {
        LOG("[eval_ast] + " << label);
    }
    ~EvalAstTrace() {
        LOG("[eval_ast] - " << label);
    }
};

} // namespace

Value eval_ast_value(VM& vm, const RuntimeAstNode& node) {
    EvalAstTrace trace_enter("eval_ast_value");
    LOG("[eval_ast] kind=" << static_cast<int>(node.kind)
        << " bindings=" << node.binding_names.size()
        << " has_body=" << (node.slot_a != nullptr));
    const RuntimeAstNode* body = &node;
    if (node.kind == lmx::ASTNodeType::QuoteExpr && node.slot_a != nullptr) {
        body = &*node.slot_a;
    }

    if (is_type_convert_to_text(*body)) {
        LOG("[eval_ast] text.(ast) fast path");
        return eval_type_convert_text(*body);
    }

    TempParseTree tree;
    EvalAstTrace trace_tree("TempParseTree");
    lmx::ASTNode* parse_node = tree.to_parse_tree(*body);
    if (parse_node == nullptr) {
        throw RuntimeError("eval: empty AST");
    }
    LOG("[eval_ast] to_parse_tree ok");

    std::stack<lm::irgen::LoopLabels> loop_stack;
    lm::irgen::Stack<lm::irgen::LocalScope> local_scope_stack;
    std::vector<lm::irgen::FunctionContext> func_context_stack;
    local_scope_stack.emplace();

    const std::vector<std::string> source_lines;
    auto code = lm::irgen::gen_code(
        parse_node,
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );
    EvalAstTrace trace_code("gen_code");
    LOG("[eval_ast] gen_code size=" << code.size());

    if (code.empty()) {
        LOG("[eval_ast] empty code, returning none");
        return Value();
    }

    EvalScopeGuard scope_guard(vm);
    EvalAstTrace trace_guard("EvalScopeGuard");
    if (node.kind == lmx::ASTNodeType::QuoteExpr) {
        scope_guard.push_quote_bindings(node);
    }

    Value result = run_ir_snippet(vm, code);
    LOG("[eval_ast] run_ir_snippet result=" << result.deref().printString()
        << " op_stack=" << vm.op_stack.size());
    LOG("[eval_ast] about to unwind locals and return");
    return result;
}

RuntimeAstNode quote_ast(
    const std::vector<std::string>& hygienic_names,
    const std::vector<std::pair<std::string, RuntimeAstNode>>& captured_bindings,
    RuntimeAstNode body
) {
    std::unordered_map<std::string, std::string> rename_map;
    rename_map.reserve(hygienic_names.size());
    for (size_t i = 0; i < hygienic_names.size(); ++i) {
        rename_map[hygienic_names[i]] = rename_hygienic(hygienic_names[i], i);
    }
    rename_in_node(body, rename_map);

    RuntimeAstNode out;
    out.kind = lmx::ASTNodeType::QuoteExpr;
    out.hygienic_names = hygienic_names;
    out.binding_names.reserve(captured_bindings.size());
    out.bindings.reserve(captured_bindings.size());
    for (const auto& [name, value] : captured_bindings) {
        out.binding_names.push_back(name);
        out.bindings.push_back(value.clone());
    }
    out.slot_a = std::make_unique<RuntimeAstNode>(std::move(body));
    return out;
}

namespace {

RuntimeAstNode expect_ast_node(const Value& value, const char* context) {
    if (!value_is_ast(value)) {
        throw RuntimeError(std::format("{} expects an AST value", context));
    }
    return value_as_ast(value);
}

void push_call_arg(RuntimeAstNode& out, RuntimeAstNode arg) {
    RuntimeAstNode::CallArg ca;
    ca.value = std::make_unique<RuntimeAstNode>(std::move(arg));
    out.call_args.push_back(std::move(ca));
}

} // namespace

Value clone_ast_value(const Value& value) {
    return make_ast_value(expect_ast_node(value, "clone_ast"));
}

Value compose_ast_type_convert(const Value& type_ast, const Value& value_ast) {
    RuntimeAstNode out;
    out.kind = lmx::ASTNodeType::TypeConvert;
    out.slot_a = std::make_unique<RuntimeAstNode>(expect_ast_node(type_ast, "type_convert"));
    out.slot_b = std::make_unique<RuntimeAstNode>(expect_ast_node(value_ast, "type_convert"));
    return make_ast_value(std::move(out));
}

Value compose_ast_func_call(const Value& callee_ast, const Value& args_vec) {
    const Value& vec = args_vec.deref();
    if (vec.getType() != Value::Type::Vector) {
        throw RuntimeError("func_call AST composition expects a vec of AST nodes");
    }
    RuntimeAstNode out;
    out.kind = lmx::ASTNodeType::FuncCallExpr;
    out.slot_a = std::make_unique<RuntimeAstNode>(expect_ast_node(callee_ast, "func_call"));
    for (const auto& elem : vec.asVector()) {
        if (elem == nullptr || !value_is_ast(*elem)) {
            throw RuntimeError("func_call AST composition expects AST elements");
        }
        push_call_arg(out, value_as_ast(*elem));
    }
    return make_ast_value(std::move(out));
}

Value compose_ast_macro_call(const Value& callee_ast, const Value& args_vec) {
    const Value& vec = args_vec.deref();
    if (vec.getType() != Value::Type::Vector) {
        throw RuntimeError("macro_call AST composition expects a vec of AST nodes");
    }
    RuntimeAstNode out;
    out.kind = lmx::ASTNodeType::MacroCallExpr;
    out.slot_a = std::make_unique<RuntimeAstNode>(expect_ast_node(callee_ast, "macro_call"));
    for (const auto& elem : vec.asVector()) {
        if (elem == nullptr || !value_is_ast(*elem)) {
            throw RuntimeError("macro_call AST composition expects AST elements");
        }
        push_call_arg(out, value_as_ast(*elem));
    }
    return make_ast_value(std::move(out));
}

Value ast_struct_value(VM& vm, const Value& value) {
    if (!value_is_ast(value)) {
        throw RuntimeError("ast_struct expects an AST value");
    }
    return runtime_ast_to_struct(vm, value_as_ast(value));
}

} // namespace irgen
