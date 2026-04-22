#include <iostream>
#include <vector>
#include <string>
#include "parser.tab.hpp"
#include "parser/ast.hpp"

// 声明 Bison 和 Flex 解析函数
extern int yyparse();
extern void yyrestart(FILE*);

// 存储解析结果
lmx::ASTNode* result = nullptr;

// yylval 由 Bison 自动定义

// 声明 Flex 函数
extern "C" {
    void* yy_scan_string(const char*);
    void yy_delete_buffer(void*);
}

// 打印 AST 节点
void printAST(const lmx::ASTNode* node, const int indent = 0) {
    if (not node) return;

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
        case lmx::ASTNodeType::FuncCallExpr: {
            const auto call = dynamic_cast<lmx::FuncCallExprNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "FuncCallExpr: " << call->func_name << "\n";
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
        default: std::cout << indent_str << "Unknown Node Type\n";
    }
}

int main() {
    const std::string source = R"(
    func add(a, b) {
        return a + b
    }
    func multiply(a, b) {
        return a * b
    }
    let x = 10
    let y = 20
    let result = add(x, y)
    let product = multiply(5, 3)
    func factorial(n) {
        if (n <= 1) {
            return 1
        }
        return n * factorial(n - 1)
    }
    let fact5 = factorial(5)
    )";

    std::cout << "Source code:\n" << source << "\n";

    // 解析并显示 AST
    // 将源代码写入临时文件
    if (FILE* temp_file = fopen("__temp_source.tmp", "w")) {
        fprintf(temp_file, "%s", source.c_str());
        fclose(temp_file);

        // 重新打开文件进行解析
        temp_file = fopen("__temp_source.tmp", "r");
        if (temp_file) {
            yyrestart(temp_file);
            std::cout << "\nParsing...\n";
            const int parse_result = yyparse();
            fclose(temp_file);

            if (parse_result == 0) {
                std::cout << "\nParsing successful!\n";
                std::cout << "AST:\n";
                printAST(result);
                // 释放 AST 内存
                delete result;
            } else {
                std::cout << "\nParsing failed!\n";
            }
        }
    }
    std::cin.get();
    return 0;
}
