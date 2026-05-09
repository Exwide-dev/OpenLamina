#include "builtins.hpp"

#include <iostream>
#include <cmath>

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

    Value to_float(VM&, const std::vector<Value>& args) {
        arg_must("floatstring", 1);
        return Value(args[0].asNumber().toString());
    }

    Value math_sin(VM&, const std::vector<Value>& args) {
        arg_must("sin", 1);
        auto num = args[0].asNumber();
        long double val = static_cast<long double>(num.toInt64());
        return Value(static_cast<int64_t>(std::sin(val)));
    }

    Value math_cos(VM&, const std::vector<Value>& args) {
        arg_must("cos", 1);
        auto num = args[0].asNumber();
        long double val = static_cast<long double>(num.toInt64());
        return Value(static_cast<int64_t>(std::cos(val)));
    }

    Value math_tan(VM&, const std::vector<Value>& args) {
        arg_must("tan", 1);
        const auto num = args[0].asNumber();
        const long double val = static_cast<long double>(num.toInt64());
        return Value(static_cast<int64_t>(std::tan(val)));
    }

    irgen::ModuleObject math_mod = [] {
        std::flat_map<size_t, std::shared_ptr<irgen::Value>> math_symbols;
        
        math_symbols[irgen::g_string_pool.add("sin")] = std::make_shared<irgen::Value>(Value(irgen::FunctionType(math_sin)));
        math_symbols[irgen::g_string_pool.add("cos")] = std::make_shared<irgen::Value>(Value(irgen::FunctionType(math_cos)));
        math_symbols[irgen::g_string_pool.add("tan")] = std::make_shared<irgen::Value>(Value(irgen::FunctionType(math_tan)));
        math_symbols[irgen::g_string_pool.add("pi")] = std::make_shared<irgen::Value>(Value(3141592653));
        
        return irgen::ModuleObject(irgen::SymbolTable(math_symbols));
    }();

    irgen::ModuleObject standard_mod = [] {
        std::flat_map<size_t, std::shared_ptr<irgen::Value>> std_symbols;
        
        std_symbols[irgen::g_string_pool.add("concat")] = std::make_shared<irgen::Value>(irgen::Value(irgen::FunctionType(
            [](VM &, const std::vector<Value> &args) -> Value {
                arg_must("concat", 2);
                if (args[0].getType() != Value::Type::String
                 or args[1].getType() != Value::Type::String) {
                    throw RuntimeError("Not both string");
                }
                return Value(args[0].asString() + args[1].asString());
            }
        )));
        
        std_symbols[irgen::g_string_pool.add("math")] = std::make_shared<irgen::Value>(Value(std::make_shared<irgen::ModuleObject>(math_mod)));
        
        return irgen::ModuleObject(irgen::SymbolTable(std_symbols));
    }();
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