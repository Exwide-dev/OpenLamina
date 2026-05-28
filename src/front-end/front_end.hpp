#pragma once
#include "parser/ast.hpp"
#include <string>

extern std::string detail_msg;

lmx::ProgramASTNode* parse(const std::string &source);
lmx::ProgramASTNode* parse(const std::string &source, const std::string& filename);
