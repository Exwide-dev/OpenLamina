#pragma once
#include <functional>
#include <iostream>

#include "irgen/opcode.hpp"

namespace repl {
    class REPL {
    public:
        irgen::VM vm{};
        REPL() = default;
        ~REPL() = default;
        void exec_input(
            const std::function<std::string()>& input_func = []() -> std::string {
                std::string line;
                std::getline(std::cin, line);
                return line;
            }
        );
    };
}
