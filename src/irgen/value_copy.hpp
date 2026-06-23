#pragma once

#include "opcode.hpp"

namespace irgen {

/** @brief 将 Value 从 VM cell pool 中递归脱离，得到可独立持有的副本 */
[[nodiscard]] Value detach_value(const Value& value);

} // namespace irgen
