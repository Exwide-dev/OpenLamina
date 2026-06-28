#pragma once

#include "opcode.hpp"

#include <cstddef>
#include <vector>

namespace lm::irgen {

/** @brief 字节码优化开关（默认开启；-O 仍可显式控制） */
inline bool bytecode_optimize_enabled = true;

/** @brief execute() 是否打印 AST / 字节码（测试默认关闭） */
inline bool execute_debug_output = false;

struct OptimizeReport {
    size_t ops_before = 0;
    size_t ops_after = 0;
    size_t constant_folds = 0;
    size_t jumps_threaded = 0;
    size_t dead_ops_removed = 0;
};

/** @brief 对字节码运行全部优化 pass，返回统计信息 */
[[nodiscard]] OptimizeReport optimize_bytecode(std::vector<::irgen::Opcode>& code);

/** @brief 若 bytecode_optimize_enabled 为 true 则优化 */
void maybe_optimize_bytecode(std::vector<::irgen::Opcode>& code);

} // namespace lm::irgen
