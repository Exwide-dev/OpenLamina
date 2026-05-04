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

        static void replace_string(std::vector<::irgen::Opcode>& codes) {
            for (auto& code : codes) {
                std::visit([&](auto& op) {
                    const std::string op_name = op.name();
                    if (op_name == "PUSH") return;
                    for (auto& operand : op.operands) {
                        if (operand.isString()) {
                            const std::string name = operand.asString();
                            size_t id = ::irgen::g_string_pool.add(name);
                            operand = ::irgen::Value(id);
                        }
                    }
                }, code);
            }
        }
    };

    // 包装函数：接收 ProgramASTNode，生成字节码并执行
    ::irgen::Value execute(const lmx::ProgramASTNode* program);
}
