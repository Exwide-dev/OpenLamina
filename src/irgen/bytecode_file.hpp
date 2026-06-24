#pragma once

#include "opcode.hpp"
#include "optimizer.hpp"
#include "parser/ast.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace lm::irgen {

/** @brief .lmc 文件格式版本 */
constexpr uint32_t LMC_FORMAT_VERSION = 1;

/** @brief 已编译模块：字符串池、用户类型定义与主程序字节码 */
struct CompiledModule {
    std::string source_filename;
    std::vector<std::string> string_pool;
    std::vector<::irgen::StructTypeDef> type_defs;
    std::vector<::irgen::Opcode> code;
    bool optimized = false;
    OptimizeReport optimize_report{};
};

/** @brief 将 AST 编译为字节码（不运行 VM） */
[[nodiscard]] CompiledModule compile_ast(const lmx::ProgramASTNode* program);

/** @brief 编译并在写入前对全部嵌套 IR 运行优化器 */
[[nodiscard]] CompiledModule compile_ast_optimized(const lmx::ProgramASTNode* program);

/** @brief 递归优化字节码及 PUSH 中的函数 / 类型方法体 */
[[nodiscard]] OptimizeReport optimize_bytecode_deep(std::vector<::irgen::Opcode>& code);

/** @brief 将模块写入 .lmc 二进制文件 */
void save_lmc(const std::string& path, const CompiledModule& module);

/** @brief 从 .lmc 文件加载模块 */
[[nodiscard]] CompiledModule load_lmc(const std::string& path);

/** @brief 执行已加载模块；on_result 在 VM 仍存活时调用 */
bool run_compiled_module(
    CompiledModule& module,
    const std::function<bool(::irgen::VM& vm)>& on_result = {}
);

} // namespace lm::irgen
