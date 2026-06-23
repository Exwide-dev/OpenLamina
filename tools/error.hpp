#pragma once

#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief 运行时错误 traceback 中的一帧
 */
struct TraceFrame {
    std::string filename = "<unknown>";
    int line = 0;
    int column = 0;
    std::string scope = "global-scope";
    std::string source_line;
};

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
    explicit SyntaxError(const std::string& msg) : std::runtime_error(msg) {
    }
};

/**
 * @class RuntimeError
 * @brief 运行时错误异常类
 */
class RuntimeError final : public std::runtime_error {
    TraceFrame error_site_;
    std::vector<TraceFrame> traceback_;
    std::string exception_type_;
    bool stop_iteration_ = false;

public:
    RuntimeError(std::string msg, TraceFrame error_site = {}, std::vector<TraceFrame> traceback = {})
        : std::runtime_error(std::move(msg)),
          error_site_(std::move(error_site)),
          traceback_(std::move(traceback)) {
    }

    [[nodiscard]] const TraceFrame& error_site() const { return error_site_; }
    [[nodiscard]] const std::vector<TraceFrame>& traceback() const { return traceback_; }

    /** @deprecated 兼容旧接口；优先使用 error_site().source_line */
    [[nodiscard]] const std::string& source_line() const { return error_site_.source_line; }

    void set_exception_type(std::string type) { exception_type_ = std::move(type); }
    [[nodiscard]] const std::string& exception_type() const { return exception_type_; }

    void mark_stop_iteration() { stop_iteration_ = true; }
    [[nodiscard]] bool is_stop_iteration() const { return stop_iteration_; }
};

[[nodiscard]] std::string format_runtime_error_message(const RuntimeError& e);

std::string format_trace_frame(const TraceFrame& frame);
std::string format_runtime_traceback(const RuntimeError& e);
