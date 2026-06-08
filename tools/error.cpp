#include "error.hpp"
#include "cli.hpp"

#include <format>

std::string format_trace_frame(const TraceFrame& frame) {
    if (frame.line <= 0 && frame.source_line.empty()) {
        return "  ...\n";
    }

    std::string result = std::format("\033[1;31mFile \"{}\", line {}", frame.filename, frame.line);
    if (frame.column > 0) {
        result += std::format(", column {}", frame.column);
    }
    result += std::format(", in {}\033[0m\n", frame.scope);
    if (!frame.source_line.empty()) {
        result += ">>> " + frame.source_line + "\n";
    } else {
        result += ">>> ...\n";
    }
    return result;
}

std::string format_runtime_traceback(const RuntimeError& e) {
    if (e.traceback().empty() &&
        e.error_site().line <= 0 &&
        e.error_site().source_line.empty()) {
        return "";
    }

    std::string result = "\nTraceback (most recent call last):\n";
    for (const auto& frame : e.traceback()) {
        result += format_trace_frame(frame);
    }
    result += format_trace_frame(e.error_site());
    return result;
}
