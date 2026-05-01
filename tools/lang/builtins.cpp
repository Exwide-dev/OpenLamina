#include "builtins.hpp"

#include <iostream>

#include "irgen/opcode.hpp"

#define arg_must(funcname, num) \
    if ((args.empty() and num != 0) or args.size() != num) { \
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

    Value now(VM& vm, const std::vector<Value>& args) {
        arg_must("now", 0);
        return Value(clock()) / Value(CLOCKS_PER_SEC);
    }

    Value to_float(VM& vm, const std::vector<Value>& args) {
        arg_must("floatstring", 1);
        return Value(std::to_string(
            static_cast<long double>(args[0].asNumber().getNumerator())
          / static_cast<long double>(args[0].asNumber().getDenominator())
        ));
    }
}

#undef arg_must

void lang::init_builtins(irgen::SymbolTable& symbols) {
    symbols.set(irgen::g_string_pool.add("true"), Value(true));
    symbols.set(irgen::g_string_pool.add("false"), Value(false));
    symbols.set(irgen::g_string_pool.add("input"), Value(irgen::FunctionType(input)));
    symbols.set(irgen::g_string_pool.add("print"), Value(irgen::FunctionType(print)));
    symbols.set(irgen::g_string_pool.add("now"), Value(irgen::FunctionType(now)));
    symbols.set(irgen::g_string_pool.add("floatstring"), Value(irgen::FunctionType(to_float)));
}