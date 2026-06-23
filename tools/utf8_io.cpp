#include "utf8_io.hpp"

#include "../tools/error.hpp"
#include "../src/lexer/utf8.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace lm::utf8_io {
void init_stdio_utf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

#ifdef _WIN32
namespace {
std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        static_cast<int>(wide.size()),
        out.data(),
        size,
        nullptr,
        nullptr
    );
    return out;
}

std::string acp_to_utf8(const std::string& bytes) {
    if (bytes.empty()) {
        return {};
    }
    const int wide_len = MultiByteToWideChar(
        CP_ACP,
        MB_ERR_INVALID_CHARS,
        bytes.data(),
        static_cast<int>(bytes.size()),
        nullptr,
        0
    );
    if (wide_len <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(
        CP_ACP,
        MB_ERR_INVALID_CHARS,
        bytes.data(),
        static_cast<int>(bytes.size()),
        wide.data(),
        wide_len
    );
    return wide_to_utf8(wide);
}

std::string read_line_utf8_windows() {
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (stdin_handle == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::wstring buffer;
    buffer.reserve(256);
    wchar_t chunk[128];
    DWORD read_count = 0;

    while (true) {
        if (!ReadConsoleW(stdin_handle, chunk, 128, &read_count, nullptr)) {
            break;
        }
        if (read_count == 0) {
            break;
        }
        buffer.append(chunk, read_count);
        if (read_count < 128 || chunk[read_count - 1] == L'\n') {
            break;
        }
    }

    while (!buffer.empty() && (buffer.back() == L'\n' || buffer.back() == L'\r')) {
        buffer.pop_back();
    }

    return wide_to_utf8(buffer);
}
}
#endif

std::string read_line_utf8() {
#ifdef _WIN32
    return read_line_utf8_windows();
#else
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
#endif
}

std::string normalize_to_utf8(const std::string_view input) {
    if (input.empty()) {
        return {};
    }
    if (lmx::utf8::validate(input)) {
        return std::string(input);
    }

#ifdef _WIN32
    if (const std::string converted = acp_to_utf8(std::string(input)); !converted.empty()) {
        if (lmx::utf8::validate(converted)) {
            return converted;
        }
    }
#endif

    return std::string(input);
}

std::string read_file_utf8(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return normalize_to_utf8(oss.str());
}

void write_file_utf8(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw RuntimeError("could not open file for writing: " + path);
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}
}
