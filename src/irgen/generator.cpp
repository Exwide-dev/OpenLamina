#include "generator.hpp"

#include <expected>

#include "../tools/debug.hpp"
#include "../tools/lang/builtins.hpp"

#include <format>
#include <ranges>
#include <stack>
#include <unordered_set>

#include "opcode.hpp"
#include "struct_types.hpp"

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
} // namespace

namespace lm::irgen {
thread_local size_t label_counter = 0;

struct LoopLabels {
    size_t start_label;
    size_t end_label;

    LoopLabels(size_t start, size_t end) : start_label(start), end_label(end) {
    }
};

struct FunctionContext {
    bool needs_closure = false;
    size_t func_depth = 0;
};

std::vector<::irgen::Opcode> gen_code(
    lmx::ASTNode* node,
    std::stack<LoopLabels>& loop_stack,
    Stack<LocalScope>& local_scope_stack,
    std::vector<FunctionContext>& func_context_stack
) {
    std::vector<::irgen::Opcode> code;

    if (node->kind == lmx::ASTNodeType::Number) {
        const auto* num_node = dynamic_cast<lmx::NumberNode*>(node);
        auto value = lang::lammp::Number(num_node->value);
        code.emplace_back(::irgen::PUSH(::irgen::Value(value)));
    } else if (node->kind == lmx::ASTNodeType::Bool) {
        const auto* bool_node = dynamic_cast<lmx::BoolNode*>(node);
        code.emplace_back(::irgen::PUSH(::irgen::Value(bool_node->value)));
    } else if (node->kind == lmx::ASTNodeType::String) {
        const auto* string_node = dynamic_cast<lmx::StringNode*>(node);
        code.emplace_back(::irgen::PUSH(::irgen::Value(string_node->value)));
    } else if (node->kind == lmx::ASTNodeType::Vector) {
        const auto* vec_node = dynamic_cast<lmx::VectorNode*>(node);
        for (auto elem : std::ranges::reverse_view(vec_node->elements)) {
            auto elem_code = gen_code(elem, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), elem_code.begin(), elem_code.end());
        }
        code.emplace_back(::irgen::VEC_NEW(vec_node->elements.size()));
    } else if (node->kind == lmx::ASTNodeType::Dictionary) {
        const auto* dict_node = dynamic_cast<lmx::DictionaryNode*>(node);
        for (auto entry : std::ranges::reverse_view(dict_node->entries)) {
            auto entry_code = gen_code(entry, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), entry_code.begin(), entry_code.end());
        }
        code.emplace_back(::irgen::DICT_NEW(dict_node->entries.size()));
    } else if (node->kind == lmx::ASTNodeType::DictEntry) {
        const auto* entry_node = dynamic_cast<lmx::DictEntryNode*>(node);
        auto key_code = gen_code(entry_node->key, loop_stack, local_scope_stack, func_context_stack);
        auto value_code = gen_code(entry_node->value, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), key_code.begin(), key_code.end());
        code.insert(code.end(), value_code.begin(), value_code.end());
    } else if (node->kind == lmx::ASTNodeType::Binary) {
        const auto* bin_node = dynamic_cast<lmx::BinaryNode*>(node);
        auto left_code = gen_code(bin_node->left, loop_stack, local_scope_stack, func_context_stack);
        auto right_code = gen_code(bin_node->right, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), left_code.begin(), left_code.end());
        code.insert(code.end(), right_code.begin(), right_code.end());

        if (bin_node->op == "+") {
            code.emplace_back(::irgen::ADD());
        } else if (bin_node->op == "*") {
            code.emplace_back(::irgen::MUL());
        } else if (bin_node->op == "-") {
            code.emplace_back(::irgen::SUB());
        } else if (bin_node->op == "/") {
            code.emplace_back(::irgen::DIV());
        } else if (bin_node->op == "&&") {
            code.emplace_back(::irgen::AND());
        } else if (bin_node->op == "||") {
            code.emplace_back(::irgen::OR());
        } else if (bin_node->op == "==") {
            code.emplace_back(::irgen::EQ());
        } else if (bin_node->op == "!=") {
            code.emplace_back(::irgen::NEQ());
        } else if (bin_node->op == "<") {
            code.emplace_back(::irgen::LT());
        } else if (bin_node->op == "<=") {
            code.emplace_back(::irgen::LTE());
        } else if (bin_node->op == ">") {
            code.emplace_back(::irgen::GT());
        } else if (bin_node->op == ">=") {
            code.emplace_back(::irgen::GTE());
        }
    } else if (node->kind == lmx::ASTNodeType::Unary) {
        const auto* unary_node = dynamic_cast<lmx::UnaryNode*>(node);
        auto operand_code = gen_code(unary_node->operand, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), operand_code.begin(), operand_code.end());

        if (unary_node->op == "-") {
            code.emplace_back(::irgen::NEG());
        } else if (unary_node->op == "!") {
            code.emplace_back(::irgen::NOT());
        }
    } else if (node->kind == lmx::ASTNodeType::VarRef) {
        const auto* var_ref_node = dynamic_cast<lmx::VarRefNode*>(node);
        const std::string& var_name = var_ref_node->name;

        bool found_in_local = false;
        size_t slot = 0;
        size_t define_depth = 0;
        const auto& container = local_scope_stack.get_container();
        for (int i = static_cast<int>(container.size()) - 1; i >= 0; --i) {
            const auto& scope = container[static_cast<size_t>(i)];
            auto loc_opt = scope.get_location(var_name);
            if (loc_opt.has_value()) {
                slot = loc_opt->slot;
                define_depth = loc_opt->define_depth;
                found_in_local = true;
                break;
            }
        }

        bool use_closure_load = false;
        if (found_in_local) {
            if (!func_context_stack.empty()) {
                const size_t func_depth = func_context_stack.back().func_depth;
                if (define_depth < func_depth) {
                    func_context_stack.back().needs_closure = true;
                    use_closure_load = true;
                }
            }
            if (!use_closure_load) {
                code.emplace_back(::irgen::LOAD_FAST(slot));
            }
        }
        if (!found_in_local || use_closure_load) {
            code.emplace_back(::irgen::LOAD(var_name));
        }
        // 读取变量值：LOAD 压入引用槽，DEREF 解引用为实际值（赋值走 AssignNode，不经此处）
        code.emplace_back(::irgen::DEREF());
    } else if (node->kind == lmx::ASTNodeType::FuncCallExpr) {
        const auto* func_call_node = dynamic_cast<lmx::FuncCallExprNode*>(node);
        for (auto arg : std::ranges::reverse_view(func_call_node->args)) {
            auto bcode = gen_code(arg, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), bcode.begin(), bcode.end());
        }

        if (func_call_node->func_expr->kind == lmx::ASTNodeType::VarRef) {
            const auto& callee =
                    dynamic_cast<const lmx::VarRefNode*>(func_call_node->func_expr)->name;
            if (::irgen::is_struct_type(callee)) {
                code.emplace_back(
                    ::irgen::STRUCT_NEW(
                        callee,
                        func_call_node->args.size()
                    )
                );
                return code;
            }
        }

        auto bcode = gen_code(func_call_node->func_expr, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), bcode.begin(), bcode.end());

        code.emplace_back(
            ::irgen::CALL(
                func_call_node->args.size()
            )
        );
    } else if (node->kind == lmx::ASTNodeType::VMCall) {
    } else if (node->kind == lmx::ASTNodeType::BlockStmt) {
        const auto* block_node = dynamic_cast<lmx::BlockStmtNode*>(node);
        for (const auto& stmt : block_node->stmts) {
            auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack);
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
                auto init_code = gen_code(var_decl_node->init, loop_stack, local_scope_stack, func_context_stack);
                code.insert(code.end(), init_code.begin(), init_code.end());
            } else {
                code.emplace_back(::irgen::PUSH(::irgen::Value()));
            }
            code.emplace_back(::irgen::STORE_FAST(slot));
            emit_bind_fast(code, slot, var_decl_node->name);
        } else {
            if (var_decl_node->init) {
                auto init_code = gen_code(var_decl_node->init, loop_stack, local_scope_stack, func_context_stack);
                code.insert(code.end(), init_code.begin(), init_code.end());
            } else {
                code.emplace_back(::irgen::PUSH(::irgen::Value()));
            }
            if (var_decl_node->is_const) {
                code.emplace_back(::irgen::NEW_CONST(var_decl_node->name));
            } else {
                code.emplace_back(::irgen::NEW_VAR(var_decl_node->name));
            }
            code.emplace_back(::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::Assign) {
        const auto* assign_node = dynamic_cast<lmx::AssignNode*>(node);
        if (assign_node->var->getValueCategory() != lmx::ValueCategory::LVALUE) {
            throw RuntimeError("Left-hand side of assignment must be an lvalue");
        }

        auto value_code = gen_code(assign_node->value, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), value_code.begin(), value_code.end());

        if (assign_node->var->kind == lmx::ASTNodeType::MemberAccess) {
            const auto* member_node =
                    dynamic_cast<const lmx::MemberAccessNode*>(assign_node->var);
            auto obj_code = gen_code(
                member_node->object,
                loop_stack,
                local_scope_stack,
                func_context_stack
            );
            code.insert(code.end(), obj_code.begin(), obj_code.end());
            code.emplace_back(::irgen::SET_FIELD(member_node->member));
        } else if (assign_node->var->kind == lmx::ASTNodeType::VarRef) {
            const std::string& var_name = dynamic_cast<lmx::VarRefNode*>(assign_node->var)->name;

            bool found_in_local = false;
            size_t slot = 0;
            const auto& container = local_scope_stack.get_container();
            for (int i = static_cast<int>(container.size()) - 1; i >= 0; --i) {
                const auto& scope = container[static_cast<size_t>(i)];
                auto slot_opt = scope.get_slot(var_name);
                if (slot_opt.has_value()) {
                    slot = *slot_opt;
                    found_in_local = true;
                    break;
                }
            }

            if (found_in_local) {
                code.emplace_back(::irgen::STORE_FAST(slot));
            } else {
                code.emplace_back(::irgen::NEW_VAR_OR_LOAD(var_name));
                code.emplace_back(::irgen::STORE());
            }
        } else {
            auto var_code = gen_code(assign_node->var, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), var_code.begin(), var_code.end());
            code.emplace_back(::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::StructDecl) {
        const auto* struct_node = dynamic_cast<const lmx::StructDeclNode*>(node);
        ::irgen::StructTypeDef def;
        def.name = struct_node->name;
        def.typed = struct_node->typed;

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

        ::irgen::register_struct_type(std::move(def));

        const std::string struct_name = struct_node->name;
        code.emplace_back(
            ::irgen::PUSH(
                ::irgen::Value(
                    ::irgen::FunctionType(
                        [struct_name](
                    ::irgen::VM&,
                    const std::vector<::irgen::Value>& args
                ) {
                            return ::irgen::make_struct_instance(struct_name, args);
                        }
                    )
                )
            )
        );
        code.emplace_back(::irgen::NEW_VAR(struct_name));
        code.emplace_back(::irgen::STORE());
    } else if (node->kind == lmx::ASTNodeType::FuncDecl) {
        const auto* func_decl_node = dynamic_cast<lmx::FuncDeclNode*>(node);

        size_t func_label = label_counter++;
        size_t func_end_label = label_counter++;

        auto func_obj = std::make_shared<::irgen::FunctionObject>();
        func_obj->params = func_decl_node->params;
        func_obj->location = func_label;
        func_obj->name = func_decl_node->name;

        code.emplace_back(::irgen::GOTO(func_end_label));
        code.emplace_back(::irgen::LABEL(func_label));
        code.emplace_back(::irgen::ENTER_SCOPE());

        local_scope_stack.emplace();
        LocalScope& func_scope = local_scope_stack.top();
        func_scope.depth = local_scope_stack.size() - 1;

        func_context_stack.emplace_back();
        func_context_stack.back().func_depth = func_scope.depth;

        for (const auto& param : func_decl_node->params) {
            func_scope.allocate_slot(param, false, lmx::Visibility::Internal);
        }

        for (size_t i = 0; i < func_decl_node->params.size(); ++i) {
            const auto& param = func_decl_node->params[i];
            const auto slot = func_scope.get_slot(param).value();
            code.emplace_back(::irgen::STORE_FAST(slot));
            emit_bind_fast(code, slot, param);
        }

        if (func_decl_node->body) {
            auto body_code = gen_code(func_decl_node->body, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), body_code.begin(), body_code.end());
        }

        func_obj->needs_closure = func_context_stack.back().needs_closure;
        func_context_stack.pop_back();

        code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
        local_scope_stack.pop();

        code.emplace_back(::irgen::LABEL(func_end_label));

        code.emplace_back(::irgen::PUSH(::irgen::Value(func_obj)));

        for (const auto& decorator : func_decl_node->decos) {
            auto deco_code = gen_code(decorator, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), deco_code.begin(), deco_code.end());
            code.emplace_back(::irgen::CALL(1));
        }

        if (func_decl_node->visibility == lmx::Visibility::Internal) {
            local_scope_stack.top().allocate_slot(
                func_decl_node->name,
                false,
                lmx::Visibility::Internal
            );
            auto slot = local_scope_stack.top().get_slot(func_decl_node->name).value();
            code.emplace_back(::irgen::STORE_FAST(slot));
        } else {
            code.emplace_back(::irgen::NEW_VAR(func_decl_node->name));
            code.emplace_back(::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::DoFuncDecl) {
        const auto* do_func_node = dynamic_cast<lmx::DoFuncDeclNode*>(node);

        size_t func_label = label_counter++;
        size_t func_end_label = label_counter++;

        auto func_obj = std::make_shared<::irgen::FunctionObject>();
        func_obj->params = do_func_node->params;
        func_obj->location = func_label;
        func_obj->name = "";

        code.emplace_back(::irgen::GOTO(func_end_label));
        code.emplace_back(::irgen::LABEL(func_label));
        code.emplace_back(::irgen::ENTER_SCOPE());

        local_scope_stack.emplace();
        LocalScope& func_scope = local_scope_stack.top();
        func_scope.depth = local_scope_stack.size() - 1;

        func_context_stack.emplace_back();
        func_context_stack.back().func_depth = func_scope.depth;

        for (const auto& param : do_func_node->params) {
            func_scope.allocate_slot(param, false, lmx::Visibility::Internal);
        }

        for (size_t i = 0; i < do_func_node->params.size(); ++i) {
            const auto& param = do_func_node->params[i];
            const auto slot = func_scope.get_slot(param).value();
            code.emplace_back(::irgen::STORE_FAST(slot));
            emit_bind_fast(code, slot, param);
        }

        if (do_func_node->body) {
            auto body_code = gen_code(do_func_node->body, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), body_code.begin(), body_code.end());
        }

        func_obj->needs_closure = func_context_stack.back().needs_closure;
        func_context_stack.pop_back();

        code.emplace_back(::irgen::RET());
        local_scope_stack.pop();

        code.emplace_back(::irgen::LABEL(func_end_label));

        code.emplace_back(::irgen::PUSH(::irgen::Value(func_obj)));

        for (const auto& decorator : do_func_node->decos) {
            auto deco_code = gen_code(decorator, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), deco_code.begin(), deco_code.end());
            code.emplace_back(::irgen::CALL(1));
        }
    } else if (node->kind == lmx::ASTNodeType::ExternFunc) {
    } else if (node->kind == lmx::ASTNodeType::ReturnStmt) {
        const auto* return_node = dynamic_cast<lmx::ReturnStmtNode*>(node);
        if (return_node->expr) {
            auto expr_code = gen_code(return_node->expr, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), expr_code.begin(), expr_code.end());
        } else {
            code.emplace_back(::irgen::PUSH(::irgen::Value()));
        }
        code.emplace_back(::irgen::RET_THEN_LEAVE_SCOPE());
    } else if (node->kind == lmx::ASTNodeType::Loop) {
        const auto* loop_node = dynamic_cast<lmx::LoopNode*>(node);

        size_t loop_start_label = label_counter++;
        size_t loop_end_label = label_counter++;

        loop_stack.emplace(loop_start_label, loop_end_label);

        code.emplace_back(::irgen::LABEL(loop_start_label));

        if (loop_node->body) {
            auto body_code = gen_code(loop_node->body, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), body_code.begin(), body_code.end());
        }

        code.emplace_back(::irgen::GOTO(loop_start_label));
        code.emplace_back(::irgen::LABEL(loop_end_label));

        loop_stack.pop();
    } else if (node->kind == lmx::ASTNodeType::WhileStmt) {
        const auto* while_node = dynamic_cast<lmx::WhileStmtNode*>(node);

        size_t loop_start_label = label_counter++;
        size_t loop_end_label = label_counter++;

        loop_stack.emplace(loop_start_label, loop_end_label);

        code.emplace_back(::irgen::LABEL(loop_start_label));

        if (while_node->condition) {
            auto cond_code = gen_code(while_node->condition, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), cond_code.begin(), cond_code.end());

            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::GOTOIF(loop_end_label));
        }

        if (while_node->body) {
            auto body_code = gen_code(while_node->body, loop_stack, local_scope_stack, func_context_stack);
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
                auto iterable_code = gen_code(item->iterable, loop_stack, local_scope_stack, func_context_stack);
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
                auto iterable_code = gen_code(item->iterable, loop_stack, local_scope_stack, func_context_stack);
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
            auto body_code = gen_code(for_node->body, loop_stack, local_scope_stack, func_context_stack);
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

        size_t else_label = label_counter++;
        size_t end_label = label_counter++;

        auto cond_code = gen_code(if_node->condition, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), cond_code.begin(), cond_code.end());

        code.emplace_back(::irgen::NOT());
        code.emplace_back(::irgen::GOTOIF(else_label));

        if (if_node->then_block) {
            auto then_code = gen_code(if_node->then_block, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), then_code.begin(), then_code.end());
        }

        if (if_node->else_block) {
            code.emplace_back(::irgen::GOTO(end_label));
        }

        code.emplace_back(::irgen::LABEL(else_label));
        if (if_node->else_block) {
            auto else_code = gen_code(if_node->else_block, loop_stack, local_scope_stack, func_context_stack);
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
        code.emplace_back(::irgen::FINDMOD(::irgen::Value(full_module_name)));
        code.emplace_back(::irgen::NEW_VAR(store_name));
        code.emplace_back(::irgen::STORE());
    } else if (node->kind == lmx::ASTNodeType::Use) {
        const auto* use_node = dynamic_cast<lmx::UseNode*>(node);
        std::string full_module_name;
        for (size_t i = 0; i < use_node->module_name.size(); ++i) {
            if (i > 0) full_module_name += ".";
            full_module_name += use_node->module_name[i];
        }
        code.emplace_back(::irgen::FINDMOD(::irgen::Value(full_module_name)));
        for (const auto& item : use_node->items) {
            code.emplace_back(::irgen::GETATTR(::irgen::Value(item.name)));
            std::string alias = item.alias.empty() ? item.name : item.alias;
            code.emplace_back(::irgen::NEW_VAR(alias));
            code.emplace_back(::irgen::STORE());
        }
    } else if (node->kind == lmx::ASTNodeType::MemberAccess) {
        const auto* member_node = dynamic_cast<lmx::MemberAccessNode*>(node);
        auto obj_code = gen_code(member_node->object, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), obj_code.begin(), obj_code.end());
        code.emplace_back(::irgen::GETATTR(::irgen::Value(member_node->member)));
    } else if (node->kind == lmx::ASTNodeType::IndexAccess) {
        const auto* index_node = dynamic_cast<lmx::IndexAccessNode*>(node);
        auto obj_code = gen_code(index_node->object, loop_stack, local_scope_stack, func_context_stack);
        auto index_code = gen_code(index_node->index, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), obj_code.begin(), obj_code.end());
        code.insert(code.end(), index_code.begin(), index_code.end());
        code.emplace_back(::irgen::INDEX());
    } else if (node->kind == lmx::ASTNodeType::Program) {
        const auto* program_node = dynamic_cast<lmx::ProgramASTNode*>(node);
        for (const auto& stmt : program_node->stmts) {
            auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }
    } else if (node->kind == lmx::ASTNodeType::Expr) {
        const auto* expr_node = dynamic_cast<lmx::ExprNode*>(node);
        for (const auto i : expr_node->children) gen_code(i, loop_stack, local_scope_stack, func_context_stack);
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
        auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack);
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
    return code;
}

::irgen::Value execute(const lmx::ProgramASTNode* program) {
    if (!program) {
        std::cerr << "Error: Null program AST\n";
        return {};
    }

    printAST(program);

    std::vector<::irgen::Opcode> code;

    std::stack<LoopLabels> loop_stack;
    Stack<LocalScope> local_scope_stack;
    std::vector<FunctionContext> func_context_stack;
    local_scope_stack.emplace();

    for (const auto& stmt : program->stmts) {
        if (!stmt) continue;
        auto stmt_code = gen_code(stmt, loop_stack, local_scope_stack, func_context_stack);
        code.insert(code.end(), stmt_code.begin(), stmt_code.end());
    }

    int i = 0;
    for (const auto& c : code) {
        std::visit(
            [&](const auto& op) {
                std::cerr << std::format(
                    "{} | {} {}",
                    i,
                    op.name(),
                    [&]()-> std::string {
                        std::string str;
                        for (const auto& operand : op.operands) {
                            str.append(operand.toString() + " ");
                        }
                        return str;
                    }()
                ) << std::endl;
            },
            c
        );
        i++;
    }

    Generator::replace_string(code);

    ::irgen::VM vm(code);
    vm.run();


    if (!vm.op_stack.empty()) {
        return vm.op_stack.top();
    }
    return {};
}
}
