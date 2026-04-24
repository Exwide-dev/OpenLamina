#include <iostream>
#include <vector>
#include <string>
#include "lexer_generated.h"
#include "parser.tab.hpp"
#include "lexer/lex.hpp"
#include "lexer/token.hpp"
#include "parser/ast.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "irgen/opcode.hpp"
#include "repl/repl.hpp"


// 存储解析结果

// yylval 由 Bison 自动定义

// 声明 Flex 函数

extern "C" {
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

[[noreturn]] int main() {
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
    std::cout << "\nParsing...\n";
    const lmx::ProgramASTNode* ast = parse(source);
    printAST(ast);

    // 手动构建一个简单的 AST 进行测试
    std::cout << "\nTesting with manually built AST...\n";
    const auto* manual_ast = parse("let a = 10 + 5 * 2\na");
    printAST(manual_ast);

    const ::irgen::Value manual_result = lm::irgen::execute(manual_ast);
    if (!manual_result.isNone()) {
        std::cout << "Manual AST execution result: " << manual_result << std::endl;
    } else {
        std::cout << "Manual AST execution resulted in no value" << std::endl;
    }

    // 释放内存
    delete ast;
    delete manual_ast;

    std::cout << "Then, REPL" << std::endl;

    auto repl = repl::REPL();
    while (true) {
        repl.exec_input([&]() -> std::string {
            std::cout << "\n>>> ";
            std::string line;
            std::getline(std::cin, line);  // 先读取，再返回
            return line;
        });
        if (!repl.vm.op_stack.empty()) {
            std::cout << repl.vm.op_stack.top();
        }
    }
}
