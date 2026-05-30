#pragma once
#include "parser/ast.hpp"
#include <string>

/**
 * @brief 详细错误信息
 */
extern std::string detail_msg;

/**
 * @brief 解析源代码字符串
 * @param source 源代码字符串
 * @return 程序AST根节点
 */
lmx::ProgramASTNode* parse(const std::string &source);

/**
 * @brief 解析源代码字符串（带文件名）
 * @param source 源代码字符串
 * @param filename 源文件名
 * @return 程序AST根节点
 */
lmx::ProgramASTNode* parse(const std::string &source, const std::string& filename);