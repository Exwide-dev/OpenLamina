#pragma once

#include "opcode.hpp"

namespace irgen {

void register_builtin_exceptions();

[[nodiscard]] std::string exception_type_name(const Value& exc);
[[nodiscard]] std::string exception_message(const Value& exc);

[[nodiscard]] Value make_exception_instance(
    VM& vm,
    const char* type_name,
    const std::string& message = ""
);

/** @brief 是否为内置/用户异常实例（BaseException 子类） */
[[nodiscard]] bool value_is_exception(const Value& value);

[[nodiscard]] std::vector<TraceFrame> build_traceback(const VM& vm);
[[nodiscard]] TraceFrame vm_exception_site(const VM& vm);

void unwind_to_try_frame(VM& vm, const TryHandlerFrame& frame);

/** @return true 若异常已被 try/catch 接管（pc 已跳转） */
[[nodiscard]] bool dispatch_exception(
    VM& vm,
    const Value& exc,
    TraceFrame site = {},
    std::vector<TraceFrame> traceback = {}
);

[[nodiscard]] bool dispatch_runtime_error(VM& vm, const RuntimeError& err);

void throw_user_exception(VM& vm, const Value& exc);
[[noreturn]] void raise_stop_iteration(VM& vm);

[[nodiscard]] bool is_stop_iteration(const RuntimeError& error);

[[noreturn]] void raise_cpp_runtime_error(
    VM& vm,
    std::string msg,
    std::string type_name = {},
    bool stop_iteration = false
);

} // namespace irgen
