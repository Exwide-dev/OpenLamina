#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace lmx::utf8 {
struct DecodeResult {
    char32_t codepoint = 0;
    size_t bytes = 0;
    bool ok = false;
};

[[nodiscard]] inline bool is_continuation(const unsigned char b) {
    return (b & 0xC0) == 0x80;
}

[[nodiscard]] inline DecodeResult decode(std::string_view text, const size_t index) {
    DecodeResult result;
    if (index >= text.size()) {
        return result;
    }

    const unsigned char b0 = static_cast<unsigned char>(text[index]);
    if (b0 < 0x80) {
        result.codepoint = b0;
        result.bytes = 1;
        result.ok = true;
        return result;
    }

    size_t need = 0;
    if ((b0 & 0xE0) == 0xC0) {
        need = 2;
    } else if ((b0 & 0xF0) == 0xE0) {
        need = 3;
    } else if ((b0 & 0xF8) == 0xF0) {
        need = 4;
    } else {
        return result;
    }

    if (index + need > text.size()) {
        return result;
    }

    for (size_t i = 1; i < need; ++i) {
        if (!is_continuation(static_cast<unsigned char>(text[index + i]))) {
            return result;
        }
    }

    char32_t cp = 0;
    if (need == 2) {
        cp = ((b0 & 0x1F) << 6) |
             (static_cast<unsigned char>(text[index + 1]) & 0x3F);
        if (cp < 0x80) {
            return result;
        }
    } else if (need == 3) {
        cp = ((b0 & 0x0F) << 12) |
             ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
             (static_cast<unsigned char>(text[index + 2]) & 0x3F);
        if (cp < 0x800) {
            return result;
        }
    } else {
        cp = ((static_cast<char32_t>(b0) & 0x07) << 18) |
             ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12) |
             ((static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6) |
             (static_cast<unsigned char>(text[index + 3]) & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) {
            return result;
        }
    }

    result.codepoint = cp;
    result.bytes = need;
    result.ok = true;
    return result;
}

[[nodiscard]] inline bool is_ascii_letter(const char32_t cp) {
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
}

[[nodiscard]] inline bool is_ascii_digit(const char32_t cp) {
    return cp >= '0' && cp <= '9';
}

/// 近似 Unicode XID_Start：ASCII 字母、下划线及常见脚本书写字母
[[nodiscard]] inline bool is_identifier_start(const char32_t cp) {
    if (cp == '_') {
        return true;
    }
    if (is_ascii_letter(cp)) {
        return true;
    }
    if (cp >= 0x00C0 && cp <= 0x00D6) return true;
    if (cp >= 0x00D8 && cp <= 0x00F6) return true;
    if (cp >= 0x00F8 && cp <= 0x02FF) return true;
    if (cp >= 0x0370 && cp <= 0x037D) return true;
    if (cp >= 0x037F && cp <= 0x1FFF) return true;
    if (cp >= 0x200C && cp <= 0x200D) return true;
    if (cp >= 0x2070 && cp <= 0x218F) return true;
    if (cp >= 0x2C00 && cp <= 0x2FEF) return true;
    if (cp >= 0x3001 && cp <= 0xD7FF) return true;
    if (cp >= 0xF900 && cp <= 0xFDCF) return true;
    if (cp >= 0xFDF0 && cp <= 0xFFFD) return true;
    if (cp >= 0x10000 && cp <= 0xEFFFF) return true;
    return false;
}

/// 近似 Unicode XID_Continue
[[nodiscard]] inline bool is_identifier_continue(const char32_t cp) {
    if (is_identifier_start(cp)) {
        return true;
    }
    if (is_ascii_digit(cp)) {
        return true;
    }
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return true;
    if (cp >= 0x20D0 && cp <= 0x20FF) return true;
    if (cp >= 0xFE20 && cp <= 0xFE2F) return true;
    return false;
}

[[nodiscard]] inline bool identifier_start_at(std::string_view text, const size_t index) {
    const DecodeResult r = decode(text, index);
    return r.ok && is_identifier_start(r.codepoint);
}

[[nodiscard]] inline bool identifier_continue_at(std::string_view text, const size_t index) {
    const DecodeResult r = decode(text, index);
    return r.ok && is_identifier_continue(r.codepoint);
}

[[nodiscard]] inline bool ascii_digit_at(std::string_view text, const size_t index) {
    const DecodeResult r = decode(text, index);
    return r.ok && is_ascii_digit(r.codepoint);
}

[[nodiscard]] inline size_t codepoint_length(std::string_view text, const size_t index) {
    const DecodeResult r = decode(text, index);
    return r.ok ? r.bytes : 1;
}

[[nodiscard]] inline bool validate(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        const DecodeResult r = decode(text, i);
        if (!r.ok) {
            return false;
        }
        i += r.bytes;
    }
    return true;
}

[[nodiscard]] inline bool has_bom(std::string_view text) {
    return text.size() >= 3 &&
           static_cast<unsigned char>(text[0]) == 0xEF &&
           static_cast<unsigned char>(text[1]) == 0xBB &&
           static_cast<unsigned char>(text[2]) == 0xBF;
}

[[nodiscard]] inline size_t bom_length(std::string_view text) {
    return has_bom(text) ? 3 : 0;
}
}
