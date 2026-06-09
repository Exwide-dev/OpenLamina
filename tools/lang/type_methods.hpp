#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace irgen {
class Value;
class VM;
using FunctionType = std::function<Value(VM&, const std::vector<Value>&)>;
} // namespace irgen

namespace lang {

/** @brief 为槽位左值绑定内置方法；receiver 指向变量单元内的 Value */
std::optional<irgen::FunctionType> bind_method(
    const std::shared_ptr<irgen::Value>& receiver,
    const std::string& method_name
);

} // namespace lang
