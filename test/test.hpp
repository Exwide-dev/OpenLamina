#pragma once

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string_view>

namespace test {

inline int failures = 0;
inline int ran = 0;

inline void check(const bool ok, const std::string_view expr,
                  const std::string_view file, const int line) {
    if (!ok) {
        std::cerr << "  ASSERT failed: " << expr
                  << " (" << file << ":" << line << ")\n";
        ++failures;
    }
}

#define ASSERT(expr) \
    do { \
        test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__); \
    } while (0)

inline void run(const std::string_view name, const std::function<void()>& body) {
    ++ran;
    const int failures_before = failures;
    std::cerr << "[ RUN ] " << name << '\n';
    try {
        body();
        if (failures == failures_before) {
            std::cerr << "[  OK ] " << name << '\n';
        } else {
            std::cerr << "[ FAIL ] " << name << " (assertions failed)\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[ FAIL ] " << name << ": " << e.what() << '\n';
        ++failures;
    }
}

[[nodiscard]] inline int summary() {
    std::cerr << "---\n" << ran << " test(s), " << failures << " failure(s)\n";
    const int exit_code = failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    std::cerr << "Exit: " << exit_code << std::endl;
    return exit_code;
}

} // namespace test
