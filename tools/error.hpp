#pragma once

#include <stdexcept>

/**
 * @class SyntaxError
 * @brief 语法错误异常类
 */
class SyntaxError final : public std::runtime_error {
public:
    /**
     * @brief 构造函数
     * @param msg 错误信息
     */
    explicit SyntaxError(const std::string &msg) : std::runtime_error(msg) {}
};

/**
 * @class RuntimeError
 * @brief 运行时错误异常类
 */
class RuntimeError final : public std::runtime_error {
public:
    /**
     * @brief 构造函数
     * @param msg 错误信息
     */
    explicit RuntimeError(const std::string &msg) : std::runtime_error(msg) {}
};