#include "builtins.hpp"

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>

#include "irgen/opcode.hpp"
#include "rational.hpp"

#define arg_must(funcname, num) \
    if ((args.empty() and num != 0) or args.size() != num) { \
        throw RuntimeError("Function " funcname " requires " \
        + std::to_string(num) + " arguments, got " \
        + std::to_string(args.size()));\
    }

#define arg_at_least(funcname, num) \
    if (args.size() < num) { \
        throw RuntimeError("Function " funcname " requires at least " \
        + std::to_string(num) + " arguments, got " \
        + std::to_string(args.size()));\
    }

namespace lang {
Value floatstring(VM&, const std::vector<Value>& args) {
    arg_at_least("floatstring", 1);

    size_t max_fraction_digits = 20;
    if (args.size() >= 2) {
        const lammp::Number prec = args[1].asNumber();
        if (prec.isNegative()) {
            throw RuntimeError("floatstring precision must be non-negative");
        }
        max_fraction_digits = static_cast<size_t>(prec.toUInt64());
    }

    const Value& val = args[0].deref();
    if (val.isRational()) {
        return Value(val.asRational().toDecimalString(max_fraction_digits));
    }
    if (val.isNumber()) {
        return Value(lammp::Rational(val.asNumber()).toDecimalString(max_fraction_digits));
    }
    throw RuntimeError("floatstring requires a number or rational");
}

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
    auto val = static_cast<long double>(num.toInt64());
    return Value(static_cast<int64_t>(std::sin(val)));
}

Value math_cos(VM&, const std::vector<Value>& args) {
    arg_must("cos", 1);
    auto num = args[0].asNumber();
    auto val = static_cast<long double>(num.toInt64());
    return Value(static_cast<int64_t>(std::cos(val)));
}

Value math_tan(VM&, const std::vector<Value>& args) {
    arg_must("tan", 1);
    const auto num = args[0].asNumber();
    const auto val = static_cast<long double>(num.toInt64());
    return Value(static_cast<int64_t>(std::tan(val)));
}

Value math_sqrt(VM&, const std::vector<Value>& args) {
    arg_must("sqrt", 1);
    auto num = args[0].asNumber();
    auto val = static_cast<long double>(num.toInt64());
    return Value(static_cast<int64_t>(std::sqrt(val)));
}

Value math_abs(VM&, const std::vector<Value>& args) {
    arg_must("abs", 1);
    auto num = args[0].asNumber();
    if (num.toInt64() < 0) {
        return Value(-num.toInt64());
    }
    return Value(num.toInt64());
}

Value len(VM&, const std::vector<Value>& args) {
    arg_must("len", 1);
    const auto& val = args[0].deref();
    if (val.isVector()) {
        return Value(static_cast<int64_t>(val.asVector().size()));
    } else if (val.isDictionary()) {
        return Value(static_cast<int64_t>(val.asDictionary().size()));
    } else if (val.isString()) {
        return Value(static_cast<int64_t>(val.asString().size()));
    }
    throw RuntimeError("len requires vector, dictionary, or string");
}

Value type_of(VM&, const std::vector<Value>& args) {
    arg_must("type", 1);
    return Value(args[0].deref().type_name());
}

Value str_of(VM&, const std::vector<Value>& args) {
    arg_must("str", 1);
    return Value(args[0].toString());
}

Value int_of(VM&, const std::vector<Value>& args) {
    arg_must("int", 1);
    const auto& val = args[0].deref();
    if (val.isString()) {
        return Value(std::stoll(val.asString()));
    } else if (val.isNumber()) {
        return Value(val.asNumber().toInt64());
    }
    throw RuntimeError("int requires string or number");
}

Value exit(VM&, const std::vector<Value>& args) {
    std::exit(0);
    return {};
}

Value help(VM&, const std::vector<Value>& args) {
    std::cout << "OpenLamina Programming Language\n";
    std::cout << "Built-in functions:\n";
    std::cout << "  print(...args) - Print values\n";
    std::cout << "  input(prompt) - Read input from user\n";
    std::cout << "  now() - Get current time\n";
    std::cout << "  len(obj) - Get length of vector/dict/string\n";
    std::cout << "  type(obj) - Get type name\n";
    std::cout << "  str(obj) - Convert to string\n";
    std::cout << "  int(obj) - Convert to integer\n";
    std::cout << "  exit() - Exit the program\n";
    std::cout << "  help() - Show this help\n";
    std::cout << "  copyright() - Show copyright info\n";
    std::cout << "Dictionary functions:\n";
    std::cout << "  dict() - Create empty dictionary\n";
    std::cout << "  keys(dict) - Get dictionary keys\n";
    std::cout << "  values(dict) - Get dictionary values\n";
    std::cout << "  items(dict) - Get dictionary items as vector of pairs\n";
    std::cout << "  get(dict, key, default) - Get value or default\n";
    std::cout << "Standard library:\n";
    std::cout << "  std.math.sin(x), std.math.cos(x), std.math.tan(x)\n";
    std::cout << "  std.math.sqrt(x), std.math.abs(x)\n";
    std::cout << "  std.concat(str1, str2)\n";
    std::cout << "Decorators (std.decos.*):\n";
    std::cout << "  memoize(func) - Cache function results\n";
    std::cout << "  timer(func) - Measure execution time\n";
    std::cout << "  debug(func) - Print call info and return value\n";
    std::cout << "  log(func) - Log function calls\n";
    std::cout << "  once(func) - Allow only one call\n";
    std::cout << "  retry(func, n) - Retry n times on failure\n";
    std::cout << "  validate(func, validator) - Validate args before call\n";
    std::cout << "  catch(func, handler) - Catch and handle exceptions\n";
    std::cout << "\nSyntax examples:\n";
    std::cout << "  let x = 42\n";
    std::cout << "  let arr = vec[1, 2, 3]\n";
    std::cout << "  let obj = {\"key\": value}\n";
    std::cout << "  func add(a, b) { return a + b }\n";
    std::cout << "  std.decos.debug func my_func(x) { return x * 2 }\n";
    std::cout << "  std.decos.timer do(x) { return x + 1 }\n";
    return {};
}

Value copyright(VM&, const std::vector<Value>& args) {
    std::cout << "OpenLamina Programming Language\n";
    std::cout << "Copyright (C) 2024 OpenLamina Project\n";
    std::cout << "All rights reserved.\n";
    return {};
}

Value dict_create(VM&, const std::vector<Value>& args) {
    if (args.size() % 2 != 0) {
        throw RuntimeError("dict requires even number of arguments (key-value pairs)");
    }
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> dict;
    for (size_t i = 0; i < args.size(); i += 2) {
        auto key = std::make_shared<Value>(args[i].deref());
        dict[key] = std::make_shared<Value>(args[i + 1]);
    }
    return Value(std::move(dict));
}

Value dict_keys(VM&, const std::vector<Value>& args) {
    arg_must("keys", 1);
    const auto& val = args[0].deref();
    if (!val.isDictionary()) {
        throw RuntimeError("keys requires a dictionary");
    }
    std::vector<std::shared_ptr<Value>> keys;
    for (const auto& key : val.asDictionary() | std::views::keys) {
        keys.push_back(key);
    }
    return Value(std::move(keys));
}

Value dict_values(VM&, const std::vector<Value>& args) {
    arg_must("values", 1);
    const auto& val = args[0].deref();
    if (!val.isDictionary()) {
        throw RuntimeError("values requires a dictionary");
    }
    std::vector<std::shared_ptr<Value>> values;
    for (const auto& value : val.asDictionary() | std::views::values) {
        values.push_back(value);
    }
    return Value(std::move(values));
}

Value dict_items(VM&, const std::vector<Value>& args) {
    arg_must("items", 1);
    const auto& val = args[0].deref();
    if (!val.isDictionary()) {
        throw RuntimeError("items requires a dictionary");
    }
    std::vector<std::shared_ptr<Value>> items;
    for (const auto& [key, value] : val.asDictionary()) {
        std::vector<std::shared_ptr<Value>> pair;
        pair.push_back(key);
        pair.push_back(value);
        items.push_back(std::make_shared<Value>(std::move(pair)));
    }
    return Value(std::move(items));
}

Value dict_get(VM&, const std::vector<Value>& args) {
    arg_at_least("get", 2);
    const auto& dict_val = args[0].deref();
    if (!dict_val.isDictionary()) {
        throw RuntimeError("get requires a dictionary as first argument");
    }
    auto key = std::make_shared<Value>(args[1].deref());
    const auto& dict = dict_val.asDictionary();
    auto it = dict.find(key);
    if (it != dict.end()) {
        return *it->second;
    }
    if (args.size() >= 3) {
        return args[2];
    }
    throw RuntimeError("Key not found: " + key->toString());
}

irgen::ModuleObject math_mod = [] {
    std::map<size_t, std::shared_ptr<irgen::Value>> math_symbols;

    math_symbols[irgen::g_string_pool.add("sin")] = std::make_shared<
        irgen::Value>(Value(irgen::FunctionType(math_sin)));
    math_symbols[irgen::g_string_pool.add("cos")] = std::make_shared<
        irgen::Value>(Value(irgen::FunctionType(math_cos)));
    math_symbols[irgen::g_string_pool.add("tan")] = std::make_shared<
        irgen::Value>(Value(irgen::FunctionType(math_tan)));
    math_symbols[irgen::g_string_pool.add("sqrt")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(math_sqrt))
    );
    math_symbols[irgen::g_string_pool.add("abs")] = std::make_shared<
        irgen::Value>(Value(irgen::FunctionType(math_abs)));
    math_symbols[irgen::g_string_pool.add("pi")] = std::make_shared<irgen::Value>(Value(3141592653));

    return irgen::ModuleObject(irgen::SymbolTable(math_symbols));
}();

irgen::ModuleObject dict_mod = [] {
    std::map<size_t, std::shared_ptr<irgen::Value>> dict_symbols;

    dict_symbols[irgen::g_string_pool.add("keys")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(dict_keys))
    );
    dict_symbols[irgen::g_string_pool.add("values")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(dict_values))
    );
    dict_symbols[irgen::g_string_pool.add("items")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(dict_items))
    );
    dict_symbols[irgen::g_string_pool.add("get")] = std::make_shared<
        irgen::Value>(Value(irgen::FunctionType(dict_get)));

    return irgen::ModuleObject(irgen::SymbolTable(dict_symbols));
}();

Value deco_memoize(VM& vm, const std::vector<Value>& args) {
    arg_must("memoize", 1);
    if (!args[0].isFunction()) {
        throw RuntimeError("memoize requires a function");
    }

    std::shared_ptr<irgen::FunctionObject> user_func;
    irgen::FunctionType builtin_func;
    bool is_user = args[0].isUserFunction();

    if (is_user) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }

    auto cache = std::make_shared<std::unordered_map<std::string, Value>>();

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user, cache](VM& vm, const std::vector<Value>& call_args) -> Value {
                std::string key;
                for (const auto& arg : call_args) {
                    key += arg.toString() + ",";
                }

                auto it = cache->find(key);
                if (it != cache->end()) {
                    return it->second;
                }

                Value result;
                if (is_user) {
                    result = user_func->call(vm, call_args);
                } else {
                    result = builtin_func(vm, call_args);
                }
                (*cache)[key] = result;
                return result;
            }
        )
    );
}

Value deco_timer(VM& vm, const std::vector<Value>& args) {
    arg_must("timer", 1);
    if (!args[0].isFunction()) {
        throw RuntimeError("timer requires a function");
    }

    std::shared_ptr<irgen::FunctionObject> user_func;
    irgen::FunctionType builtin_func;
    bool is_user = args[0].isUserFunction();
    std::string func_name = is_user ? args[0].asFunctionObject()->name : "<builtin>";

    if (is_user) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user, func_name](VM& vm, const std::vector<Value>& call_args) -> Value {
                clock_t start = clock();
                Value result = is_user ? user_func->call(vm, call_args) : builtin_func(vm, call_args);
                clock_t end = clock();
                double elapsed = static_cast<double>(end - start) / CLOCKS_PER_SEC;
                std::cout << "Function " << func_name << " took " << elapsed << " seconds\n";
                return result;
            }
        )
    );
}

Value deco_debug(VM& vm, const std::vector<Value>& args) {
    arg_must("debug", 1);
    if (!args[0].isFunction()) {
        throw RuntimeError("debug requires a function");
    }

    std::shared_ptr<irgen::FunctionObject> user_func;
    irgen::FunctionType builtin_func;
    bool is_user = args[0].isUserFunction();
    std::string func_name = is_user ? args[0].asFunctionObject()->name : "<builtin>";

    if (is_user) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user, func_name](VM& vm, const std::vector<Value>& call_args) -> Value {
                std::cout << "Calling " << func_name << "(";
                for (size_t i = 0; i < call_args.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << call_args[i].toString();
                }
                std::cout << ")\n";

                Value result = is_user ? user_func->call(vm, call_args) : builtin_func(vm, call_args);

                std::cout << "Returned: " << result.toString() << "\n";
                return result;
            }
        )
    );
}

Value deco_log(VM& vm, const std::vector<Value>& args) {
    arg_must("log", 1);
    if (!args[0].isFunction()) {
        throw RuntimeError("log requires a function");
    }

    std::shared_ptr<irgen::FunctionObject> user_func;
    irgen::FunctionType builtin_func;
    bool is_user = args[0].isUserFunction();
    std::string func_name = is_user ? args[0].asFunctionObject()->name : "<builtin>";

    if (is_user) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user, func_name](VM& vm, const std::vector<Value>& call_args) -> Value {
                std::cout << "[LOG] Calling " << func_name << "\n";
                Value result = is_user ? user_func->call(vm, call_args) : builtin_func(vm, call_args);
                std::cout << "[LOG] " << func_name << " completed\n";
                return result;
            }
        )
    );
}

Value deco_once(VM& vm, const std::vector<Value>& args) {
    arg_must("once", 1);
    if (!args[0].isFunction()) {
        throw RuntimeError("once requires a function");
    }

    std::shared_ptr<irgen::FunctionObject> user_func;
    irgen::FunctionType builtin_func;
    bool is_user = args[0].isUserFunction();
    std::string func_name = is_user ? args[0].asFunctionObject()->name : "<builtin>";

    if (is_user) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }

    auto called = std::make_shared<std::unordered_set<std::string>>();

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user, func_name, called](
        VM& vm,
        const std::vector<Value>& call_args
    ) -> Value {
                if (called->contains(func_name)) {
                    throw RuntimeError("Function " + func_name + " can only be called once");
                }
                called->insert(func_name);
                return is_user ? user_func->call(vm, call_args) : builtin_func(vm, call_args);
            }
        )
    );
}

Value deco_retry(VM& vm, const std::vector<Value>& args) {
    arg_must("retry", 2);
    if (!args[0].isFunction()) {
        throw RuntimeError("retry requires a function as first argument");
    }
    if (!args[1].isNumber()) {
        throw RuntimeError("retry requires a number as second argument");
    }

    std::shared_ptr<irgen::FunctionObject> user_func;
    irgen::FunctionType builtin_func;
    bool is_user = args[0].isUserFunction();
    [[maybe_unused]] int max_retries = static_cast<int>(args[1].asNumber().toInt64());

    if (is_user) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user, max_retries](VM& vm, const std::vector<Value>& call_args) -> Value {
                for (int i = 0; i < max_retries; ++i) {
                    try {
                        return is_user ? user_func->call(vm, call_args) : builtin_func(vm, call_args);
                    } catch ([[maybe_unused]] const std::exception& e) {
                        if (i == max_retries - 1) {
                            throw;
                        }
                    }
                }
                throw RuntimeError("Retry failed after " + std::to_string(max_retries) + " attempts");
            }
        )
    );
}

Value deco_validate(VM& vm, const std::vector<Value>& args) {
    arg_must("validate", 2);
    if (!args[0].isFunction()) {
        throw RuntimeError("validate requires a function as first argument");
    }
    if (!args[1].isFunction()) {
        throw RuntimeError("validate requires a validator function as second argument");
    }

    std::shared_ptr<irgen::FunctionObject> user_func, user_validator;
    irgen::FunctionType builtin_func, builtin_validator;
    bool is_user_func = args[0].isUserFunction();
    bool is_user_validator = args[1].isUserFunction();

    if (is_user_func) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }
    if (is_user_validator) {
        user_validator = args[1].asFunctionObject();
        if (!user_validator->owner_vm) {
            user_validator->owner_vm = &vm;
        }
    } else {
        builtin_validator = args[1].asFunction();
    }

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user_func, user_validator, builtin_validator, is_user_validator](
        VM& vm,
        const std::vector<Value>& call_args
    ) -> Value {
                Value result = is_user_validator
                                   ? user_validator->call(vm, call_args)
                                   : builtin_validator(vm, call_args);
                if (!result.isBool() || !result.asBool()) {
                    throw RuntimeError("Validation failed");
                }
                return is_user_func ? user_func->call(vm, call_args) : builtin_func(vm, call_args);
            }
        )
    );
}

Value deco_catch(VM& vm, const std::vector<Value>& args) {
    arg_must("catch", 2);
    if (!args[0].isFunction()) {
        throw RuntimeError("catch requires a function as first argument");
    }
    if (!args[1].isFunction()) {
        throw RuntimeError("catch requires a handler function as second argument");
    }

    std::shared_ptr<irgen::FunctionObject> user_func, user_handler;
    irgen::FunctionType builtin_func, builtin_handler;
    bool is_user_func = args[0].isUserFunction();
    bool is_user_handler = args[1].isUserFunction();

    if (is_user_func) {
        user_func = args[0].asFunctionObject();
        if (!user_func->owner_vm) {
            user_func->owner_vm = &vm;
        }
    } else {
        builtin_func = args[0].asFunction();
    }
    if (is_user_handler) {
        user_handler = args[1].asFunctionObject();
        if (!user_handler->owner_vm) {
            user_handler->owner_vm = &vm;
        }
    } else {
        builtin_handler = args[1].asFunction();
    }

    return Value(
        irgen::FunctionType(
            [user_func, builtin_func, is_user_func, user_handler, builtin_handler, is_user_handler](
        VM& vm,
        const std::vector<Value>& call_args
    ) -> Value {
                try {
                    return is_user_func ? user_func->call(vm, call_args) : builtin_func(vm, call_args);
                } catch (const std::exception& e) {
                    std::vector<Value> handler_args;
                    handler_args.emplace_back(std::string(e.what()));
                    return is_user_handler ? user_handler->call(vm, handler_args) : builtin_handler(vm, handler_args);
                }
            }
        )
    );
}

irgen::ModuleObject decos_mod = [] {
    std::map<size_t, std::shared_ptr<irgen::Value>> decos_symbols;

    decos_symbols[irgen::g_string_pool.add("memoize")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_memoize))
    );
    decos_symbols[irgen::g_string_pool.add("timer")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_timer))
    );
    decos_symbols[irgen::g_string_pool.add("debug")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_debug))
    );
    decos_symbols[irgen::g_string_pool.add("log")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_log))
    );
    decos_symbols[irgen::g_string_pool.add("once")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_once))
    );
    decos_symbols[irgen::g_string_pool.add("retry")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_retry))
    );
    decos_symbols[irgen::g_string_pool.add("validate")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_validate))
    );
    decos_symbols[irgen::g_string_pool.add("catch")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(deco_catch))
    );

    return irgen::ModuleObject(irgen::SymbolTable(decos_symbols));
}();

irgen::ModuleObject standard_mod = [] {
    std::map<size_t, std::shared_ptr<irgen::Value>> std_symbols;

    std_symbols[irgen::g_string_pool.add("concat")] = std::make_shared<irgen::Value>(
        irgen::Value(
            irgen::FunctionType(
                [](VM&, const std::vector<Value>& args) -> Value {
                    arg_must("concat", 2);
                    if (args[0].getType() != Value::Type::String
                        or args[1]
                    .
                    getType() != Value::Type::String
                    )
                    {
                        throw RuntimeError("Not both string");
                    }
                    return Value(args[0].asString() + args[1].asString());
                }
            )
        )
    );

    std_symbols[irgen::g_string_pool.add("math")] = std::make_shared<irgen::Value>(
        Value(std::make_shared<irgen::ModuleObject>(math_mod))
    );
    std_symbols[irgen::g_string_pool.add("dict")] = std::make_shared<irgen::Value>(
        Value(std::make_shared<irgen::ModuleObject>(dict_mod))
    );
    std_symbols[irgen::g_string_pool.add("decos")] = std::make_shared<irgen::Value>(
        Value(std::make_shared<irgen::ModuleObject>(decos_mod))
    );

    return irgen::ModuleObject(irgen::SymbolTable(std_symbols));
}();

Value rational(VM&, const std::vector<Value>& args) {
    arg_at_least("rational", 1);
    if (args.size() == 1) {
        return Value(lammp::Rational(args[0].asNumber()));
    } else {
        return Value(lammp::Rational(args[0].asNumber(), args[1].asNumber()));
    }
}
}

#undef arg_must
#undef arg_at_least

void lang::init_builtins(irgen::SymbolTable& symbols) {
    symbols.set(irgen::g_string_pool.add("true"), Value(true));
    symbols.set(irgen::g_string_pool.add("false"), Value(false));
    symbols.set(irgen::g_string_pool.add("input"), Value(irgen::FunctionType(input)));
    symbols.set(irgen::g_string_pool.add("print"), Value(irgen::FunctionType(print)));
    symbols.set(irgen::g_string_pool.add("rational"), Value(irgen::FunctionType(rational)));
    symbols.set(irgen::g_string_pool.add("now"), Value(irgen::FunctionType(now)));
    symbols.set(irgen::g_string_pool.add("floatstring"), Value(irgen::FunctionType(floatstring)));
    symbols.set(irgen::g_string_pool.add("len"), Value(irgen::FunctionType(len)));
    symbols.set(irgen::g_string_pool.add("type"), Value(irgen::FunctionType(type_of)));
    symbols.set(irgen::g_string_pool.add("str"), Value(irgen::FunctionType(str_of)));
    symbols.set(irgen::g_string_pool.add("int"), Value(irgen::FunctionType(int_of)));
    symbols.set(irgen::g_string_pool.add("exit"), Value(irgen::FunctionType(exit)));
    symbols.set(irgen::g_string_pool.add("help"), Value(irgen::FunctionType(help)));
    symbols.set(irgen::g_string_pool.add("copyright"), Value(irgen::FunctionType(copyright)));
    symbols.set(irgen::g_string_pool.add("dict"), Value(irgen::FunctionType(dict_create)));
}