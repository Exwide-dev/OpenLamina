#include "generator.hpp"
#include "../tools/debug.hpp"
#include "../tools/lang/builtins.hpp"

#include <format>
#include <ranges>
#include <stack>

#include "opcode.hpp"

namespace lm::irgen {
    static size_t label_counter = 0;

    struct LoopLabels {
        size_t start_label;
        size_t end_label;
        LoopLabels(size_t start, size_t end) : start_label(start), end_label(end) {}
    };

    std::vector<::irgen::Opcode> gen_code(
        lmx::ASTNode* node,
        std::stack<LoopLabels>& loop_stack
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
            auto elem_code = gen_code(elem, loop_stack);
            code.insert(code.end(), elem_code.begin(), elem_code.end());
        }
        code.emplace_back(::irgen::VEC_NEW(vec_node->elements.size()));
    } else if (node->kind == lmx::ASTNodeType::Dictionary) {
        const auto* dict_node = dynamic_cast<lmx::DictionaryNode*>(node);
        for (auto entry : std::ranges::reverse_view(dict_node->entries)) {
            auto entry_code = gen_code(entry, loop_stack);
            code.insert(code.end(), entry_code.begin(), entry_code.end());
        }
        code.emplace_back(::irgen::DICT_NEW(dict_node->entries.size()));
    } else if (node->kind == lmx::ASTNodeType::DictEntry) {
        const auto* entry_node = dynamic_cast<lmx::DictEntryNode*>(node);
        auto key_code = gen_code(entry_node->key, loop_stack);
        auto value_code = gen_code(entry_node->value, loop_stack);
        code.insert(code.end(), key_code.begin(), key_code.end());
        code.insert(code.end(), value_code.begin(), value_code.end());
    } else if (node->kind == lmx::ASTNodeType::Binary) {
            const auto* bin_node = dynamic_cast<lmx::BinaryNode*>(node);
            auto left_code = gen_code(bin_node->left, loop_stack);
            auto right_code = gen_code(bin_node->right, loop_stack);
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
            auto operand_code = gen_code(unary_node->operand, loop_stack);
            code.insert(code.end(), operand_code.begin(), operand_code.end());

            if (unary_node->op == "-") {
                code.emplace_back(::irgen::NEG());
            } else if (unary_node->op == "!") {
                code.emplace_back(::irgen::NOT());
            }
        } else if (node->kind == lmx::ASTNodeType::VarRef) {
            const auto* var_ref_node = dynamic_cast<lmx::VarRefNode*>(node);
            code.emplace_back(::irgen::LOAD(var_ref_node->name));
        } else if (node->kind == lmx::ASTNodeType::FuncCallExpr) {
            const auto* func_call_node = dynamic_cast<lmx::FuncCallExprNode*>(node);
            for (auto arg : std::ranges::reverse_view(func_call_node->args)) {
                auto bcode = gen_code(arg, loop_stack);
                code.insert(code.end(), bcode.begin(), bcode.end());
            }

            auto bcode = gen_code(func_call_node->func_expr, loop_stack);
            code.insert(code.end(), bcode.begin(), bcode.end());

            code.emplace_back(::irgen::CALL(
                func_call_node->args.size()
            ));
        } else if (node->kind == lmx::ASTNodeType::VMCall) {
            // TODO: VM调用暂时不处理
        } else if (node->kind == lmx::ASTNodeType::BlockStmt) {
            const auto* block_node = dynamic_cast<lmx::BlockStmtNode*>(node);
            for (const auto& stmt : block_node->stmts) {
                auto stmt_code = gen_code(stmt, loop_stack);
                code.insert(code.end(), stmt_code.begin(), stmt_code.end());
            }
        } else if (node->kind == lmx::ASTNodeType::VarDecl) {
            const auto* var_decl_node = dynamic_cast<lmx::VarDeclNode*>(node);
            if (var_decl_node->init) {
                if (var_decl_node->visibility == lmx::Visibility::Internal) {
                    if (var_decl_node->is_const) {
                        code.emplace_back(::irgen::NEW_INTERN_CONST(var_decl_node->name));
                    } else {
                        code.emplace_back(::irgen::NEW_INTERN_VAR(var_decl_node->name));
                    }
                } else {
                    if (var_decl_node->is_const) {
                        code.emplace_back(::irgen::NEW_CONST(var_decl_node->name));
                    } else {
                        code.emplace_back(::irgen::NEW_VAR(var_decl_node->name));
                    }
                }
                auto init_code = gen_code(var_decl_node->init, loop_stack);
                code.insert(code.end(), init_code.begin(), init_code.end());
                code.emplace_back(::irgen::STORE());
            }
        } else if (node->kind == lmx::ASTNodeType::Assign) {
            const auto* assign_node = dynamic_cast<lmx::AssignNode*>(node);
            if (assign_node->var->getValueCategory() != lmx::ValueCategory::LVALUE) {
                throw RuntimeError("Left-hand side of assignment must be an lvalue");
            }
            if (assign_node->var->kind == lmx::ASTNodeType::VarRef) {
                code.emplace_back(::irgen::NEW_VAR(dynamic_cast<lmx::VarRefNode*>(assign_node->var)->name));
            } else {
                auto var_code = gen_code(assign_node->var, loop_stack);
                code.insert(code.end(), var_code.begin(), var_code.end());
            }
            auto value_code = gen_code(assign_node->value, loop_stack);
            code.insert(code.end(), value_code.begin(), value_code.end());
            code.emplace_back(::irgen::STORE());
        } else if (node->kind == lmx::ASTNodeType::FuncDecl) {
            const auto* func_decl_node = dynamic_cast<lmx::FuncDeclNode*>(node);

            size_t func_label = label_counter++;
            size_t func_end_label = label_counter++;

            auto func_obj = std::make_shared<::irgen::FunctionObject>();
            func_obj->params = func_decl_node->params;
            func_obj->location = func_label;
            func_obj->name = func_decl_node->name;

            std::vector<::irgen::Opcode> func_body;

            func_body.emplace_back(::irgen::GOTO(func_end_label));
            func_body.emplace_back(::irgen::LABEL(func_label));
            func_body.emplace_back(::irgen::ENTER_SCOPE());

            for (const auto& param : func_decl_node->params) {
                func_body.emplace_back(::irgen::STORE_ARG(param));
            }

            if (func_decl_node->body) {
                auto body_code = gen_code(func_decl_node->body, loop_stack);
                func_body.insert(func_body.end(), body_code.begin(), body_code.end());
            }

            func_body.emplace_back(::irgen::PUSH(::irgen::Value()));
            func_body.emplace_back(::irgen::LEAVE_SCOPE());
            func_body.emplace_back(::irgen::RET());
            func_body.emplace_back(::irgen::LABEL(func_end_label));

            code.insert(code.end(), func_body.begin(), func_body.end());

            if (func_decl_node->visibility == lmx::Visibility::Internal) {
                code.emplace_back(::irgen::NEW_INTERN_VAR(func_decl_node->name));
            } else {
                code.emplace_back(::irgen::NEW_VAR(func_decl_node->name));
            }
            code.emplace_back(::irgen::PUSH(::irgen::Value(func_obj)));
            code.emplace_back(::irgen::STORE());
        } else if (node->kind == lmx::ASTNodeType::DoFuncDecl) {
            const auto* do_func_node = dynamic_cast<lmx::DoFuncDeclNode*>(node);
            size_t func_label = label_counter++;
            size_t func_end_label = label_counter++;

            auto func_obj = std::make_shared<::irgen::FunctionObject>();
            func_obj->params = do_func_node->params;
            func_obj->location = func_label;

            std::vector<::irgen::Opcode> func_body;

            func_body.emplace_back(::irgen::GOTO(func_end_label));
            func_body.emplace_back(::irgen::LABEL(func_label));
            func_body.emplace_back(::irgen::ENTER_SCOPE());

            for (const auto& param : do_func_node->params) {
                func_body.emplace_back(::irgen::STORE_ARG(param));
            }

            if (do_func_node->body) {
                auto body_code = gen_code(do_func_node->body, loop_stack);
                func_body.insert(func_body.end(), body_code.begin(), body_code.end());
            }

            func_body.emplace_back(::irgen::PUSH(::irgen::Value()));
            func_body.emplace_back(::irgen::LEAVE_SCOPE());
            func_body.emplace_back(::irgen::RET());
            func_body.emplace_back(::irgen::LABEL(func_end_label));

            code.insert(code.end(), func_body.begin(), func_body.end());

            code.emplace_back(::irgen::PUSH(::irgen::Value(func_obj)));
        } else if (node->kind == lmx::ASTNodeType::ExternFunc) {
            // TODO: 外部函数声明暂时不处理
        } else if (node->kind == lmx::ASTNodeType::ReturnStmt) {
            const auto* return_node = dynamic_cast<lmx::ReturnStmtNode*>(node);
            if (return_node->expr) {
                auto expr_code = gen_code(return_node->expr, loop_stack);
                code.insert(code.end(), expr_code.begin(), expr_code.end());
            } else {
                code.emplace_back(::irgen::PUSH(::irgen::Value()));
            }
            code.emplace_back(::irgen::LEAVE_SCOPE());
            code.emplace_back(::irgen::RET());
        } else if (node->kind == lmx::ASTNodeType::Loop) {
            const auto* loop_node = dynamic_cast<lmx::LoopNode*>(node);

            size_t loop_start_label = label_counter++;
            size_t loop_end_label = label_counter++;

            loop_stack.emplace(loop_start_label, loop_end_label);

            code.emplace_back(::irgen::LABEL(loop_start_label));

            if (loop_node->body) {
                auto body_code = gen_code(loop_node->body, loop_stack);
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
                auto cond_code = gen_code(while_node->condition, loop_stack);
                code.insert(code.end(), cond_code.begin(), cond_code.end());

                code.emplace_back(::irgen::NOT());
                code.emplace_back(::irgen::GOTOIF(loop_end_label));
            }

            if (while_node->body) {
                auto body_code = gen_code(while_node->body, loop_stack);
                code.insert(code.end(), body_code.begin(), body_code.end());
            }

            code.emplace_back(::irgen::GOTO(loop_start_label));
            code.emplace_back(::irgen::LABEL(loop_end_label));

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

            auto cond_code = gen_code(if_node->condition, loop_stack);
            code.insert(code.end(), cond_code.begin(), cond_code.end());

            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::GOTOIF(else_label));

            if (if_node->then_block) {
                auto then_code = gen_code(if_node->then_block, loop_stack);
                code.insert(code.end(), then_code.begin(), then_code.end());
            }

            if (if_node->else_block) {
                code.emplace_back(::irgen::GOTO(end_label));
            }

            code.emplace_back(::irgen::LABEL(else_label));
            if (if_node->else_block) {
                auto else_code = gen_code(if_node->else_block, loop_stack);
                code.insert(code.end(), else_code.begin(), else_code.end());
            }

            code.emplace_back(::irgen::LABEL(end_label));
        } else if (node->kind == lmx::ASTNodeType::Module) {
            // TODO: 模块暂时不处理
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
            code.emplace_back(::irgen::NEW_VAR(store_name));
            code.emplace_back(::irgen::FINDMOD(::irgen::Value(full_module_name)));
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
            auto obj_code = gen_code(member_node->object, loop_stack);
            code.insert(code.end(), obj_code.begin(), obj_code.end());
            code.emplace_back(::irgen::GETATTR(::irgen::Value(member_node->member)));
        } else if (node->kind == lmx::ASTNodeType::IndexAccess) {
            const auto* index_node = dynamic_cast<lmx::IndexAccessNode*>(node);
            auto obj_code = gen_code(index_node->object, loop_stack);
            auto index_code = gen_code(index_node->index, loop_stack);
            code.insert(code.end(), obj_code.begin(), obj_code.end());
            code.insert(code.end(), index_code.begin(), index_code.end());
            code.emplace_back(::irgen::INDEX());
        } else if (node->kind == lmx::ASTNodeType::Program) {
            const auto* program_node = dynamic_cast<lmx::ProgramASTNode*>(node);
            for (const auto& stmt : program_node->stmts) {
                auto stmt_code = gen_code(stmt, loop_stack);
                code.insert(code.end(), stmt_code.begin(), stmt_code.end());
            }
        } else if (node->kind == lmx::ASTNodeType::Expr) {
            const auto* expr_node = dynamic_cast<lmx::ExprNode*>(node);
            for (const auto i : expr_node->children) gen_code(i, loop_stack);
        }
        return code;
    }

    std::vector<::irgen::Opcode> Generator::gen() const {
        std::vector<::irgen::Opcode> code;
#ifdef DEBUG
        printAST(ast);
#endif

        std::stack<LoopLabels> loop_stack;
        for (const auto& stmt : ast->stmts) {
            auto stmt_code = gen_code(stmt, loop_stack);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }
#ifdef DEBUG
        std::string codes;
        size_t i = 0;
        for (auto const& elem : code) {
            std::visit([&](auto& op) {
                codes.append(std::format("{:3} | {}\n", i, op.toString()));
            }, elem);
            i++;
        }
        std::cerr << codes << std::endl;
#endif
        replace_string(code);
        return code;
    }

    // 包装函数：接收 ProgramASTNode，生成字节码并执行
    ::irgen::Value execute(const lmx::ProgramASTNode* program) {
        if (!program) {
            std::cerr << "Error: Null program AST\n";
            return {};
        }
        
        std::vector<::irgen::Opcode> code;

        std::stack<LoopLabels> loop_stack;
        for (const auto& stmt : program->stmts) {
            if (!stmt) continue;
            auto stmt_code = gen_code(stmt, loop_stack);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }

        int i = 0;
        for (const auto& c : code) {
            std::visit([&](const auto& op) {
                std::cerr << std::format("{} | {} {}", i, op.name(), [&]()->std::string {
                    std::string str;
                    for (const auto& operand : op.operands) {
                        str.append(operand.toString() + " ");
                    }
                    return str;
                }()) << std::endl;
            }, c);
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
