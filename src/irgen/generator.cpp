#include "generator.hpp"
#include "../tools/debug.hpp"
#include "../tools/lang/builtins.hpp"

#include <format>
#include <ranges>

#include "opcode.hpp"

namespace lm::irgen {
    static size_t label_counter = 0;
    std::vector<::irgen::Opcode> gen_code(lmx::ASTNode* node) {
        std::vector<::irgen::Opcode> code;

        if (node->kind == lmx::ASTNodeType::Number) {
            const auto* num_node = dynamic_cast<lmx::NumberNode*>(node);
            ptrdiff_t value = std::stoll(num_node->value);
            code.emplace_back(::irgen::PUSH(::irgen::Value(value)));
        } else if (node->kind == lmx::ASTNodeType::Bool) {
            const auto* bool_node = dynamic_cast<lmx::BoolNode*>(node);
            code.emplace_back(::irgen::PUSH(::irgen::Value(bool_node->value)));
        } else if (node->kind == lmx::ASTNodeType::String) {
            const auto* string_node = dynamic_cast<lmx::StringNode*>(node);
            code.emplace_back(::irgen::PUSH(::irgen::Value(string_node->value)));
        } else if (node->kind == lmx::ASTNodeType::Binary) {
            const auto* bin_node = dynamic_cast<lmx::BinaryNode*>(node);
            auto left_code = gen_code(bin_node->left);
            auto right_code = gen_code(bin_node->right);
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
            auto operand_code = gen_code(unary_node->operand);
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
            // 从右到左压入实参
            for (auto arg : std::ranges::reverse_view(func_call_node->args)) {
                auto bcode = gen_code(arg);
                code.insert(code.end(), bcode.begin(), bcode.end());
            }
            code.emplace_back(::irgen::CALL(
                func_call_node->func_name,
                func_call_node->args.size()
            ));
        } else if (node->kind == lmx::ASTNodeType::VMCall) {
            // TODO: VM调用暂时不处理
        } else if (node->kind == lmx::ASTNodeType::BlockStmt) {
            const auto* block_node = dynamic_cast<lmx::BlockStmtNode*>(node);
            for (const auto& stmt : block_node->stmts) {
                auto stmt_code = gen_code(stmt);
                code.insert(code.end(), stmt_code.begin(), stmt_code.end());
            }
        } else if (node->kind == lmx::ASTNodeType::VarDecl) {
            const auto* var_decl_node = dynamic_cast<lmx::VarDeclNode*>(node);
            if (var_decl_node->init) {
                auto init_code = gen_code(var_decl_node->init);
                code.insert(code.end(), init_code.begin(), init_code.end());
                code.emplace_back(::irgen::STORE(var_decl_node->name));
            }
        } else if (node->kind == lmx::ASTNodeType::Assign) {
            const auto* assign_node = dynamic_cast<lmx::AssignNode*>(node);
            auto value_code = gen_code(assign_node->value);
            code.insert(code.end(), value_code.begin(), value_code.end());
            code.emplace_back(::irgen::STORE(assign_node->name));
        } else if (node->kind == lmx::ASTNodeType::FuncDecl) {
            const auto* func_decl_node = dynamic_cast<lmx::FuncDeclNode*>(node);
            
            // 生成函数标签ID
            size_t func_label = label_counter++;
            size_t func_end_label = label_counter++;
            
            // 创建函数对象
            auto func_obj = std::make_shared<::irgen::FunctionObject>();
            func_obj->params = func_decl_node->params;
            func_obj->location = func_label;
            
            // 生成函数体的指令序列
            std::vector<::irgen::Opcode> func_body;
            
            // 函数开始标签
            func_body.emplace_back(::irgen::GOTO(func_end_label));
            func_body.emplace_back(::irgen::LABEL(func_label));
            
            // ENTER_SCOPE
            func_body.emplace_back(::irgen::ENTER_SCOPE());
            
            // 从栈中弹出参数并存储到当前作用域
            for (const auto& param : func_decl_node->params) {
                func_body.emplace_back(::irgen::STORE(param));
            }
            
            // 生成函数体的字节码
            if (func_decl_node->body) {
                auto body_code = gen_code(func_decl_node->body);
                func_body.insert(func_body.end(), body_code.begin(), body_code.end());
            }
            
            // 默认返回值 None
            func_body.emplace_back(::irgen::PUSH(::irgen::Value()));
            
            // LEAVE_SCOPE
            func_body.emplace_back(::irgen::LEAVE_SCOPE());
            
            // RET
            func_body.emplace_back(::irgen::RET());
            
            // 函数结束标签（可选）
            func_body.emplace_back(::irgen::LABEL(func_end_label));
            
            // 将函数体指令添加到主代码中
            code.insert(code.end(), func_body.begin(), func_body.end());
            
            // 创建函数对象并存储到符号表
            auto func = ::irgen::Value(func_obj);
            code.emplace_back(::irgen::PUSH(func));
            code.emplace_back(::irgen::STORE(func_decl_node->name));
        } else if (node->kind == lmx::ASTNodeType::ExternFunc) {
            // 外部函数声明暂时不处理
        } else if (node->kind == lmx::ASTNodeType::ReturnStmt) {
            const auto* return_node = dynamic_cast<lmx::ReturnStmtNode*>(node);
            if (return_node->expr) {
                // 计算返回值表达式
                auto expr_code = gen_code(return_node->expr);
                code.insert(code.end(), expr_code.begin(), expr_code.end());
            } else {
                // 如果没有返回值，压入一个None值
                code.emplace_back(::irgen::PUSH(::irgen::Value()));
            }
            // LEAVE_SCOPE
            code.emplace_back(::irgen::LEAVE_SCOPE());
            // RET
            code.emplace_back(::irgen::RET());
        } else if (node->kind == lmx::ASTNodeType::Loop) {
            // 循环语句暂时不处理
        } else if (node->kind == lmx::ASTNodeType::Break) {
            // break语句暂时不处理
        } else if (node->kind == lmx::ASTNodeType::Continue) {
            // continue语句暂时不处理
        } else if (node->kind == lmx::ASTNodeType::IfStmt) {
            const auto* if_node = dynamic_cast<lmx::IfStmtNode*>(node);

            size_t else_label = label_counter++;
            size_t end_label = label_counter++;

            // 条件表达式：压入结果到栈
            auto cond_code = gen_code(if_node->condition);
            code.insert(code.end(), cond_code.begin(), cond_code.end());

            // 对条件取反 → 如果原条件 false，则栈顶为 true → 跳到 else
            code.emplace_back(::irgen::NOT());
            code.emplace_back(::irgen::IFTRUEGOTO(else_label));

            // Then 块
            if (if_node->then_block) {
                auto then_code = gen_code(if_node->then_block);
                code.insert(code.end(), then_code.begin(), then_code.end());
            }

            // 跳过 else 块（如果有）
            if (if_node->else_block) {
                code.emplace_back(::irgen::GOTO(end_label));
            }

            // Else 块
            code.emplace_back(::irgen::LABEL(else_label));
            if (if_node->else_block) {
                auto else_code = gen_code(if_node->else_block);
                code.insert(code.end(), else_code.begin(), else_code.end());
            }

            // 结束标签
            code.emplace_back(::irgen::LABEL(end_label));
        } else if (node->kind == lmx::ASTNodeType::Module) {
            // 模块暂时不处理
        } else if (node->kind == lmx::ASTNodeType::Program) {
            const auto* program_node = dynamic_cast<lmx::ProgramASTNode*>(node);
            for (const auto& stmt : program_node->stmts) {
                auto stmt_code = gen_code(stmt);
                code.insert(code.end(), stmt_code.begin(), stmt_code.end());
            }
        }
        return code;
    }

    std::vector<::irgen::Opcode> Generator::gen() const {
        std::vector<::irgen::Opcode> code;
#ifdef DEBUG
        printAST(ast);
#endif

        for (const auto& stmt : ast->stmts) {
            auto stmt_code = gen_code(stmt);
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
        std::vector<::irgen::Opcode> code;

        for (const auto& stmt : program->stmts) {
            auto stmt_code = gen_code(stmt);
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

        LOG("VM Exec");

        ::irgen::VM vm(code);
        vm.run();


        if (!vm.op_stack.empty()) {
            return vm.op_stack.top();
        }
        return {};
    }
}
