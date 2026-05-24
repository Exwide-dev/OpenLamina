#pragma once
#include <functional>
#include <iostream>
#include <stack>
#include <string>

#include "irgen/opcode.hpp"

namespace repl {
    class REPL {
    public:
        irgen::VM vm{};
        REPL() = default;
        ~REPL() = default;

        struct ExecResult {
            bool success;
            bool needs_more_input;
        };

        ExecResult exec_input(
            const std::function<std::string()> &input_func = []() -> std::string {
                std::string line;
                std::getline(std::cin, line);
                return line;
            }
        );

    private:
        enum class BraceType {
            NONE,
            PAREN,     // ()
            BRACKET,   // []
            BRACE_FUNC,// {} for function body
            BRACE_DICT // {} for dictionary
        };

        std::stack<BraceType> brace_stack;
        std::string pending_input;
        bool in_string = false;
        char string_delimiter = '"';

        void update_state(const std::string& line);
        bool needs_more_input() const;
        void reset_state();
    };
}
