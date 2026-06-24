#include "generator.hpp"

#include <expected>

#include "../tools/debug.hpp"
#include "../tools/lang/builtins.hpp"
#include "optimizer.hpp"

#include <format>
#include <iostream>
#include <ranges>
#include <stack>
#include <unordered_set>

#include "opcode.hpp"
#include "struct_types.hpp"
#include "friend_function.hpp"
#include "runtime_ast.hpp"
#include "macro_ops.hpp"

namespace {
::irgen::Value literal_to_value(const lmx::ExprNode* expr) {
    if (!expr) {
        throw RuntimeError("struct field default must be a literal");
    }
    if (expr->kind == lmx::ASTNodeType::Number) {
        const auto* num_node = dynamic_cast<const lmx::NumberNode*>(expr);
        return ::irgen::Value(lang::lammp::Number(num_node->value));
    }
    if (expr->kind == lmx::ASTNodeType::String) {
        const auto* str_node = dynamic_cast<const lmx::StringNode*>(expr);
        return ::irgen::Value(str_node->value);
    }
    if (expr->kind == lmx::ASTNodeType::Bool) {
        const auto* bool_node = dynamic_cast<const lmx::BoolNode*>(expr);
        return ::irgen::Value(bool_node->value);
    }
    throw RuntimeError("struct field default must be a literal");
}
} // namespace

namespace {
void apply_param_types(
    std::shared_ptr<::irgen::FunctionObject>& func_obj,
    const std::vector<lmx::FuncParam>& params
) {
    func_obj->param_types.resize(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
        if (params[i].has_type) {
            func_obj->param_types[i] = params[i].type_name;
        } else {
            func_obj->param_types[i] = std::nullopt;
        }
    }
}

void collectVarRefs(const lmx::ASTNode* node, std::unordered_set<std::string>& out) {
    if (!node) {
        return;
    }
    if (node->kind == lmx::ASTNodeType::VarRef) {
        out.insert(dynamic_cast<const lmx::VarRefNode*>(node)->name);
    }
    for (const auto* child : node->children) {
        collectVarRefs(child, out);
    }
}

bool iterableDependsOnPriorVars(
    const lmx::ExprNode* iterable,
    const std::vector<std::string>& prior_vars
) {
    std::unordered_set<std::string> refs;
    collectVarRefs(iterable, refs);
    for (const auto& name : prior_vars) {
        if (refs.contains(name)) {
            return true;
        }
    }
    return false;
}

void emit_bind_fast(std::vector<::irgen::Opcode>& code, const size_t slot, const std::string& name) {
    code.emplace_back(::irgen::BIND_FAST(slot, name));
}

std::string line_text(const lmx::ASTNode* node, const std::vector<std::string>& source_lines) {
    if (node == nullptr || node->source_line <= 0) {
        return {};
    }
    const auto idx = static_cast<size_t>(node->source_line - 1);
    if (idx >= source_lines.size()) {
        return {};
    }
    return source_lines[idx];
}

template<typename Op>
void push_op(std::vector<::irgen::Opcode>& code, const std::string& line, const int line_no, Op op) {
    if (!line.empty()) {
        op.line = line;
    }
    if (line_no > 0) {
        op.line_no = line_no;
    }
    code.emplace_back(std::move(op));
}

struct ResolvedVar {
    bool found = false;
    size_t slot = 0;
    size_t define_depth = 0;
};

[[nodiscard]] ResolvedVar resolve_var(
    const std::string& name,
    const lm::irgen::Stack<lm::irgen::LocalScope>& local_scope_stack
) {
    ResolvedVar result;
    const auto& container = local_scope_stack.get_container();
    for (int i = static_cast<int>(container.size()) - 1; i >= 0; --i) {
        const auto& scope = container[static_cast<size_t>(i)];
        if (const auto loc = scope.get_location(name)) {
            result.found = true;
            result.slot = loc->slot;
            result.define_depth = loc->define_depth;
            return result;
        }
    }
    return result;
}

[[nodiscard]] bool uses_fast_local(const ResolvedVar& var) {
    return var.found && var.define_depth > 0;
}

[[nodiscard]] bool compiling_in_macro(const std::vector<lm::irgen::FunctionContext>& func_context_stack) {
    return !func_context_stack.empty() && func_context_stack.back().is_macro;
}

[[nodiscard]] bool is_materializable_macro_local(
    const ResolvedVar& var,
    const std::vector<lm::irgen::FunctionContext>& func_context_stack
) {
    if (!var.found || !uses_fast_local(var) || func_context_stack.empty()) {
        return false;
    }
    const lm::irgen::FunctionContext& ctx = func_context_stack.back();
    return ctx.is_macro && var.define_depth == ctx.func_depth;
}

void gen_materialized_ast_expr(
    lmx::ExprNode* expr,
    std::vector<::irgen::Opcode>& code,
    std::stack<lm::irgen::LoopLabels>& loop_stack,
    lm::irgen::Stack<lm::irgen::LocalScope>& local_scope_stack,
    std::vector<lm::irgen::FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    const std::string src = line_text(expr, source_lines);
    const int src_line_no = expr != nullptr ? expr->source_line : 0;

    if (expr == nullptr) {
        throw RuntimeError("macro argument expression is null");
    }

    switch (expr->kind) {
        case lmx::ASTNodeType::Number:
        case lmx::ASTNodeType::String:
        case lmx::ASTNodeType::Bool:
        case lmx::ASTNodeType::VarRef:
        case lmx::ASTNodeType::Unary:
        case lmx::ASTNodeType::Binary:
        case lmx::ASTNodeType::MemberAccess:
        case lmx::ASTNodeType::IndexAccess:
        case lmx::ASTNodeType::Vector:
        case lmx::ASTNodeType::QuoteExpr: {
            if (expr->kind == lmx::ASTNodeType::VarRef) {
                const auto* var_ref = dynamic_cast<lmx::VarRefNode*>(expr);
                const ResolvedVar var = resolve_var(var_ref->name, local_scope_stack);
                if (is_materializable_macro_local(var, func_context_stack)) {
                    push_op(code, src, src_line_no, ::irgen::LOAD_FAST(var.slot));
                    push_op(code, src, src_line_no, ::irgen::DEREF());
                    push_op(code, src, src_line_no, ::irgen::LOAD("__ast_clone__"));
                    push_op(code, src, src_line_no, ::irgen::CALL(1));
                    return;
                }
            }
            push_op(
                code,
                src,
                src_line_no,
                ::irgen::PUSH(::irgen::make_ast_value(::irgen::ast_from_parse(expr)))
            );
            return;
        }
        case lmx::ASTNodeType::TypeConvert: {
            const auto* convert = dynamic_cast<lmx::TypeConvertExprNode*>(expr);
            gen_materialized_ast_expr(
                convert->value_expr,
                code,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            gen_materialized_ast_expr(
                convert->type_expr,
                code,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            push_op(code, src, src_line_no, ::irgen::LOAD("__ast_type_convert__"));
            push_op(code, src, src_line_no, ::irgen::CALL(2));
            return;
        }
        case lmx::ASTNodeType::FuncCallExpr:
        case lmx::ASTNodeType::MacroCallExpr: {
            std::vector<lmx::CallArgument> call_args;
            lmx::ExprNode* callee_expr = nullptr;
            if (expr->kind == lmx::ASTNodeType::MacroCallExpr) {
                const auto* macro_call = dynamic_cast<lmx::MacroCallExprNode*>(expr);
                call_args = macro_call->args;
                callee_expr = macro_call->macro_expr;
            } else {
                const auto* func_call = dynamic_cast<lmx::FuncCallExprNode*>(expr);
                call_args = func_call->args;
                callee_expr = func_call->func_expr;
            }

            bool has_splat = false;
            for (const auto& arg : call_args) {
                if (arg.is_splat) {
                    has_splat = true;
                    break;
                }
            }

            if (has_splat) {
                push_op(code, src, src_line_no, ::irgen::VEC_NEW(0));

                for (const auto& arg : call_args) {
                    if (arg.is_splat) {
                        gen_materialized_ast_expr(
                            arg.value,
                            code,
                            loop_stack,
                            local_scope_stack,
                            func_context_stack,
                            source_lines
                        );
                        push_op(code, src, src_line_no, ::irgen::LOAD("__ast_vec_extend__"));
                        push_op(code, src, src_line_no, ::irgen::CALL(2));
                    } else {
                        gen_materialized_ast_expr(
                            arg.value,
                            code,
                            loop_stack,
                            local_scope_stack,
                            func_context_stack,
                            source_lines
                        );
                        push_op(code, src, src_line_no, ::irgen::LOAD("__ast_vec_push__"));
                        push_op(code, src, src_line_no, ::irgen::CALL(2));
                    }
                }
            } else {
                for (auto it = call_args.rbegin(); it != call_args.rend(); ++it) {
                    gen_materialized_ast_expr(
                        it->value,
                        code,
                        loop_stack,
                        local_scope_stack,
                        func_context_stack,
                        source_lines
                    );
                }
                push_op(code, src, src_line_no, ::irgen::VEC_NEW(call_args.size()));
            }

            gen_materialized_ast_expr(
                callee_expr,
                code,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            push_op(
                code,
                src,
                src_line_no,
                ::irgen::LOAD(
                    expr->kind == lmx::ASTNodeType::MacroCallExpr
                        ? "__ast_macro_call__"
                        : "__ast_func_call__"
                )
            );
            push_op(code, src, src_line_no, ::irgen::CALL(2));
            return;
        }
        default:
            throw RuntimeError(
                std::format(
                    "unsupported expression in macro argument AST materialization: {}",
                    static_cast<int>(expr->kind)
                )
            );
    }
}
} // namespace

namespace lm::irgen {
thread_local size_t label_counter = 0;

std::vector<::irgen::Opcode> gen_code(
    lmx::ASTNode* node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
);

namespace {

void emit_load_match_subject(std::vector<::irgen::Opcode>& code, const std::string& temp_name) {
    code.emplace_back(::irgen::LOAD(temp_name));
    code.emplace_back(::irgen::DEREF());
}

void emit_load_match_at_path(
    std::vector<::irgen::Opcode>& code,
    const std::string& temp_name,
    const std::vector<size_t>& path
) {
    emit_load_match_subject(code, temp_name);
    for (const size_t idx : path) {
        code.emplace_back(::irgen::PUSH(::irgen::Value(static_cast<int64_t>(idx))));
        code.emplace_back(::irgen::INDEX());
        code.emplace_back(::irgen::DEREF());
    }
}

void emit_short_circuit_and(
    std::vector<::irgen::Opcode>& code,
    const lmx::BinaryNode* bin_node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines,
    const std::string& src,
    const int src_line_no
) {
    const std::string temp = std::format("__sc_and_{}", label_counter++);
    auto left_code = gen_code(
        bin_node->left,
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );
    code.insert(code.end(), left_code.begin(), left_code.end());
    push_op(code, src, src_line_no, ::irgen::NEW_INTERN_VAR(temp));
    push_op(code, src, src_line_no, ::irgen::STORE());
    push_op(code, src, src_line_no, ::irgen::LOAD(temp));

    const size_t false_label = label_counter++;
    const size_t end_label = label_counter++;
    code.emplace_back(::irgen::GOTOIFNOT(false_label));

    auto right_code = gen_code(
        bin_node->right,
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );
    code.insert(code.end(), right_code.begin(), right_code.end());
    code.emplace_back(::irgen::GOTO(end_label));
    code.emplace_back(::irgen::LABEL(false_label));
    push_op(code, src, src_line_no, ::irgen::LOAD(temp));
    code.emplace_back(::irgen::LABEL(end_label));
}

void emit_short_circuit_or(
    std::vector<::irgen::Opcode>& code,
    const lmx::BinaryNode* bin_node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines,
    const std::string& src,
    const int src_line_no
) {
    const std::string temp = std::format("__sc_or_{}", label_counter++);
    auto left_code = gen_code(
        bin_node->left,
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );
    code.insert(code.end(), left_code.begin(), left_code.end());
    push_op(code, src, src_line_no, ::irgen::NEW_INTERN_VAR(temp));
    push_op(code, src, src_line_no, ::irgen::STORE());
    push_op(code, src, src_line_no, ::irgen::LOAD(temp));

    const size_t true_label = label_counter++;
    const size_t end_label = label_counter++;
    code.emplace_back(::irgen::GOTOIF(true_label));

    auto right_code = gen_code(
        bin_node->right,
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );
    code.insert(code.end(), right_code.begin(), right_code.end());
    code.emplace_back(::irgen::GOTO(end_label));
    code.emplace_back(::irgen::LABEL(true_label));
    push_op(code, src, src_line_no, ::irgen::LOAD(temp));
    code.emplace_back(::irgen::LABEL(end_label));
}

void emit_match_pattern_test(
    std::vector<::irgen::Opcode>& code,
    const lmx::MatchPatternNode* pattern,
    const std::string& temp_name,
    const std::vector<size_t>& path,
    const size_t fail_label,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    if (pattern->pattern_kind == lmx::MatchPatternKind::Bind) {
        return;
    }

    if (pattern->pattern_kind == lmx::MatchPatternKind::Expr) {
        emit_load_match_at_path(code, temp_name, path);
        auto expr_code = gen_code(
            pattern->expr,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), expr_code.begin(), expr_code.end());
        code.emplace_back(::irgen::MATCH_EQ());
        code.emplace_back(::irgen::GOTOIFNOT(fail_label));
        return;
    }

    if (pattern->pattern_kind == lmx::MatchPatternKind::Vector) {
        emit_load_match_at_path(code, temp_name, path);
        code.emplace_back(::irgen::IS_VECTOR());
        code.emplace_back(::irgen::GOTOIFNOT(fail_label));

        emit_load_match_at_path(code, temp_name, path);
        code.emplace_back(::irgen::VEC_LEN());
        code.emplace_back(::irgen::PUSH(::irgen::Value(static_cast<int64_t>(pattern->elements.size()))));
        code.emplace_back(::irgen::EQ());
        code.emplace_back(::irgen::GOTOIFNOT(fail_label));

        for (size_t i = 0; i < pattern->elements.size(); ++i) {
            std::vector<size_t> child_path = path;
            child_path.push_back(i);
            emit_match_pattern_test(
                code,
                pattern->elements[i],
                temp_name,
                child_path,
                fail_label,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
        }
        return;
    }

    if (pattern->pattern_kind == lmx::MatchPatternKind::Struct) {
        emit_load_match_at_path(code, temp_name, path);
        code.emplace_back(::irgen::IS_INSTANCE(pattern->struct_type_name));
        code.emplace_back(::irgen::GOTOIFNOT(fail_label));
        return;
    }

    throw RuntimeError("Invalid match pattern in test");
}

void emit_match_pattern_bindings(
    std::vector<::irgen::Opcode>& code,
    const lmx::MatchPatternNode* pattern,
    const std::string& temp_name,
    const std::vector<size_t>& path,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    if (pattern->pattern_kind == lmx::MatchPatternKind::Bind) {
        emit_load_match_at_path(code, temp_name, path);
        code.emplace_back(::irgen::NEW_VAR(pattern->bind_name));
        code.emplace_back(::irgen::STORE());
        return;
    }

    if (pattern->pattern_kind == lmx::MatchPatternKind::Vector) {
        for (size_t i = 0; i < pattern->elements.size(); ++i) {
            std::vector<size_t> child_path = path;
            child_path.push_back(i);
            emit_match_pattern_bindings(
                code,
                pattern->elements[i],
                temp_name,
                child_path,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
        }
        return;
    }

    if (pattern->pattern_kind == lmx::MatchPatternKind::Struct) {
        for (const auto& field_name : pattern->struct_field_binds) {
            emit_load_match_at_path(code, temp_name, path);
            code.emplace_back(::irgen::GETATTR(field_name));
            code.emplace_back(::irgen::NEW_VAR(field_name));
            code.emplace_back(::irgen::STORE());
        }
    }
}

void emit_match_case(
    std::vector<::irgen::Opcode>& code,
    const lmx::MatchCaseNode* match_case,
    const std::string& temp_name,
    const size_t next_case_label,
    const size_t end_label,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    const auto* pattern = match_case->pattern;

    if (pattern->pattern_kind == lmx::MatchPatternKind::Or) {
        std::vector<size_t> body_labels;
        body_labels.reserve(pattern->alternatives.size());

        for (const auto* alt : pattern->alternatives) {
            const size_t body_label = label_counter++;
            body_labels.push_back(body_label);
            const size_t fail_label = label_counter++;
            emit_match_pattern_test(
                code,
                alt,
                temp_name,
                {},
                fail_label,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.emplace_back(::irgen::GOTO(body_label));
            code.emplace_back(::irgen::LABEL(fail_label));
        }

        code.emplace_back(::irgen::GOTO(next_case_label));

        for (size_t i = 0; i < pattern->alternatives.size(); ++i) {
            code.emplace_back(::irgen::LABEL(body_labels[i]));
            emit_match_pattern_bindings(
                code,
                pattern->alternatives[i],
                temp_name,
                {},
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            if (match_case->body) {
                auto body_code = gen_code(
                    match_case->body,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), body_code.begin(), body_code.end());
            }
            code.emplace_back(::irgen::GOTO(end_label));
        }
        return;
    }

    const size_t fail_label = label_counter++;
    const size_t body_label = label_counter++;

    emit_match_pattern_test(
        code,
        pattern,
        temp_name,
        {},
        fail_label,
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );
    code.emplace_back(::irgen::GOTO(body_label));
    code.emplace_back(::irgen::LABEL(fail_label));
    code.emplace_back(::irgen::GOTO(next_case_label));
    code.emplace_back(::irgen::LABEL(body_label));

    emit_match_pattern_bindings(
        code,
        pattern,
        temp_name,
        {},
        loop_stack,
        local_scope_stack,
        func_context_stack,
        source_lines
    );

    if (match_case->body) {
        auto body_code = gen_code(
            match_case->body,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), body_code.begin(), body_code.end());
    }
    code.emplace_back(::irgen::GOTO(end_label));
}

} // namespace

std::shared_ptr<::irgen::FunctionObject> compile_function_decl(
    const lmx::FuncDeclNode* func_decl_node,
    std::vector<::irgen::Opcode>& code,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines,
    const std::string& src,
    int src_line_no,
    bool store_in_global_scope
) {
    const size_t func_label = label_counter++;
    const size_t func_end_label = label_counter++;

    auto func_obj = std::make_shared<::irgen::FunctionObject>();
    func_obj->params.clear();
    func_obj->param_default_ir.clear();
    for (const auto& param : func_decl_node->params) {
        func_obj->params.push_back(param.name);
        if (param.default_value != nullptr) {
            auto default_code = gen_code(
                param.default_value,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            func_obj->param_default_ir.push_back(std::move(default_code));
        } else {
            func_obj->param_default_ir.emplace_back();
        }
    }
    func_obj->location = func_label;
    func_obj->name = func_decl_node->name;
    apply_param_types(func_obj, func_decl_node->params);

    local_scope_stack.emplace();
    local_scope_stack.top().depth = local_scope_stack.size() - 1;

    func_context_stack.emplace_back();
    func_context_stack.back().func_depth = local_scope_stack.top().depth;

    std::vector<size_t> param_slots;
    param_slots.reserve(func_decl_node->params.size());
    for (const auto& param : func_decl_node->params) {
        param_slots.push_back(
            local_scope_stack.top().allocate_slot(param.name, false, lmx::Visibility::Internal)
        );
    }

    const size_t self_slot = local_scope_stack.top().allocate_slot(
        func_decl_node->name,
        false,
        lmx::Visibility::Internal
    );

    std::vector<::irgen::Opcode> body_code;
    if (func_decl_node->body) {
        body_code = gen_code(func_decl_node->body, loop_stack, local_scope_stack, func_context_stack, source_lines);
    }

    func_obj->needs_closure = func_context_stack.back().needs_closure;
    func_obj->needs_symbol_bind = func_context_stack.back().needs_symbol_bind;
    func_context_stack.pop_back();
    const bool bind_symbols = func_obj->needs_closure || func_obj->needs_symbol_bind;

    code.emplace_back(::irgen::GOTO(func_end_label));
    code.emplace_back(::irgen::LABEL(func_label));
    code.emplace_back(::irgen::ENTER_SCOPE());

    for (size_t i = 0; i < func_decl_node->params.size(); ++i) {
        const auto& param = func_decl_node->params[i];
        code.emplace_back(::irgen::STORE_FAST(param_slots[i]));
        if (bind_symbols) {
            emit_bind_fast(code, param_slots[i], param.name);
        }
    }

    push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(func_obj)));
    push_op(code, src, src_line_no, ::irgen::STORE_FAST(self_slot));
    if (bind_symbols) {
        emit_bind_fast(code, self_slot, func_decl_node->name);
    }

    code.insert(code.end(), body_code.begin(), body_code.end());

    code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
    local_scope_stack.pop();

    code.emplace_back(::irgen::LABEL(func_end_label));

    if (store_in_global_scope) {
        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(func_obj)));

        for (const auto& decorator : func_decl_node->decos) {
            auto deco_code = gen_code(decorator, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), deco_code.begin(), deco_code.end());
            push_op(code, src, src_line_no, ::irgen::CALL(1));
        }

        if (func_decl_node->visibility == lmx::Visibility::Internal) {
            local_scope_stack.top().allocate_slot(
                func_decl_node->name,
                false,
                lmx::Visibility::Internal
            );
            const auto slot = local_scope_stack.top().get_slot(func_decl_node->name).value();
            push_op(code, src, src_line_no, ::irgen::STORE_FAST(slot));
        } else {
            push_op(code, src, src_line_no, ::irgen::NEW_VAR(func_decl_node->name));
            push_op(code, src, src_line_no, ::irgen::STORE());
        }
    }

    return func_obj;
}

std::shared_ptr<::irgen::FunctionObject> compile_macro_decl(
    const lmx::MacroDeclNode* macro_decl_node,
    std::vector<::irgen::Opcode>& code,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines,
    const std::string& src,
    int src_line_no
) {
    const size_t func_label = label_counter++;
    const size_t func_end_label = label_counter++;

    auto func_obj = std::make_shared<::irgen::FunctionObject>();
    func_obj->is_macro = true;
    func_obj->params.clear();
    func_obj->param_default_ir.clear();
    for (size_t i = 0; i < macro_decl_node->params.size(); ++i) {
        const auto& param = macro_decl_node->params[i];
        func_obj->params.push_back(param.name);
        if (param.is_variadic) {
            func_obj->variadic_param_index = i;
        }
        func_obj->param_default_ir.emplace_back();
    }
    func_obj->location = func_label;
    func_obj->name = macro_decl_node->name;

    local_scope_stack.emplace();
    local_scope_stack.top().depth = local_scope_stack.size() - 1;

    func_context_stack.emplace_back();
    func_context_stack.back().func_depth = local_scope_stack.top().depth;
    func_context_stack.back().is_macro = true;

    std::vector<size_t> param_slots;
    param_slots.reserve(macro_decl_node->params.size());
    for (const auto& param : macro_decl_node->params) {
        param_slots.push_back(
            local_scope_stack.top().allocate_slot(param.name, false, lmx::Visibility::Internal)
        );
    }

    std::vector<::irgen::Opcode> body_code;
    if (macro_decl_node->body) {
        body_code = gen_code(
            macro_decl_node->body,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
    }

    func_obj->needs_closure = func_context_stack.back().needs_closure;
    func_obj->needs_symbol_bind = func_context_stack.back().needs_symbol_bind;
    func_context_stack.pop_back();
    const bool bind_symbols = func_obj->needs_closure || func_obj->needs_symbol_bind;

    code.emplace_back(::irgen::GOTO(func_end_label));
    code.emplace_back(::irgen::LABEL(func_label));
    code.emplace_back(::irgen::ENTER_SCOPE());

    for (size_t i = 0; i < macro_decl_node->params.size(); ++i) {
        const auto& param = macro_decl_node->params[i];
        code.emplace_back(::irgen::STORE_FAST(param_slots[i]));
        if (bind_symbols) {
            emit_bind_fast(code, param_slots[i], param.name);
        }
    }

    code.insert(code.end(), body_code.begin(), body_code.end());
    code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
    local_scope_stack.pop();

    code.emplace_back(::irgen::LABEL(func_end_label));

    push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(func_obj)));
    push_op(code, src, src_line_no, ::irgen::NEW_VAR(macro_decl_node->name));
    push_op(code, src, src_line_no, ::irgen::STORE());

    return func_obj;
}

std::shared_ptr<::irgen::FunctionObject> compile_dispatch_handler(
    const std::vector<lmx::FuncParam>& params,
    lmx::BlockStmtNode* body,
    std::string name,
    std::vector<::irgen::Opcode>& code,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    const size_t func_label = label_counter++;
    const size_t func_end_label = label_counter++;

    auto func_obj = std::make_shared<::irgen::FunctionObject>();
    func_obj->params.clear();
    func_obj->param_default_ir.clear();
    for (const auto& param : params) {
        func_obj->params.push_back(param.name);
        if (param.default_value != nullptr) {
            auto default_code = gen_code(
                param.default_value,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            func_obj->param_default_ir.push_back(std::move(default_code));
        } else {
            func_obj->param_default_ir.emplace_back();
        }
    }
    func_obj->location = func_label;
    func_obj->name = std::move(name);
    apply_param_types(func_obj, params);

    local_scope_stack.emplace();
    local_scope_stack.top().depth = local_scope_stack.size() - 1;

    func_context_stack.emplace_back();
    func_context_stack.back().func_depth = local_scope_stack.top().depth;

    std::vector<size_t> param_slots;
    param_slots.reserve(params.size());
    for (const auto& param : params) {
        param_slots.push_back(
            local_scope_stack.top().allocate_slot(param.name, false, lmx::Visibility::Internal)
        );
    }

    std::vector<::irgen::Opcode> body_code;
    if (body) {
        body_code = gen_code(body, loop_stack, local_scope_stack, func_context_stack, source_lines);
    }

    func_obj->needs_closure = func_context_stack.back().needs_closure;
    func_obj->needs_symbol_bind = func_context_stack.back().needs_symbol_bind;
    func_context_stack.pop_back();
    const bool bind_symbols = func_obj->needs_closure || func_obj->needs_symbol_bind;

    code.emplace_back(::irgen::GOTO(func_end_label));
    code.emplace_back(::irgen::LABEL(func_label));
    code.emplace_back(::irgen::ENTER_SCOPE());

    for (size_t i = 0; i < params.size(); ++i) {
        code.emplace_back(::irgen::STORE_FAST(param_slots[i]));
        if (bind_symbols) {
            emit_bind_fast(code, param_slots[i], params[i].name);
        }
    }

    code.insert(code.end(), body_code.begin(), body_code.end());
    code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
    local_scope_stack.pop();
    code.emplace_back(::irgen::LABEL(func_end_label));

    return func_obj;
}

std::vector<::irgen::Opcode> gen_lvalue_code(
    lmx::ExprNode* node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
);

std::vector<::irgen::Opcode> gen_lvalue_code(
    lmx::ExprNode* node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    std::vector<::irgen::Opcode> code;
    const std::string src = line_text(node, source_lines);
    const int src_line_no = node != nullptr ? node->source_line : 0;

    if (node->kind == lmx::ASTNodeType::VarRef) {
        const auto* var_ref_node = dynamic_cast<lmx::VarRefNode*>(node);
        const std::string& var_name = var_ref_node->name;

        const ResolvedVar var = resolve_var(var_name, local_scope_stack);

        bool use_closure_load = false;
        if (var.found) {
            if (!func_context_stack.empty()) {
                const size_t func_depth = func_context_stack.back().func_depth;
                if (var.define_depth < func_depth) {
                    func_context_stack.back().needs_closure = true;
                    use_closure_load = true;
                    for (auto& ctx : func_context_stack) {
                        if (ctx.func_depth == var.define_depth) {
                            ctx.needs_symbol_bind = true;
                        }
                    }
                }
            }
            if (!use_closure_load && uses_fast_local(var)) {
                push_op(code, src, src_line_no, ::irgen::LOAD_FAST(var.slot));
            }
        }
        if (!var.found || use_closure_load || !uses_fast_local(var)) {
            push_op(code, src, src_line_no, ::irgen::LOAD(var_name));
        }
    } else if (node->kind == lmx::ASTNodeType::IndexAccess) {
        const auto* index_node = dynamic_cast<lmx::IndexAccessNode*>(node);
        auto obj_code = gen_code(index_node->object, loop_stack, local_scope_stack, func_context_stack, source_lines);
        auto index_code = gen_code(index_node->index, loop_stack, local_scope_stack, func_context_stack, source_lines);
        code.insert(code.end(), obj_code.begin(), obj_code.end());
        code.insert(code.end(), index_code.begin(), index_code.end());
        push_op(code, src, src_line_no, ::irgen::INDEX());
    } else if (node->kind == lmx::ASTNodeType::Unary) {
        const auto* unary_node = dynamic_cast<lmx::UnaryNode*>(node);
        if (unary_node->op == "*") {
            auto ptr_code = gen_code(unary_node->operand, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), ptr_code.begin(), ptr_code.end());
            push_op(code, src, src_line_no, ::irgen::PTR_TO_REF());
        } else {
            throw RuntimeError("Invalid lvalue for address-of or assignment");
        }
    } else {
        throw RuntimeError("Expression is not an assignable lvalue");
    }

    return code;
}

bool is_slot_lvalue_receiver(const lmx::ExprNode* object) {
    if (object == nullptr || object->getValueCategory() != lmx::ValueCategory::LVALUE) {
        return false;
    }
    if (object->kind == lmx::ASTNodeType::VarRef || object->kind == lmx::ASTNodeType::IndexAccess) {
        return true;
    }
    if (object->kind == lmx::ASTNodeType::Unary) {
        const auto* unary = dynamic_cast<const lmx::UnaryNode*>(object);
        return unary != nullptr && unary->op == "*";
    }
    return false;
}

std::vector<::irgen::Opcode> gen_member_receiver_code(
    lmx::ExprNode* object,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    if (is_slot_lvalue_receiver(object)) {
        return gen_lvalue_code(object, loop_stack, local_scope_stack, func_context_stack, source_lines);
    }
    return gen_code(object, loop_stack, local_scope_stack, func_context_stack, source_lines);
}

std::vector<::irgen::Opcode> gen_code(
    lmx::ASTNode* node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack,
    const std::vector<std::string>& source_lines
) {
    std::vector<::irgen::Opcode> code;
    const std::string src = line_text(node, source_lines);
    const int src_line_no = node != nullptr ? node->source_line : 0;

    if (node->kind == lmx::ASTNodeType::Number) {
        const auto* num_node = dynamic_cast<lmx::NumberNode*>(node);
        auto value = lang::lammp::Number(num_node->value);
        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(value)));
    } else if (node->kind == lmx::ASTNodeType::Bool) {
        const auto* bool_node = dynamic_cast<lmx::BoolNode*>(node);
        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(bool_node->value)));
    } else if (node->kind == lmx::ASTNodeType::String) {
        const auto* string_node = dynamic_cast<lmx::StringNode*>(node);
        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(string_node->value)));
    } else if (node->kind == lmx::ASTNodeType::Vector) {
        const auto* vec_node = dynamic_cast<lmx::VectorNode*>(node);
        for (auto elem : std::ranges::reverse_view(vec_node->elements)) {
            auto elem_code = gen_code(elem, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), elem_code.begin(), elem_code.end());
        }
        push_op(code, src, src_line_no, ::irgen::VEC_NEW(vec_node->elements.size()));
    } else if (node->kind == lmx::ASTNodeType::Comprehension) {
        const auto* comp_node = dynamic_cast<lmx::ComprehensionNode*>(node);
        const std::string result_var = std::format("__comp_{}", label_counter++);
        const size_t item_count = comp_node->items.size();

        push_op(code, src, src_line_no, ::irgen::VEC_NEW(0));
        code.emplace_back(::irgen::NEW_INTERN_VAR(result_var));
        code.emplace_back(::irgen::STORE());

        const size_t loop_start_label = label_counter++;
        const size_t loop_end_label = label_counter++;
        const size_t append_skip_label = label_counter++;

        loop_stack.emplace(loop_start_label, loop_end_label);

        std::vector<std::string> prior_var_names;
        std::vector<std::string> iter_slot_names(item_count);
        std::vector<bool> iter_is_dependent(item_count);

        for (size_t i = 0; i < item_count; ++i) {
            const auto& item = comp_node->items[i];
            const bool dependent = iterableDependsOnPriorVars(item->iterable, prior_var_names);
            iter_is_dependent[i] = dependent;
            prior_var_names.push_back(item->var_name);

            if (!dependent) {
                iter_slot_names[i] = std::format("__comp_iter_{}", label_counter++);
                auto iterable_code = gen_code(
                    item->iterable,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), iterable_code.begin(), iterable_code.end());
                code.emplace_back(::irgen::ITER_NEW());
                code.emplace_back(::irgen::NEW_INTERN_VAR(iter_slot_names[i]));
                code.emplace_back(::irgen::STORE());
            }
        }

        code.emplace_back(::irgen::LABEL(loop_start_label));

        for (size_t i = 0; i < item_count; ++i) {
            const auto& item = comp_node->items[i];

            if (iter_is_dependent[i]) {
                auto iterable_code = gen_code(
                    item->iterable,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), iterable_code.begin(), iterable_code.end());
                code.emplace_back(::irgen::ITER_NEW());
            } else {
                code.emplace_back(::irgen::LOAD(iter_slot_names[i]));
            }

            code.emplace_back(::irgen::ITER_NEXT());
            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::GOTOIF(loop_end_label));
            code.emplace_back(::irgen::NEW_VAR(item->var_name));
            code.emplace_back(::irgen::STORE());
            code.emplace_back(::irgen::ITER_END());
        }

        if (comp_node->guard) {
            auto guard_code = gen_code(
                comp_node->guard,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), guard_code.begin(), guard_code.end());
            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::GOTOIF(append_skip_label));
        }

        auto elem_code = gen_code(
            comp_node->expr,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), elem_code.begin(), elem_code.end());
        code.emplace_back(::irgen::LOAD(result_var));
        push_op(code, src, src_line_no, ::irgen::GETATTR("append"));
        push_op(code, src, src_line_no, ::irgen::CALL(1));

        code.emplace_back(::irgen::LABEL(append_skip_label));
        code.emplace_back(::irgen::GOTO(loop_start_label));
        code.emplace_back(::irgen::LABEL(loop_end_label));

        for (size_t i = 0; i < item_count; ++i) {
            if (!iter_is_dependent[i]) {
                code.emplace_back(::irgen::LOAD(iter_slot_names[i]));
                code.emplace_back(::irgen::ITER_END());
            }
        }

        loop_stack.pop();

        code.emplace_back(::irgen::LOAD(result_var));
        code.emplace_back(::irgen::DEREF());
    } else if (node->kind == lmx::ASTNodeType::Dictionary) {
        const auto* dict_node = dynamic_cast<lmx::DictionaryNode*>(node);
        for (auto entry : std::ranges::reverse_view(dict_node->entries)) {
            auto entry_code = gen_code(entry, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), entry_code.begin(), entry_code.end());
        }
        push_op(code, src, src_line_no, ::irgen::DICT_NEW(dict_node->entries.size()));
    } else if (node->kind == lmx::ASTNodeType::DictEntry) {
        const auto* entry_node = dynamic_cast<lmx::DictEntryNode*>(node);
        auto key_code = gen_code(entry_node->key, loop_stack, local_scope_stack, func_context_stack, source_lines);
        auto value_code = gen_code(entry_node->value, loop_stack, local_scope_stack, func_context_stack, source_lines);
        code.insert(code.end(), key_code.begin(), key_code.end());
        code.insert(code.end(), value_code.begin(), value_code.end());
    } else if (node->kind == lmx::ASTNodeType::Binary) {
        const auto* bin_node = dynamic_cast<lmx::BinaryNode*>(node);
        if (bin_node->op == "and") {
            emit_short_circuit_and(
                code,
                bin_node,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines,
                src,
                src_line_no
            );
        } else if (bin_node->op == "or") {
            emit_short_circuit_or(
                code,
                bin_node,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines,
                src,
                src_line_no
            );
        } else {
        auto left_code = gen_code(bin_node->left, loop_stack, local_scope_stack, func_context_stack, source_lines);
        auto right_code = gen_code(bin_node->right, loop_stack, local_scope_stack, func_context_stack, source_lines);
        code.insert(code.end(), left_code.begin(), left_code.end());
        code.insert(code.end(), right_code.begin(), right_code.end());

        if (bin_node->op == "+") {
            push_op(code, src, src_line_no, ::irgen::ADD());
        } else if (bin_node->op == "*") {
            push_op(code, src, src_line_no, ::irgen::MUL());
        } else if (bin_node->op == "-") {
            push_op(code, src, src_line_no, ::irgen::SUB());
        } else if (bin_node->op == "/") {
            push_op(code, src, src_line_no, ::irgen::DIV());
        } else if (bin_node->op == "&&") {
            push_op(code, src, src_line_no, ::irgen::AND());
        } else if (bin_node->op == "||") {
            push_op(code, src, src_line_no, ::irgen::OR());
        } else if (bin_node->op == "==") {
            push_op(code, src, src_line_no, ::irgen::EQ());
        } else if (bin_node->op == "!=") {
            push_op(code, src, src_line_no, ::irgen::NEQ());
        } else if (bin_node->op == "<") {
            push_op(code, src, src_line_no, ::irgen::LT());
        } else if (bin_node->op == "<=") {
            push_op(code, src, src_line_no, ::irgen::LTE());
        } else if (bin_node->op == ">") {
            push_op(code, src, src_line_no, ::irgen::GT());
        } else if (bin_node->op == ">=") {
            push_op(code, src, src_line_no, ::irgen::GTE());
        }
        }
    } else if (node->kind == lmx::ASTNodeType::Unary) {
        const auto* unary_node = dynamic_cast<lmx::UnaryNode*>(node);

        if (unary_node->op == "&") {
            auto lvalue_code = gen_lvalue_code(
                unary_node->operand,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), lvalue_code.begin(), lvalue_code.end());
            push_op(code, src, src_line_no, ::irgen::ADDR_OF());
        } else {
            auto operand_code = gen_code(
                unary_node->operand,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), operand_code.begin(), operand_code.end());

            if (unary_node->op == "-") {
                push_op(code, src, src_line_no, ::irgen::NEG());
            } else if (unary_node->op == "!") {
                push_op(code, src, src_line_no, ::irgen::NOT());
            } else if (unary_node->op == "not") {
                push_op(code, src, src_line_no, ::irgen::TRUTHY_NOT());
            } else if (unary_node->op == "*") {
                push_op(code, src, src_line_no, ::irgen::DEREF_PTR());
            }
        }
    } else if (node->kind == lmx::ASTNodeType::VarRef) {
        const auto* var_ref_node = dynamic_cast<lmx::VarRefNode*>(node);
        const std::string& var_name = var_ref_node->name;

        const ResolvedVar var = resolve_var(var_name, local_scope_stack);

        bool use_closure_load = false;
        if (var.found) {
            if (!func_context_stack.empty()) {
                const size_t func_depth = func_context_stack.back().func_depth;
                if (var.define_depth < func_depth) {
                    func_context_stack.back().needs_closure = true;
                    use_closure_load = true;
                    for (auto& ctx : func_context_stack) {
                        if (ctx.func_depth == var.define_depth) {
                            ctx.needs_symbol_bind = true;
                        }
                    }
                }
            }
            if (!use_closure_load && uses_fast_local(var)) {
                push_op(code, src, src_line_no, ::irgen::LOAD_FAST(var.slot));
            }
        }
        if (!var.found || use_closure_load || !uses_fast_local(var)) {
            push_op(code, src, src_line_no, ::irgen::LOAD(var_name));
            push_op(code, src, src_line_no, ::irgen::DEREF());
        }
    } else if (node->kind == lmx::ASTNodeType::FuncCallExpr) {
        const auto* func_call_node = dynamic_cast<lmx::FuncCallExprNode*>(node);

        size_t positional_count = 0;
        uint64_t splat_mask = 0;
        std::vector<const lmx::CallArgument*> keyword_args;
        for (const auto& arg : func_call_node->args) {
            if (arg.name.empty()) {
                auto arg_code = gen_code(arg.value, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), arg_code.begin(), arg_code.end());
                if (arg.is_splat) {
                    splat_mask |= (1ULL << positional_count);
                }
                ++positional_count;
            } else {
                keyword_args.push_back(&arg);
            }
        }

        const bool has_kwargs = !keyword_args.empty();
        if (has_kwargs) {
            for (const auto* kw : keyword_args) {
                push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(kw->name)));
                auto kw_code = gen_code(kw->value, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), kw_code.begin(), kw_code.end());
            }
            push_op(code, src, src_line_no, ::irgen::DICT_NEW(keyword_args.size()));
        }

        if (func_call_node->func_expr->kind == lmx::ASTNodeType::MemberAccess) {
            const auto* member_callee =
                    dynamic_cast<const lmx::MemberAccessNode*>(func_call_node->func_expr);
            auto receiver_code = gen_member_receiver_code(
                member_callee->object,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), receiver_code.begin(), receiver_code.end());
            push_op(code, src, src_line_no, ::irgen::GETATTR(member_callee->member));
            push_op(code, src, src_line_no, ::irgen::CALL(positional_count, has_kwargs, splat_mask));
            return code;
        }

        if (func_call_node->func_expr->kind == lmx::ASTNodeType::VarRef) {
            const auto& callee =
                    dynamic_cast<const lmx::VarRefNode*>(func_call_node->func_expr)->name;
            if (::irgen::is_type_name(callee)) {
                push_op(code, src, src_line_no, ::irgen::LOAD(callee));
                push_op(code, src, src_line_no, ::irgen::CALL(positional_count, has_kwargs, splat_mask));
                return code;
            }
        }

        auto bcode = gen_code(func_call_node->func_expr, loop_stack, local_scope_stack, func_context_stack, source_lines);
        code.insert(code.end(), bcode.begin(), bcode.end());

        push_op(code, src, src_line_no, ::irgen::CALL(positional_count, has_kwargs, splat_mask));
    } else if (node->kind == lmx::ASTNodeType::MacroCallExpr) {
        const auto* macro_call_node = dynamic_cast<lmx::MacroCallExprNode*>(node);

        size_t positional_count = 0;
        uint64_t splat_mask = 0;
        for (const auto& arg : macro_call_node->args) {
            if (!arg.name.empty()) {
                throw RuntimeError("macro arguments cannot be keyword arguments");
            }
            if (arg.is_splat) {
                auto arg_code = gen_code(arg.value, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), arg_code.begin(), arg_code.end());
                splat_mask |= (1ULL << positional_count);
            } else if (compiling_in_macro(func_context_stack)) {
                gen_materialized_ast_expr(
                    arg.value,
                    code,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
            } else {
                push_op(
                    code,
                    src,
                    src_line_no,
                    ::irgen::PUSH(::irgen::make_ast_value(::irgen::ast_from_parse(arg.value)))
                );
            }
            ++positional_count;
        }

        if (macro_call_node->macro_expr->kind == lmx::ASTNodeType::VarRef) {
            const auto& callee = dynamic_cast<const lmx::VarRefNode*>(macro_call_node->macro_expr)->name;
            push_op(code, src, src_line_no, ::irgen::LOAD(callee));
        } else {
            auto callee_code = gen_code(
                macro_call_node->macro_expr,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), callee_code.begin(), callee_code.end());
        }

        push_op(code, src, src_line_no, ::irgen::CALL(positional_count, false, splat_mask));
    } else if (node->kind == lmx::ASTNodeType::QuoteExpr) {
        const auto* quote_node = dynamic_cast<const lmx::QuoteExprNode*>(node);

        std::vector<std::shared_ptr<::irgen::Value>> hygienic_vec;
        for (const auto& name : quote_node->hygienic_names) {
            hygienic_vec.push_back(
                std::make_shared<::irgen::Value>(::irgen::Value(name))
            );
        }
        std::vector<std::shared_ptr<::irgen::Value>> binding_vec;
        for (const auto* binding : quote_node->bindings) {
            binding_vec.push_back(
                std::make_shared<::irgen::Value>(
                    ::irgen::make_ast_value(::irgen::ast_from_parse(binding))
                )
            );
        }
        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(std::move(hygienic_vec))));
        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(std::move(binding_vec))));
        push_op(
            code,
            src,
            src_line_no,
            ::irgen::PUSH(::irgen::make_ast_value(::irgen::ast_from_parse(quote_node->body)))
        );
        push_op(code, src, src_line_no, ::irgen::LOAD("quote"));
        push_op(code, src, src_line_no, ::irgen::CALL(3));
    } else if (node->kind == lmx::ASTNodeType::VMCall) {
        // todo!
    } else if (node->kind == lmx::ASTNodeType::BlockStmt) {
        const auto* block_node = dynamic_cast<lmx::BlockStmtNode*>(node);
        for (const auto& stmt : block_node->stmts) {
            auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }
    } else if (node->kind == lmx::ASTNodeType::VarDecl) {
        const auto* var_decl_node = dynamic_cast<lmx::VarDeclNode*>(node);

        if (var_decl_node->visibility == lmx::Visibility::Internal) {
            size_t slot = 0;
            if (!local_scope_stack.empty()) {
                slot = local_scope_stack.top().allocate_slot(
                    var_decl_node->name,
                    var_decl_node->is_const,
                    var_decl_node->visibility
                );
            }

            if (var_decl_node->init) {
                auto init_code = gen_code(var_decl_node->init, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), init_code.begin(), init_code.end());
            } else {
                push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value()));
            }

            const ResolvedVar binding{true, slot, local_scope_stack.empty() ? 0 : local_scope_stack.top().depth};
            if (uses_fast_local(binding)) {
                push_op(code, src, src_line_no, ::irgen::STORE_FAST(slot));
                emit_bind_fast(code, slot, var_decl_node->name);
            } else if (var_decl_node->is_const) {
                push_op(code, src, src_line_no, ::irgen::NEW_INTERN_CONST(var_decl_node->name));
                push_op(code, src, src_line_no, ::irgen::STORE());
            } else {
                push_op(code, src, src_line_no, ::irgen::NEW_INTERN_VAR(var_decl_node->name));
                push_op(code, src, src_line_no, ::irgen::STORE());
            }
        } else {
            if (var_decl_node->init) {
                auto init_code = gen_code(var_decl_node->init, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), init_code.begin(), init_code.end());
            } else {
                push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value()));
            }
            if (var_decl_node->is_const) {
                push_op(code, src, src_line_no, ::irgen::NEW_CONST(var_decl_node->name));
            } else {
                push_op(code, src, src_line_no, ::irgen::NEW_VAR(var_decl_node->name));
            }
            push_op(code, src, src_line_no, ::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::Assign) {
        const auto* assign_node = dynamic_cast<lmx::AssignNode*>(node);
        if (assign_node->var->getValueCategory() != lmx::ValueCategory::LVALUE) {
            throw RuntimeError("Left-hand side of assignment must be an lvalue");
        }

        auto value_code = gen_code(assign_node->value, loop_stack, local_scope_stack, func_context_stack, source_lines);
        code.insert(code.end(), value_code.begin(), value_code.end());

        if (assign_node->var->kind == lmx::ASTNodeType::MemberAccess) {
            const auto* member_node =
                    dynamic_cast<const lmx::MemberAccessNode*>(assign_node->var);
            auto obj_code = gen_code(
                member_node->object,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), obj_code.begin(), obj_code.end());
            push_op(code, src, src_line_no, ::irgen::SET_FIELD(member_node->member));
        } else if (assign_node->var->kind == lmx::ASTNodeType::VarRef) {
            const std::string& var_name = dynamic_cast<lmx::VarRefNode*>(assign_node->var)->name;
            const ResolvedVar var = resolve_var(var_name, local_scope_stack);

            if (uses_fast_local(var)) {
                push_op(code, src, src_line_no, ::irgen::STORE_FAST(var.slot));
            } else {
                push_op(code, src, src_line_no, ::irgen::NEW_VAR_OR_LOAD(var_name));
                push_op(code, src, src_line_no, ::irgen::STORE());
            }
        } else if (assign_node->var->kind == lmx::ASTNodeType::Unary) {
            const auto* unary_node = dynamic_cast<const lmx::UnaryNode*>(assign_node->var);
            if (unary_node->op != "*") {
                throw RuntimeError("Left-hand side of assignment must be an lvalue");
            }
            auto ptr_code = gen_code(
                unary_node->operand,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), ptr_code.begin(), ptr_code.end());
            push_op(code, src, src_line_no, ::irgen::PTR_TO_REF());
            push_op(code, src, src_line_no, ::irgen::STORE());
        } else {
            auto var_code = gen_code(assign_node->var, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), var_code.begin(), var_code.end());
            push_op(code, src, src_line_no, ::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::StructDecl) {
        const auto* struct_node = dynamic_cast<const lmx::StructDeclNode*>(node);
        ::irgen::StructTypeDef def;
        def.name = struct_node->name;
        def.typed = struct_node->typed;

        if (!struct_node->base_name.empty()) {
            if (!::irgen::is_type_name(struct_node->base_name)) {
                throw RuntimeError("unknown base struct: " + struct_node->base_name);
            }
            const std::shared_ptr<::irgen::StructTypeDef> base =
                ::irgen::get_type_def(struct_node->base_name);
            if (base->kind != ::irgen::TypeKind::User) {
                throw RuntimeError("cannot inherit from primitive type: " + struct_node->base_name);
            }
            def.base_name = base->name;
            def.fields = base->fields;
            def.methods = base->methods;
            def.typed = def.typed || base->typed;
        }

        for (const auto& field : struct_node->fields) {
            ::irgen::StructFieldDef fd;
            fd.name = field.name;
            fd.type_name = field.type_name;
            fd.has_type_annotation = field.has_type_annotation;
            fd.mutable_field = field.is_var;
            if (field.default_init) {
                fd.has_default = true;
                fd.default_value = literal_to_value(field.default_init);
            }
            def.fields.push_back(std::move(fd));
        }

        for (const auto* method : struct_node->methods) {
            auto func_obj = compile_function_decl(
                method,
                code,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines,
                src,
                src_line_no,
                false
            );
            def.methods[method->name] = std::move(func_obj);
        }

        const std::string struct_name = struct_node->name;
        auto type_def = ::irgen::register_type_def(std::move(def));

        push_op(
            code,
            src,
            src_line_no,
            ::irgen::PUSH(::irgen::make_type_value(type_def))
        );
        push_op(code, src, src_line_no, ::irgen::NEW_VAR(struct_name));
        push_op(code, src, src_line_no, ::irgen::STORE());
    } else if (node->kind == lmx::ASTNodeType::MacroDecl) {
        const auto* macro_decl_node = dynamic_cast<lmx::MacroDeclNode*>(node);
        compile_macro_decl(
            macro_decl_node,
            code,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines,
            src,
            src_line_no
        );
    } else if (node->kind == lmx::ASTNodeType::FuncDecl) {
        const auto* func_decl_node = dynamic_cast<lmx::FuncDeclNode*>(node);
        compile_function_decl(
            func_decl_node,
            code,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines,
            src,
            src_line_no,
            true
        );
    } else if (node->kind == lmx::ASTNodeType::FriendFuncDecl) {
        const auto* friend_node = dynamic_cast<lmx::FriendFuncDeclNode*>(node);
        auto friend_obj = ::irgen::make_friend_function(friend_node->name);

        if (friend_node->body) {
            const auto handler = compile_dispatch_handler(
                friend_node->params,
                friend_node->body,
                friend_node->name,
                code,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            friend_obj->dispatch_handlers().push_back(
                std::make_shared<::irgen::Value>(::irgen::Value(handler))
            );
        }

        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(friend_obj)));
        push_op(code, src, src_line_no, ::irgen::NEW_VAR(friend_node->name));
        push_op(code, src, src_line_no, ::irgen::STORE());
    } else if (node->kind == lmx::ASTNodeType::DoFuncDecl) {
        const auto* do_func_node = dynamic_cast<lmx::DoFuncDeclNode*>(node);

        size_t func_label = label_counter++;
        size_t func_end_label = label_counter++;

        auto func_obj = std::make_shared<::irgen::FunctionObject>();
        func_obj->params.clear();
        func_obj->param_default_ir.clear();
        for (const auto& param : do_func_node->params) {
            func_obj->params.push_back(param.name);
            if (param.default_value != nullptr) {
                auto default_code = gen_code(
                    param.default_value,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                func_obj->param_default_ir.push_back(std::move(default_code));
            } else {
                func_obj->param_default_ir.emplace_back();
            }
        }
        func_obj->location = func_label;
        func_obj->name = "";

        apply_param_types(func_obj, do_func_node->params);

        local_scope_stack.emplace();
        local_scope_stack.top().depth = local_scope_stack.size() - 1;

        func_context_stack.emplace_back();
        func_context_stack.back().func_depth = local_scope_stack.top().depth;

        std::vector<size_t> param_slots;
        param_slots.reserve(do_func_node->params.size());
        for (const auto& param : do_func_node->params) {
            param_slots.push_back(
                local_scope_stack.top().allocate_slot(param.name, false, lmx::Visibility::Internal)
            );
        }

        std::vector<::irgen::Opcode> body_code;
        if (do_func_node->body) {
            body_code = gen_code(do_func_node->body, loop_stack, local_scope_stack, func_context_stack, source_lines);
        }

        func_obj->needs_closure = func_context_stack.back().needs_closure;
        func_obj->needs_symbol_bind = func_context_stack.back().needs_symbol_bind;
        func_context_stack.pop_back();
        const bool bind_symbols = func_obj->needs_closure || func_obj->needs_symbol_bind;

        code.emplace_back(::irgen::GOTO(func_end_label));
        code.emplace_back(::irgen::LABEL(func_label));
        code.emplace_back(::irgen::ENTER_SCOPE());

        for (size_t i = 0; i < do_func_node->params.size(); ++i) {
            const auto& param = do_func_node->params[i];
            code.emplace_back(::irgen::STORE_FAST(param_slots[i]));
            if (bind_symbols) {
                emit_bind_fast(code, param_slots[i], param.name);
            }
        }

        code.insert(code.end(), body_code.begin(), body_code.end());

        code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
        local_scope_stack.pop();

        code.emplace_back(::irgen::LABEL(func_end_label));

        push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(func_obj)));

        for (const auto& decorator : do_func_node->decos) {
            auto deco_code = gen_code(decorator, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), deco_code.begin(), deco_code.end());
            push_op(code, src, src_line_no, ::irgen::CALL(1));
        }
    } else if (node->kind == lmx::ASTNodeType::ExternFunc) {
    } else if (node->kind == lmx::ASTNodeType::ReturnStmt) {
        const auto* return_node = dynamic_cast<lmx::ReturnStmtNode*>(node);
        if (return_node->expr) {
            auto expr_code = gen_code(return_node->expr, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), expr_code.begin(), expr_code.end());
        } else {
            push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value()));
        }
        code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
    } else if (node->kind == lmx::ASTNodeType::ThrowStmt) {
        const auto* throw_node = dynamic_cast<lmx::ThrowStmtNode*>(node);
        auto expr_code = gen_code(
            throw_node->expr,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), expr_code.begin(), expr_code.end());
        code.emplace_back(::irgen::THROW());
    } else if (node->kind == lmx::ASTNodeType::TryStmt) {
        const auto* try_node = dynamic_cast<lmx::TryStmtNode*>(node);

        const size_t catch_dispatch = label_counter++;
        const size_t else_label = try_node->else_body != nullptr ? label_counter++ : 0;
        const size_t try_end = label_counter++;

        code.emplace_back(::irgen::ENTER_TRY(catch_dispatch, else_label, try_end));

        if (try_node->try_body) {
            auto try_code = gen_code(
                try_node->try_body,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), try_code.begin(), try_code.end());
        }

        code.emplace_back(::irgen::END_TRY());

        code.emplace_back(::irgen::LABEL(catch_dispatch));

        size_t first_catch_all_label = 0;
        std::vector<size_t> typed_body_labels;
        typed_body_labels.reserve(try_node->catches.size());

        for (const auto* clause : try_node->catches) {
            if (clause->catch_all) {
                const size_t body_label = label_counter++;
                typed_body_labels.push_back(body_label);
                if (first_catch_all_label == 0) {
                    first_catch_all_label = body_label;
                }
                continue;
            }
            code.emplace_back(::irgen::EXC_MATCH(clause->type_name));
            const size_t body_label = label_counter++;
            typed_body_labels.push_back(body_label);
            code.emplace_back(::irgen::GOTOIF(body_label));
        }

        if (first_catch_all_label == 0) {
            code.emplace_back(::irgen::RETHROW());
        } else {
            code.emplace_back(::irgen::GOTO(first_catch_all_label));
        }

        for (size_t i = 0; i < try_node->catches.size(); ++i) {
            const auto* clause = try_node->catches[i];
            code.emplace_back(::irgen::LABEL(typed_body_labels[i]));

            if (!clause->var_name.empty()) {
                code.emplace_back(::irgen::PUSH_EXC());
                code.emplace_back(::irgen::NEW_VAR(clause->var_name));
                code.emplace_back(::irgen::STORE());
            }

            if (clause->body) {
                auto body_code = gen_code(
                    clause->body,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), body_code.begin(), body_code.end());
            }

            code.emplace_back(::irgen::GOTO(try_end));
        }

        if (else_label != 0) {
            code.emplace_back(::irgen::LABEL(else_label));
            auto else_code = gen_code(
                try_node->else_body,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), else_code.begin(), else_code.end());
        }

        code.emplace_back(::irgen::LABEL(try_end));
        code.emplace_back(::irgen::POP_TRY());
    } else if (node->kind == lmx::ASTNodeType::Loop) {
        const auto* loop_node = dynamic_cast<lmx::LoopNode*>(node);

        if (loop_node->condition) {
            const std::string count_var = std::format("__loop_count_{}", label_counter++);
            const std::string iter_var = std::format("__loop_i_{}", label_counter++);

            auto count_code = gen_code(
                loop_node->condition,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), count_code.begin(), count_code.end());
            code.emplace_back(::irgen::NEW_INTERN_VAR(count_var));
            code.emplace_back(::irgen::STORE());

            push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(static_cast<size_t>(0))));
            code.emplace_back(::irgen::NEW_INTERN_VAR(iter_var));
            code.emplace_back(::irgen::STORE());

            const size_t loop_start_label = label_counter++;
            const size_t loop_end_label = label_counter++;

            loop_stack.emplace(loop_start_label, loop_end_label);

            code.emplace_back(::irgen::LABEL(loop_start_label));

            code.emplace_back(::irgen::LOAD(iter_var));
            code.emplace_back(::irgen::LOAD(count_var));
            code.emplace_back(::irgen::GTE());
            code.emplace_back(::irgen::GOTOIF(loop_end_label));

            if (loop_node->body) {
                auto body_code = gen_code(
                    loop_node->body,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), body_code.begin(), body_code.end());
            }

            code.emplace_back(::irgen::LOAD(iter_var));
            push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value(static_cast<size_t>(1))));
            code.emplace_back(::irgen::ADD());
            code.emplace_back(::irgen::NEW_VAR_OR_LOAD(iter_var));
            code.emplace_back(::irgen::STORE());

            code.emplace_back(::irgen::GOTO(loop_start_label));
            code.emplace_back(::irgen::LABEL(loop_end_label));

            loop_stack.pop();
        } else {
            const size_t loop_start_label = label_counter++;
            const size_t loop_end_label = label_counter++;

            loop_stack.emplace(loop_start_label, loop_end_label);

            code.emplace_back(::irgen::LABEL(loop_start_label));

            if (loop_node->body) {
                auto body_code = gen_code(
                    loop_node->body,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), body_code.begin(), body_code.end());
            }

            code.emplace_back(::irgen::GOTO(loop_start_label));
            code.emplace_back(::irgen::LABEL(loop_end_label));

            loop_stack.pop();
        }
    } else if (node->kind == lmx::ASTNodeType::WhileStmt) {
        const auto* while_node = dynamic_cast<lmx::WhileStmtNode*>(node);

        size_t loop_start_label = label_counter++;
        size_t loop_end_label = label_counter++;

        loop_stack.emplace(loop_start_label, loop_end_label);

        code.emplace_back(::irgen::LABEL(loop_start_label));

        if (while_node->condition) {
            auto cond_code = gen_code(while_node->condition, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), cond_code.begin(), cond_code.end());

            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::GOTOIF(loop_end_label));
        }

        if (while_node->body) {
            auto body_code = gen_code(while_node->body, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), body_code.begin(), body_code.end());
        }

        code.emplace_back(::irgen::GOTO(loop_start_label));
        code.emplace_back(::irgen::LABEL(loop_end_label));

        loop_stack.pop();
    } else if (node->kind == lmx::ASTNodeType::ForLoop) {
        const auto* for_node = dynamic_cast<lmx::ForLoopNode*>(node);
        const size_t item_count = for_node->items.size();

        size_t loop_start_label = label_counter++;
        size_t loop_end_label = label_counter++;

        loop_stack.emplace(loop_start_label, loop_end_label);

        std::vector<std::string> prior_var_names;
        std::vector<std::string> iter_slot_names(item_count);
        std::vector<bool> iter_is_dependent(item_count);

        for (size_t i = 0; i < item_count; ++i) {
            const auto& item = for_node->items[i];
            const bool dependent = iterableDependsOnPriorVars(item->iterable, prior_var_names);
            iter_is_dependent[i] = dependent;
            prior_var_names.push_back(item->var_name);

            if (!dependent) {
                iter_slot_names[i] = std::format("__for_iter_{}", label_counter++);
                auto iterable_code = gen_code(item->iterable, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), iterable_code.begin(), iterable_code.end());
                code.emplace_back(::irgen::ITER_NEW());
                code.emplace_back(::irgen::NEW_INTERN_VAR(iter_slot_names[i]));
                code.emplace_back(::irgen::STORE());
            }
        }

        code.emplace_back(::irgen::LABEL(loop_start_label));

        for (size_t i = 0; i < item_count; ++i) {
            const auto& item = for_node->items[i];

            if (iter_is_dependent[i]) {
                auto iterable_code = gen_code(item->iterable, loop_stack, local_scope_stack, func_context_stack, source_lines);
                code.insert(code.end(), iterable_code.begin(), iterable_code.end());
                code.emplace_back(::irgen::ITER_NEW());
            } else {
                code.emplace_back(::irgen::LOAD(iter_slot_names[i]));
            }

            code.emplace_back(::irgen::ITER_NEXT());
            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::GOTOIF(loop_end_label));
            code.emplace_back(::irgen::NEW_VAR(item->var_name));
            code.emplace_back(::irgen::STORE());
            code.emplace_back(::irgen::ITER_END());
        }

        if (for_node->body) {
            auto body_code = gen_code(for_node->body, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), body_code.begin(), body_code.end());
        }

        code.emplace_back(::irgen::GOTO(loop_start_label));
        code.emplace_back(::irgen::LABEL(loop_end_label));

        for (size_t i = 0; i < item_count; ++i) {
            if (!iter_is_dependent[i]) {
                code.emplace_back(::irgen::LOAD(iter_slot_names[i]));
                code.emplace_back(::irgen::ITER_END());
            }
        }

        loop_stack.pop();
    } else if (node->kind == lmx::ASTNodeType::Break) {
        if (loop_stack.empty()) {
            throw RuntimeError("break statement outside loop");
        }
        code.emplace_back(::irgen::GOTO(loop_stack.top().end_label));
    } else if (node->kind == lmx::ASTNodeType::Continue) {
        if (loop_stack.empty()) {
            throw RuntimeError("continue statement outside loop");
        }
        code.emplace_back(::irgen::GOTO(loop_stack.top().start_label));
    } else if (node->kind == lmx::ASTNodeType::IfStmt) {
        const auto* if_node = dynamic_cast<lmx::IfStmtNode*>(node);

        const size_t end_label = label_counter++;

        auto emit_if_branch = [&](lmx::ExprNode* condition, lmx::BlockStmtNode* block) {
            const size_t fail_label = label_counter++;
            auto cond_code = gen_code(
                condition,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), cond_code.begin(), cond_code.end());
            code.emplace_back(::irgen::GOTOIFNOT(fail_label));
            if (block != nullptr) {
                auto body_code = gen_code(
                    block,
                    loop_stack,
                    local_scope_stack,
                    func_context_stack,
                    source_lines
                );
                code.insert(code.end(), body_code.begin(), body_code.end());
            }
            code.emplace_back(::irgen::GOTO(end_label));
            code.emplace_back(::irgen::LABEL(fail_label));
        };

        emit_if_branch(if_node->condition, if_node->then_block);

        for (const auto& elif_branch : if_node->elif_blocks) {
            emit_if_branch(elif_branch.condition, elif_branch.block);
        }

        if (if_node->else_block != nullptr) {
            auto else_code = gen_code(
                if_node->else_block,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), else_code.begin(), else_code.end());
        }

        code.emplace_back(::irgen::LABEL(end_label));
    } else if (node->kind == lmx::ASTNodeType::MatchStmt) {
        const auto* match_node = dynamic_cast<lmx::MatchStmtNode*>(node);
        const std::string temp_name = std::format("__match_{}", label_counter++);

        if (match_node->subject) {
            auto subject_code = gen_code(
                match_node->subject,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), subject_code.begin(), subject_code.end());
        } else {
            push_op(code, src, src_line_no, ::irgen::PUSH(::irgen::Value()));
        }
        push_op(code, src, src_line_no, ::irgen::NEW_INTERN_VAR(temp_name));
        push_op(code, src, src_line_no, ::irgen::STORE());

        const size_t end_label = label_counter++;

        for (size_t i = 0; i < match_node->cases.size(); ++i) {
            const size_t next_case_label = label_counter++;
            emit_match_case(
                code,
                match_node->cases[i],
                temp_name,
                next_case_label,
                end_label,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.emplace_back(::irgen::LABEL(next_case_label));
        }

        if (match_node->else_block) {
            auto else_code = gen_code(
                match_node->else_block,
                loop_stack,
                local_scope_stack,
                func_context_stack,
                source_lines
            );
            code.insert(code.end(), else_code.begin(), else_code.end());
        }

        code.emplace_back(::irgen::LABEL(end_label));
    } else if (node->kind == lmx::ASTNodeType::Module) {
    } else if (node->kind == lmx::ASTNodeType::Import) {
        const auto* import_node = dynamic_cast<lmx::ImportNode*>(node);
        if (import_node->module_name.empty()) {
            throw std::runtime_error("Empty module name in import statement");
        }
        std::string full_module_name;
        for (size_t i = 0; i < import_node->module_name.size(); ++i) {
            if (i > 0) full_module_name += ".";
            full_module_name += import_node->module_name[i];
        }
        std::string store_name = import_node->alias.empty()
                                     ? import_node->module_name.back()
                                     : import_node->alias;
        push_op(code, src, src_line_no, ::irgen::FINDMOD(full_module_name));
        push_op(code, src, src_line_no, ::irgen::NEW_VAR(store_name));
        push_op(code, src, src_line_no, ::irgen::STORE());
    } else if (node->kind == lmx::ASTNodeType::Use) {
        const auto* use_node = dynamic_cast<lmx::UseNode*>(node);
        std::string full_module_name;
        for (size_t i = 0; i < use_node->module_name.size(); ++i) {
            if (i > 0) full_module_name += ".";
            full_module_name += use_node->module_name[i];
        }
        const std::string mod_temp = std::format("__use_mod_{}", label_counter++);
        push_op(code, src, src_line_no, ::irgen::FINDMOD(full_module_name));
        push_op(code, src, src_line_no, ::irgen::NEW_INTERN_VAR(mod_temp));
        push_op(code, src, src_line_no, ::irgen::STORE());
        for (const auto& item : use_node->items) {
            push_op(code, src, src_line_no, ::irgen::LOAD(mod_temp));
            push_op(code, src, src_line_no, ::irgen::GETATTR(item.name));
            std::string alias = item.alias.empty() ? item.name : item.alias;
            push_op(code, src, src_line_no, ::irgen::NEW_VAR(alias));
            push_op(code, src, src_line_no, ::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::TypeConvert) {
        const auto* convert_node = dynamic_cast<const lmx::TypeConvertExprNode*>(node);
        auto type_code = gen_code(
            convert_node->type_expr,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), type_code.begin(), type_code.end());
        auto value_code = gen_code(
            convert_node->value_expr,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), value_code.begin(), value_code.end());
        push_op(code, src, src_line_no, ::irgen::LOAD("convert"));
        push_op(code, src, src_line_no, ::irgen::CALL(2));
    } else if (node->kind == lmx::ASTNodeType::MemberAccess) {
        const auto* member_node = dynamic_cast<lmx::MemberAccessNode*>(node);
        auto receiver_code = gen_member_receiver_code(
            member_node->object,
            loop_stack,
            local_scope_stack,
            func_context_stack,
            source_lines
        );
        code.insert(code.end(), receiver_code.begin(), receiver_code.end());
        push_op(code, src, src_line_no, ::irgen::GETATTR(member_node->member));
    } else if (node->kind == lmx::ASTNodeType::IndexAccess) {
        const auto* index_node = dynamic_cast<lmx::IndexAccessNode*>(node);
        auto obj_code = gen_code(index_node->object, loop_stack, local_scope_stack, func_context_stack, source_lines);
        auto index_code = gen_code(index_node->index, loop_stack, local_scope_stack, func_context_stack, source_lines);
        code.insert(code.end(), obj_code.begin(), obj_code.end());
        code.insert(code.end(), index_code.begin(), index_code.end());
        push_op(code, src, src_line_no, ::irgen::INDEX());
    } else if (node->kind == lmx::ASTNodeType::Program) {
        const auto* program_node = dynamic_cast<lmx::ProgramASTNode*>(node);
        for (const auto& stmt : program_node->stmts) {
            auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack, source_lines);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }
    } else if (node->kind == lmx::ASTNodeType::Expr) {
        const auto* expr_node = dynamic_cast<lmx::ExprNode*>(node);
        for (const auto i : expr_node->children) gen_code(i, loop_stack, local_scope_stack, func_context_stack, source_lines);
    }
    return code;
}

std::vector<::irgen::Opcode> Generator::gen() const {
    std::vector<::irgen::Opcode> code;
#ifdef DEBUG
    printAST(ast);
#endif

    std::stack<LoopLabels> loop_stack;
    Stack<LocalScope> local_scope_stack;
    std::vector<FunctionContext> func_context_stack;
    local_scope_stack.emplace();

    for (const auto& stmt : ast->stmts) {
        auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack, ast->source_lines);
        code.insert(code.end(), stmt_code.begin(), stmt_code.end());
    }
#ifdef DEBUG
    std::string codes;
    size_t i = 0;
    for (auto const& elem : code) {
        std::visit(
            [&](auto& op) {
                codes.append(std::format("{:3} | {}\n", i, op.toString()));
            },
            elem
        );
        i++;
    }
    std::cerr << codes << std::endl;
#endif
    replace_string(code);
    maybe_optimize_bytecode(code);
    return code;
}

void print_code(const std::vector<::irgen::Opcode>& code) {
    int i = 0;
    for (const auto& c : code) {
        std::visit(
            [&](const auto& op) {
                std::cerr << std::format(
                    "{} | {} {}",
                    i,
                    op.name(),
                    op.stringArgs()
                ) << std::endl;
            },
            c
        );
        i++;
    }
}

bool execute(
    const lmx::ProgramASTNode* program,
    const std::function<bool(::irgen::VM& vm)>& on_result
) {
    if (!program) {
        std::cerr << "Error: Null program AST\n";
        return false;
    }

    if (execute_debug_output) {
        printAST(program);
    }

    std::vector<::irgen::Opcode> code;

    std::stack<LoopLabels> loop_stack;
    Stack<LocalScope> local_scope_stack;
    std::vector<FunctionContext> func_context_stack;
    local_scope_stack.emplace();

    for (const auto& stmt : program->stmts) {
        if (!stmt) continue;
        auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack, program->source_lines);
        code.insert(code.end(), stmt_code.begin(), stmt_code.end());
    }

    if (execute_debug_output) {
        print_code(code);
    }

    Generator::replace_string(code);
    maybe_optimize_bytecode(code);

    auto vm = std::make_unique<::irgen::VM>(std::move(code));
    vm->source_filename = program->source_filename.empty() ? "<input>" : program->source_filename;
    vm->set_symbol("__package__", ::irgen::Value("__main__"));
    vm->run();

    LOG("execute: calling on_result");
    if (!on_result) {
        vm->shutdown();
        vm.reset();
        return true;
    }
    const bool ok = on_result(*vm);
    LOG("execute: on_result returned " << std::boolalpha << ok);
    vm->shutdown();
    LOG("execute: shutdown complete");
    vm.reset();
    LOG("execute: vm destroyed");
    LOG("execute: returning " << std::boolalpha << ok);
    return ok;
}
}
