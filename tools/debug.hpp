#pragma once

#include "../src/parser/ast.hpp"

#define ANALYSE
#undef ANALYSE

#define DEBUG
#undef DEBUG
#ifdef DEBUG

#define ITIS(...) \
ITIS_DISPATCH(__VA_ARGS__, ITIS3, ITIS2, ITIS1)(__VA_ARGS__)

#define ITIS_DISPATCH(a, b, c, name, ...) name

#define ITIS1(expr) \
std::string(#expr) + ": " + std::to_string(expr)

#define ITIS2(fn, expr) \
std::string(#expr) + ": " + fn(expr)

#define ITIS3(fn, expr, suffix) \
std::string(#expr) + ": " + fn(expr)suffix

#define LOG(msg) \
std::cout << "\033[33m[DEBUG] " << __FILE__ << ":" << __LINE__ << " | " << msg << "\033[0m" << std::endl

#else

#define LOG(msg)
#define ITIS(...)

#endif

void printAST(const lmx::ASTNode* node, int indent = 0);
