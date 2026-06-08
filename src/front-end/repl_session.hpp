#pragma once

#include <string>
#include <vector>

#include "../lexer/lexer.hpp"
#include "../parser/ast.hpp"
#include "../parser/parser.hpp"

namespace lmx {
/**
 * @brief REPL 增量解析会话：跨多次输入保持词法/语法状态与全局行号
 */
class ReplSession {
public:
    explicit ReplSession(std::string filename = "<repl>");

    /**
     * @brief 解析并返回本段输入产生的新顶层语句（调用方负责 delete 各语句节点）
     */
    std::vector<ASTNode*> parse_chunk(const std::string& source);

    void reset();

    [[nodiscard]] const std::string& cumulative_source() const { return cumulative_source_; }

    [[nodiscard]] const std::vector<std::string>& source_lines() const {
        return parser_.get_source_lines();
    }

private:
    std::string filename_;
    Lexer lexer_;
    Parser parser_;
    std::string cumulative_source_;
};
} // namespace lmx