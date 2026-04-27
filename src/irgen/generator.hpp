#pragma once
#include "opcode.hpp"
#include "parser/ast.hpp"

namespace lm::irgen {
    class Generator {
    public:
        lmx::ProgramASTNode* ast;
        explicit Generator(lmx::ProgramASTNode* ast) : ast(ast) {};
        ~Generator() = default;

        [[nodiscard]] std::vector<::irgen::Opcode> gen() const;
    };

    // 包装函数：接收 ProgramASTNode，生成字节码并执行
    ::irgen::Value execute(const lmx::ProgramASTNode* program);
}
