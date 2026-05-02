#include "debug.hpp"

#include <iostream>
#include <string>

#include "parser/ast.hpp"


void printAST(const lmx::ASTNode* node, const int indent) {
    if (!node) return;

    // 打印缩进
    const std::string indent_str(indent * 2, ' ');

    // 打印节点类型
    switch (node->kind) {
        case lmx::ASTNodeType::Program: std::cout << indent_str << "Program\n";
            for (const auto& stmt : dynamic_cast<lmx::ProgramASTNode*>(const_cast<lmx::ASTNode*>(node))->stmts) {
                printAST(stmt, indent + 1);
            }
            break;
        case lmx::ASTNodeType::VarDecl: {
            const auto var = dynamic_cast<lmx::VarDeclNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "VarDecl: " << var->name << "\n";
            if (var->init) {
                std::cout << indent_str << "  Init:\n";
                printAST(var->init, indent + 2);
            }
            break;
        }
        case lmx::ASTNodeType::Binary: {
            const auto bin = dynamic_cast<lmx::BinaryNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Binary: " << bin->op << "\n";
            std::cout << indent_str << "  Left:\n";
            printAST(bin->left, indent + 2);
            std::cout << indent_str << "  Right:\n";
            printAST(bin->right, indent + 2);
            break;
        }
        case lmx::ASTNodeType::Number: {
            const auto num = dynamic_cast<lmx::NumberNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Number: " << num->value << "\n";
            break;
        }
        case lmx::ASTNodeType::VarRef: {
            const auto var = dynamic_cast<lmx::VarRefNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "VarRef: " << var->name << "\n";
            break;
        }
        case lmx::ASTNodeType::String: {
            const auto num = dynamic_cast<lmx::StringNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "String: " << num->value << "\n";
            break;
        }
        case lmx::ASTNodeType::Unary: {
            const auto un = dynamic_cast<lmx::UnaryNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Unary: " << un->op << "\n";
            std::cout << indent_str << "  Operand:\n";
            printAST(un->operand, indent + 2);
            break;
        }
        case lmx::ASTNodeType::BlockStmt: {
            const auto block = dynamic_cast<lmx::BlockStmtNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "BlockStmt\n";
            for (const auto& stmt : block->stmts) {
                printAST(stmt, indent + 1);
            }
            break;
        }
        case lmx::ASTNodeType::IfStmt: {
            const auto if_node = dynamic_cast<lmx::IfStmtNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "IfStmt\n";
            std::cout << indent_str << "  Condition:\n";
            printAST(if_node->condition, indent + 2);
            std::cout << indent_str << "  Then:\n";
            printAST(if_node->then_block, indent + 2);
            if (if_node->else_block != nullptr) {
                std::cout << indent_str << "  Else:\n";
                printAST(if_node->else_block, indent + 2);
            }
            break;
        }
        case lmx::ASTNodeType::Loop: {
            const auto loop = dynamic_cast<lmx::LoopNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Loop\n";
            if (loop->condition != nullptr) {
                std::cout << indent_str << "  Condition:\n";
                printAST(loop->condition, indent + 2);
            }
            std::cout << indent_str << "  Body:\n";
            printAST(loop->body, indent + 2);
            break;
        }
        case lmx::ASTNodeType::Break: {
            std::cout << indent_str << "Break\n";
            break;
        }
        case lmx::ASTNodeType::Continue: {
            std::cout << indent_str << "Continue\n";
            break;
        }
        case lmx::ASTNodeType::Assign: {
            const auto assign = dynamic_cast<lmx::AssignNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Assign: " << assign->name << "\n";
            std::cout << indent_str << "  Value:\n";
            printAST(assign->value, indent + 2);
            break;
        }
        case lmx::ASTNodeType::FuncDecl: {
            const auto func = dynamic_cast<lmx::FuncDeclNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "FuncDecl: " << func->name << "\n";
            std::cout << indent_str << "  Params: ";
            for (size_t i = 0; i < func->params.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << func->params[i];
            }
            std::cout << "\n";
            std::cout << indent_str << "  Body:\n";
            printAST(func->body, indent + 2);
            break;
        }
        case lmx::ASTNodeType::DoFuncDecl: {
            const auto func = dynamic_cast<lmx::DoFuncDeclNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "DoFuncDecl\n";
            std::cout << indent_str << "  Params: ";
            for (size_t i = 0; i < func->params.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << func->params[i];
            }
            std::cout << "\n";
            std::cout << indent_str << "  Body:\n";
            printAST(func->body, indent + 2);
            break;
        }
        case lmx::ASTNodeType::FuncCallExpr: {
            const auto call = dynamic_cast<lmx::FuncCallExprNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "FuncCallExpr\n";
            std::cout << indent_str << "  Function:\n";
            printAST(call->func_expr, indent + 2);
            std::cout << indent_str << "  Args:\n";
            for (const auto& arg : call->args) {
                printAST(arg, indent + 2);
            }
            break;
        }
        case lmx::ASTNodeType::ReturnStmt: {
            const auto ret = dynamic_cast<lmx::ReturnStmtNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "ReturnStmt\n";
            if (ret->expr) {
                std::cout << indent_str << "  Value:\n";
                printAST(ret->expr, indent + 2);
            }
            break;
        }
        case lmx::ASTNodeType::Import: {
            const auto imp = dynamic_cast<lmx::ImportNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Import: ";
            for (size_t i = 0; i < imp->module_name.size(); ++i) {
                if (i > 0) std::cout << ".";
                std::cout << imp->module_name[i];
            }
            if (!imp->alias.empty()) {
                std::cout << " as " << imp->alias;
            }
            std::cout << "\n";
            break;
        }
        case lmx::ASTNodeType::Use: {
            const auto use = dynamic_cast<lmx::UseNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Use: ";
            for (size_t i = 0; i < use->module_name.size(); ++i) {
                if (i > 0) std::cout << ".";
                std::cout << use->module_name[i];
            }
            std::cout << "\n";
            std::cout << indent_str << "  Items:\n";
            for (const auto& item : use->items) {
                std::cout << indent_str << "    " << item.name;
                if (!item.alias.empty()) {
                    std::cout << " as " << item.alias;
                }
                std::cout << "\n";
            }
            break;
        }
        default: std::cout << indent_str << "Unknown Node Type\n";
    }
}
