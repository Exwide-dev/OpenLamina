#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include "parser.tab.hpp"
#include "lexer/token.hpp"
#include "parser/ast.hpp"

// 声明 Bison 和 Flex 解析函数
extern int yyparse();
extern void yyrestart(FILE*);

// 存储解析结果
lmx::ASTNode* result = nullptr;

// yylval 由 Bison 自动定义

// 打印 AST 节点
void printAST(const lmx::ASTNode* node, int indent = 0) {
    if (!node) return;
    
    // 打印缩进
    std::string indent_str(indent * 2, ' ');
    
    // 打印节点类型
    switch (node->kind) {
        case lmx::ASTNodeType::Program: std::cout << indent_str << "Program\n";
            for (const auto& stmt : dynamic_cast<lmx::ProgramASTNode*>(const_cast<lmx::ASTNode*>(node))->stmts) {
                printAST(stmt, indent + 1);
            }
            break;
        case lmx::ASTNodeType::VarDecl: {
            auto var = dynamic_cast<lmx::VarDeclNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "VarDecl: " << var->name << "\n";
            if (var->init) {
                std::cout << indent_str << "  Init:\n";
                printAST(var->init, indent + 2);
            }
            break;
        }
        case lmx::ASTNodeType::Binary: {
            auto bin = dynamic_cast<lmx::BinaryNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Binary: " << bin->op << "\n";
            std::cout << indent_str << "  Left:\n";
            printAST(bin->left, indent + 2);
            std::cout << indent_str << "  Right:\n";
            printAST(bin->right, indent + 2);
            break;
        }
        case lmx::ASTNodeType::Number: {
            auto num = dynamic_cast<lmx::NumberNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "Number: " << num->value << "\n";
            break;
        }
        case lmx::ASTNodeType::VarRef: {
            auto var = dynamic_cast<lmx::VarRefNode*>(const_cast<lmx::ASTNode*>(node));
            std::cout << indent_str << "VarRef: " << var->name << "\n";
            break;
        }
        default: std::cout << indent_str << "Unknown Node Type\n";
    }
}

int main() {
    const std::string source = R"(
    let x = 10 + 5 * 2
    )";

    std::cout << "Source code:\n" << source << "\n";

    // 解析并显示 AST
    // 将源代码写入临时文件
    FILE* temp_file = fopen("temp_source.txt", "w");
    if (temp_file) {
        fprintf(temp_file, "%s", source.c_str());
        fclose(temp_file);

        // 重新打开文件进行解析
        temp_file = fopen("temp_source.txt", "r");
        if (temp_file) {
            yyrestart(temp_file);
            std::cout << "\nParsing...\n";
            int parse_result = yyparse();
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
