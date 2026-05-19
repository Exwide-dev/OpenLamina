#pragma once
#include "parser/ast.hpp"
#include "iostream"
#include "parser.tab.hpp"
#include "../../lexer_generated.h"

extern std::string detail_msg;


lmx::ProgramASTNode *parse(const std::string &source);
