#pragma once

#include "opcode.hpp"

namespace irgen {

/** @brief 创建迭代器（iter 协议：__iter__ / 内置序列 / __next__ 对象） */
[[nodiscard]] Value make_iter(VM& vm, const Value& obj);

/** @brief 推进迭代器；成功写入 out 并返回 true，耗尽返回 false */
[[nodiscard]] bool iterator_advance(VM& vm, const Value& iter_holder, Value& out);

/** @brief 调用 next(iter[, default]) 语义 */
[[nodiscard]] Value iterator_next(
    VM& vm,
    const Value& iter_holder,
    const std::optional<Value>& default_value = std::nullopt
);

} // namespace irgen
