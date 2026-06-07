#pragma once
#include <vector>

namespace irgen {
class Value;
class VM;
class SymbolTable;
class ModuleObject;
}

namespace lang {
using irgen::Value;
using irgen::VM;
using ArgT = const std::vector<Value>&;

/**
 * @brief 读取用户输入
 * @param vm 虚拟机实例
 * @param args 参数列表
 * @return 用户输入的字符串
 */
Value input(VM& vm, ArgT args);

/**
 * @brief 打印输出
 * @param vm 虚拟机实例
 * @param args 参数列表
 * @return 空值
 */
Value print(VM& vm, ArgT args);

/**
 * @brief 显示版权信息
 * @param vm 虚拟机实例
 * @param args 参数列表
 * @return 版权信息字符串
 */
Value copyright(VM& vm, ArgT args);

/**
 * @brief 显示帮助信息
 * @param vm 虚拟机实例
 * @param args 参数列表
 * @return 帮助信息字符串
 */
Value help(VM& vm, ArgT args);

/**
 * @brief 退出程序
 * @param vm 虚拟机实例
 * @param args 参数列表
 * @return 空值
 */
Value exit(VM& vm, ArgT args);

/**
 * @brief 初始化内置函数到符号表
 * @param symbols 符号表
 */
void init_builtins(irgen::SymbolTable& symbols);

/**
 * @brief 标准模块实例
 */
extern irgen::ModuleObject standard_mod;
}
