#include "generator.hpp"

#include <format>

#include "opcode.hpp"

namespace lm::irgen {
    std::vector<::irgen::Opcode*> gen_code(lmx::ASTNode* node) {
        std::vector<::irgen::Opcode*> code;

        if (node->kind == lmx::ASTNodeType::Number) {
            const auto* num_node = dynamic_cast<lmx::NumberNode*>(node);
            int value = std::stoi(num_node->value);
            code.push_back(new ::irgen::PUSH(::irgen::Value(value)));
        } else if (node->kind == lmx::ASTNodeType::Bool) {
            const auto* bool_node = dynamic_cast<lmx::BoolNode*>(node);
            code.push_back(new ::irgen::PUSH(::irgen::Value(bool_node->value)));
        } else if (node->kind == lmx::ASTNodeType::String) {
            const auto* string_node = dynamic_cast<lmx::StringNode*>(node);
            code.push_back(new ::irgen::PUSH(::irgen::Value(string_node->value)));
        } else if (node->kind == lmx::ASTNodeType::Binary) {
            const auto* bin_node = dynamic_cast<lmx::BinaryNode*>(node);
            auto left_code = gen_code(bin_node->left);
            auto right_code = gen_code(bin_node->right);
            code.insert(code.end(), left_code.begin(), left_code.end());
            code.insert(code.end(), right_code.begin(), right_code.end());

            if (bin_node->op == "+") {
                code.push_back(new ::irgen::ADD());
            } else if (bin_node->op == "*") {
                code.push_back(new ::irgen::MUL());
            } else if (bin_node->op == "-") {
                code.push_back(new ::irgen::SUB());
            } else if (bin_node->op == "/") {
                code.push_back(new ::irgen::DIV());
            } else if (bin_node->op == "&&") {
                code.push_back(new ::irgen::AND());
            } else if (bin_node->op == "||") {
                code.push_back(new ::irgen::OR());
            } else if (bin_node->op == "==") {
                code.push_back(new ::irgen::EQ());
            } else if (bin_node->op == "!=") {
                code.push_back(new ::irgen::NEQ());
            } else if (bin_node->op == "<") {
                code.push_back(new ::irgen::LT());
            } else if (bin_node->op == "<=") {
                code.push_back(new ::irgen::LTE());
            } else if (bin_node->op == ">") {
                code.push_back(new ::irgen::GT());
            } else if (bin_node->op == ">=") {
                code.push_back(new ::irgen::GTE());
            }
        } else if (node->kind == lmx::ASTNodeType::Unary) {
            const auto* unary_node = dynamic_cast<lmx::UnaryNode*>(node);
            auto operand_code = gen_code(unary_node->operand);
            code.insert(code.end(), operand_code.begin(), operand_code.end());

            if (unary_node->op == "-") {
                code.push_back(new ::irgen::NEG());
            } else if (unary_node->op == "!") {
                code.push_back(new ::irgen::NOT());
            }
        } else if (node->kind == lmx::ASTNodeType::VarRef) {
            const auto* var_ref_node = dynamic_cast<lmx::VarRefNode*>(node);
            code.push_back(new ::irgen::LOAD(var_ref_node->name));
        } else if (node->kind == lmx::ASTNodeType::FuncCallExpr) {
            // 函数调用暂时不处理，需要函数表支持
        } else if (node->kind == lmx::ASTNodeType::VMCall) {
            // VM调用暂时不处理
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
                code.push_back(new ::irgen::STORE(var_decl_node->name));
            }
        } else if (node->kind == lmx::ASTNodeType::Assign) {
            const auto* assign_node = dynamic_cast<lmx::AssignNode*>(node);
            auto value_code = gen_code(assign_node->value);
            code.insert(code.end(), value_code.begin(), value_code.end());
            code.push_back(new ::irgen::STORE(assign_node->name));
        } else if (node->kind == lmx::ASTNodeType::FuncDecl) {
            // 函数声明暂时不处理，需要函数表支持
        } else if (node->kind == lmx::ASTNodeType::ExternFunc) {
            // 外部函数声明暂时不处理
        } else if (node->kind == lmx::ASTNodeType::ReturnStmt) {
            const auto* return_node = dynamic_cast<lmx::ReturnStmtNode*>(node);
            if (return_node->expr) {
                auto expr_code = gen_code(return_node->expr);
                code.insert(code.end(), expr_code.begin(), expr_code.end());
            }
        } else if (node->kind == lmx::ASTNodeType::Loop) {
            // 循环语句暂时不处理
        } else if (node->kind == lmx::ASTNodeType::Break) {
            // break语句暂时不处理
        } else if (node->kind == lmx::ASTNodeType::Continue) {
            // continue语句暂时不处理
        } else if (node->kind == lmx::ASTNodeType::IfStmt) {
            // if语句暂时不处理
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

    std::vector<::irgen::Opcode*> Generator::gen() const {
        std::vector<::irgen::Opcode*> code;

        for (const auto& stmt : ast->stmts) {
            auto stmt_code = gen_code(stmt);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }

        return code;
    }

    // 包装函数：接收 ProgramASTNode，生成字节码并执行
    ::irgen::Value execute(const lmx::ProgramASTNode* program) {
        std::vector<::irgen::Opcode*> code;

        for (const auto& stmt : program->stmts) {
            auto stmt_code = gen_code(stmt);
            code.insert(code.end(), stmt_code.begin(), stmt_code.end());
        }

        std::cerr << "LOGGING CODE" << std::endl;

        int i = 0;
        for (const auto& c : code) {
            std::cerr << std::format("{} | {} {}", i, c->name(), [&]()->std::string {
                std::string str;
                for (const auto& op : c->operands) {
                    // std::cerr << "Try to asString()" << std::endl;
                    str.append(op.toString() + " ");
                }
                return str;
            }()) << std::endl;
            i++;
        }

        std::cerr << "VM Exec" << std::endl;

        ::irgen::VM vm(code);
        vm.run();

        if (!vm.op_stack.empty()) {
            return vm.op_stack.top();
        }
        return {};
    }
}