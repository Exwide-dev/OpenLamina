#pragma once
#include <vector>

#include "ast.hpp"
#include "lexer/token.hpp"

lmx::ASTNode* parse(std::vector<lexer::Token> tokens);