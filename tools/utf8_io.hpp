#pragma once

#include <string>
#include <string_view>

namespace lm::utf8_io {
/// 约定：项目内部源码与标识符均使用 UTF-8 字节序列（与 std::u8string 编码一致）
using Utf8View = std::string_view;

/**
 * @brief 将 UTF-8 字节序列转为 std::string（显式标注语义，便于与 u8 字面量对接）
 */
[[nodiscard]] inline std::string from_u8_bytes(const char* bytes, const size_t len) {
    return {bytes, len};
}

[[nodiscard]] inline std::string from_u8_string(const std::u8string& text) {
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

/**
 * @brief 初始化标准输入/输出为 UTF-8（Windows 控制台）
 */
void init_stdio_utf8();

/**
 * @brief 从标准输入读取一行 UTF-8 文本
 */
[[nodiscard]] std::string read_line_utf8();

/**
 * @brief 将输入规范为有效 UTF-8；若非 UTF-8 则在 Windows 上尝试按系统 ANSI 代码页转码
 */
[[nodiscard]] std::string normalize_to_utf8(std::string_view input);

/**
 * @brief 以 UTF-8 读取整个文件（二进制读入，保留 BOM 供词法器处理）
 */
[[nodiscard]] std::string read_file_utf8(const std::string& path);
} // namespace lm::utf8_io