#pragma once
#include <functional>
#include <iostream>
#include <stack>
#include <string>

#include "irgen/opcode.hpp"

namespace repl {

/**
 * @class REPL
 * @brief 交互式解释器类
 */
class REPL {
public:
    irgen::VM vm{}; ///< 虚拟机实例
    
    /**
     * @brief 默认构造函数
     */
    REPL() = default;
    
    /**
     * @brief 默认析构函数
     */
    ~REPL() = default;

    /**
     * @struct ExecResult
     * @brief 执行结果结构体
     */
    struct ExecResult {
        bool success;          ///< 是否执行成功
        bool needs_more_input; ///< 是否需要更多输入（多行输入场景）
    };

    /**
     * @brief 执行用户输入
     * @param input_func 输入获取函数，默认为从标准输入读取一行
     * @return 执行结果
     */
    ExecResult exec_input(
        const std::function<std::string()> &input_func = []() -> std::string {
            std::string line;
            std::getline(std::cin, line);
            return line;
        }
    );

private:
    /**
     * @enum BraceType
     * @brief 括号类型枚举
     */
    enum class BraceType {
        NONE,         ///< 无括号
        PAREN,        ///< 圆括号 ()
        BRACKET,      ///< 方括号 []
        BRACE_FUNC,   ///< 花括号 {}（函数体）
        BRACE_DICT    ///< 花括号 {}（字典）
    };

    std::stack<BraceType> brace_stack; ///< 括号栈，用于处理多行输入
    std::string pending_input;         ///< 待完成的输入
    bool in_string = false;            ///< 是否在字符串中
    char string_delimiter = '"';       ///< 字符串分隔符

    /**
     * @brief 更新解析状态
     * @param line 当前输入行
     */
    void update_state(const std::string& line);
    
    /**
     * @brief 判断是否需要更多输入
     * @return 如果需要更多输入返回true
     */
    bool needs_more_input() const;
    
    /**
     * @brief 重置解析状态
     */
    void reset_state();
};

}