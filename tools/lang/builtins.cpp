#include "builtins.hpp"

#include <iostream>

#include "irgen/opcode.hpp"

#define arg_must(funcname, num) \
    if (args.size() != num) { \
        throw RuntimeError("Function " funcname " requires " \
        + std::to_string(num) + " arguments, got " \
        + std::to_string(args.size()));\
    }

namespace lang {
    Value print(VM& vm, const std::vector<Value>& args) {
        for (auto const& elem : args) {
            std::cout << elem << " ";
        }
        std::cout << std::endl;
        return {};
    }

    Value input(VM& vm, const std::vector<Value>& args) {
        arg_must("input", 1);
        std::string input;
        std::cout << args[0];
        std::getline(std::cin, input);
        return Value(input);
    }
}

#undef arg_must

void lang::init_builtins(irgen::SymbolTable& symbols) {
    symbols.set("true", Value(true));
    symbols.set("false", Value(false));
    symbols.set("input", Value(irgen::FunctionType(input)));
    symbols.set("print", Value(irgen::FunctionType(print)));
}