#include "builtins.hpp"

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>

#include "irgen/opcode.hpp"
#include "irgen/struct_types.hpp"
#include "irgen/iterator_ops.hpp"
#include "irgen/macro_ops.hpp"
#include "irgen/runtime_ast.hpp"
#include "rational.hpp"
#include "std_modules.hpp"

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
namespace {

using lammp::Number;
using lammp::Rational;

long double math_arg_as_long_double(const Value& arg) {
    const Value& val = arg.deref();
    if (val.isRational()) {
        return val.asRational().toLongDouble();
    }
    if (val.isNumber()) {
        return std::stold(val.asNumber().toString());
    }
    throw RuntimeError("math functions require a number or rational argument");
}

Value math_unary_real(const long double arg, long double (*fn)(long double)) {
    return Value(Rational::fromDouble(fn(arg)));
}

} // namespace

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
        std::cout << elem.deref().printString() << " ";
    }
    std::cout << std::endl;
    return {};
}

Value input(VM& vm, const std::vector<Value>& args) {
    arg_must("input", 1);
    std::string input;
    std::cout << args[0].deref().printString();
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
    return math_unary_real(math_arg_as_long_double(args[0]), std::sin);
}

Value math_cos(VM&, const std::vector<Value>& args) {
    arg_must("cos", 1);
    return math_unary_real(math_arg_as_long_double(args[0]), std::cos);
}

Value math_tan(VM&, const std::vector<Value>& args) {
    arg_must("tan", 1);
    return math_unary_real(math_arg_as_long_double(args[0]), std::tan);
}

Value math_sqrt(VM&, const std::vector<Value>& args) {
    arg_must("sqrt", 1);
    return math_unary_real(math_arg_as_long_double(args[0]), std::sqrt);
}

Value math_abs(VM&, const std::vector<Value>& args) {
    arg_must("abs", 1);
    const Value& val = args[0].deref();
    if (val.isRational()) {
        const Rational& r = val.asRational();
        return r.numerator().isNegative() ? Value(-r) : Value(r);
    }
    if (val.isNumber()) {
        const Number& n = val.asNumber();
        return n.isNegative() ? Value(Rational(-n)) : Value(Rational(n));
    }
    throw RuntimeError("abs requires a number or rational");
}

Value math_range(VM& vm, const std::vector<Value>& args) {
    if (args.empty() || args.size() > 3) {
        throw RuntimeError("range requires 1 ~ 3 arguments");
    }

    auto to_int = [](const Value& v) -> Number {
        const Value& val = v.deref();
        if (val.isNumber()) {
            return val.asNumber();
        }
        if (val.isRational() && val.asRational().denominator() == Number(1)) {
            return val.asRational().numerator();
        }
        throw RuntimeError("range arguments must be integers");
    };

    Number start(0);
    Number stop(0);
    Number step(1);

    switch (args.size()) {
        case 1:
            stop = to_int(args[0]);
            break;
        case 2:
            start = to_int(args[0]);
            stop = to_int(args[1]);
            break;
        case 3:
            start = to_int(args[0]);
            stop = to_int(args[1]);
            step = to_int(args[2]);
            if (step.isZero()) {
                throw RuntimeError("range() arg 3 must not be zero");
            }
            break;
        default:
            break;
    }

    std::vector<std::shared_ptr<Value>> result;
    const irgen::VmGcSuppress gc_guard{vm};
    if (step > Number(0)) {
        for (Number current = start; current < stop; current += step) {
            result.push_back(vm.cell_pool.allocateValue(Value(current)));
        }
    } else {
        for (Number current = start; current > stop; current += step) {
            result.push_back(vm.cell_pool.allocateValue(Value(current)));
        }
    }

    return Value(std::move(result));
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
    return Value(args[0].deref().printString());
}

Value int_of(VM&, const std::vector<Value>& args) {
    arg_must("int", 1);
    const auto& val = args[0].deref();
    if (val.isString()) {
        try {
            return Value(lang::lammp::Number(val.asString()));
        } catch (const std::exception&) {
            throw RuntimeError(std::format("cannot convert \"{}\" to int", val.asString()));
        }
    } else if (val.isNumber()) {
        return Value(val.asNumber().toInt64());
    }
    throw RuntimeError("int requires string or number");
}

Value convert_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("convert", 2);
    return irgen::convert_to_type(vm, args[0], args[1]);
}

Value iter_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("iter", 1);
    return irgen::make_iter(vm, args[0]);
}

Value next_builtin(VM& vm, const std::vector<Value>& args) {
    arg_at_least("next", 1);
    if (args.size() >= 2) {
        return irgen::iterator_next(vm, args[0], args[1]);
    }
    return irgen::iterator_next(vm, args[0]);
}

Value exit(VM&, const std::vector<Value>& args) {
    std::exit(0);
}

Value gc(VM& vm, const std::vector<Value>& args) {
    (void)args;
    const size_t before = vm.cell_pool.liveCells();
    vm.collectGarbage();
    const size_t after = vm.cell_pool.liveCells();
    return Value(static_cast<int64_t>(before > after ? before - after : 0));
}

Value help(VM&, const std::vector<Value>& args) {
    std::cout << "OpenLamina Programming Language\n";
    std::cout << "Built-in functions:\n";
    std::cout << "  print(...args) - Print values\n";
    std::cout << "  input(prompt) - Read input from user\n";
    std::cout << "  now() - Get current time\n";
    std::cout << "  len(obj) - Get length of vector/dict/string\n";
    std::cout << "  type(obj) - Get type name\n";
    std::cout << "  str(obj) - Convert to string (uses printString)\n";
    std::cout << "  int(obj) - Convert to integer\n";
    std::cout << "  convert(type, obj) - Convert obj via type.__convert__ (most specific match)\n";
    std::cout << "  iter(obj) - Create iterator from iterable\n";
    std::cout << "  next(iter[, default]) - Advance iterator (__next__ protocol)\n";
    std::cout << "  exit() - Exit the program\n";
    std::cout << "  gc() - Run mark-sweep GC on object pool; returns cells reclaimed\n";
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
    std::cout << "  std.math.sqrt(x), std.math.abs(x), std.math.range(...)\n";
    std::cout << "  std.concat(str1, str2)\n";
    std::cout << "  std.iter.iter(x), std.iter.next(it), std.iter.enumerate(x)\n";
    std::cout << "  std.iter.chain(a, b), std.iter.to_list(x)\n";
    std::cout << "  std.io.read_line(), std.io.read_file(path), std.io.write_file(path, text)\n";
    std::cout << "  std.io.write_line(...), std.io.eprint(...)\n";
    std::cout << "Decorators (std.decos.*):\n";
    std::cout << "  memoize(func) - Cache function results\n";
    std::cout << "  timer(func) - Measure execution time\n";
    std::cout << "  debug(func) - Print call info and return value\n";
    std::cout << "  log(func) - Log function calls\n";
    std::cout << "  once(func) - Allow only one call\n";
    std::cout << "  retry(func, n) - Retry n times on failure\n";
    std::cout << "  validate(func, validator) - Validate args before call\n";
    std::cout << "  catch(func, handler) - Catch and handle exceptions\n";
    std::cout << "\nBuiltin exception types (BaseException hierarchy):\n";
    std::cout << "  BaseException, Exception, StopIteration, RuntimeError, ValueError, TypeError\n";
    std::cout << "  throw only accepts instances of BaseException subclasses\n";
    std::cout << "\nException syntax:\n";
    std::cout << "  throw ValueError(\"msg\")\n";
    std::cout << "  try { ... } catch (e: ValueError) { ... } else { ... }\n";
    std::cout << "  catch (e) / catch (e: ...) / catch (...) — broad handlers\n";
    std::cout << "  for-in digests StopIteration from iterators (not caught by outer try)\n";
    std::cout << "\nFriend func (multi-dispatch via __dispatch__):\n";
    std::cout << "  friend func f(x: num) { return x + 1 }\n";
    std::cout << "  f.__dispatch__.append(do(x: text) { return x + \"!\" })\n";
    std::cout << "  friend func placeholder   // no body; append handlers later\n";
    std::cout << "\nSyntax examples:\n";
    std::cout << "  let x = 42\n";
    std::cout << "  let arr = vec[1, 2, 3]\n";
    std::cout << "  let a = []\n";
    std::cout << "  a.append(1)\n";
    std::cout << "  let obj = {\"key\": value}\n";
    std::cout << "\nType methods (call on mutable lvalue):\n";
    std::cout << "  all values: displayString(), printString()\n";
    std::cout << "  vector: append, extend, pop, clear, len, contains, reverse\n";
    std::cout << "  dict: get, set, pop, clear, len, contains/has, keys, values, update\n";
    std::cout << "  string: upper, lower, strip, split, contains, startswith, endswith, len\n";
    std::cout << "  number: abs\n";
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

Value dict_create(VM& vm, const std::vector<Value>& args) {
    if (args.size() % 2 != 0) {
        throw RuntimeError("dict requires even number of arguments (key-value pairs)");
    }
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> dict;
    const irgen::VmGcSuppress gc_guard{vm};
    for (size_t i = 0; i < args.size(); i += 2) {
        auto key = vm.cell_pool.allocateCopy(args[i].deref());
        dict[key] = vm.cell_pool.allocateCopy(args[i + 1]);
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

Value dict_items(VM& vm, const std::vector<Value>& args) {
    arg_must("items", 1);
    const auto& val = args[0].deref();
    if (!val.isDictionary()) {
        throw RuntimeError("items requires a dictionary");
    }
    std::vector<std::shared_ptr<Value>> items;
    const irgen::VmGcSuppress gc_guard{vm};
    for (const auto& [key, value] : val.asDictionary()) {
        std::vector<std::shared_ptr<Value>> pair;
        pair.push_back(key);
        pair.push_back(value);
        items.push_back(vm.cell_pool.allocateValue(Value(std::move(pair))));
    }
    return Value(std::move(items));
}

Value dict_get(VM& vm, const std::vector<Value>& args) {
    arg_at_least("get", 2);
    const auto& dict_val = args[0].deref();
    if (!dict_val.isDictionary()) {
        throw RuntimeError("get requires a dictionary as first argument");
    }
    auto key = vm.cell_pool.allocateCopy(args[1].deref());
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
    math_symbols[irgen::g_string_pool.add("range")] = std::make_shared<irgen::Value>(
        Value(irgen::FunctionType(math_range))
    );
    math_symbols[irgen::g_string_pool.add("pi")] = std::make_shared<irgen::Value>(
        Value(Rational::fromDouble(3.14159265358979323846L))
    );

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
    std_symbols[irgen::g_string_pool.add("iter")] = std::make_shared<irgen::Value>(
        Value(std::make_shared<irgen::ModuleObject>(make_iter_module()))
    );
    std_symbols[irgen::g_string_pool.add("io")] = std::make_shared<irgen::Value>(
        Value(std::make_shared<irgen::ModuleObject>(make_io_module()))
    );

    return irgen::ModuleObject(irgen::SymbolTable(std_symbols));
}();

Value eval_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("eval", 1);
    if (!irgen::value_is_ast(args[0])) {
        throw RuntimeError("eval expects an AST value");
    }
    return irgen::eval_ast_value(vm, irgen::value_as_ast(args[0]));
}

Value quote_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("quote", 3);
    if (args[0].deref().getType() != Value::Type::Vector) {
        throw RuntimeError("quote expects a vec of hygienic names");
    }
    if (args[1].deref().getType() != Value::Type::Vector) {
        throw RuntimeError("quote expects a vec of binding expressions");
    }
    if (!irgen::value_is_ast(args[2])) {
        throw RuntimeError("quote expects an AST body");
    }

    std::vector<std::string> hygienic;
    for (const auto& elem : args[0].deref().asVector()) {
        hygienic.push_back(elem->deref().asString());
    }

    std::vector<std::pair<std::string, irgen::RuntimeAstNode>> captured;
    for (const auto& elem : args[1].deref().asVector()) {
        const irgen::RuntimeAstNode bind_expr = irgen::value_as_ast(*elem);
        const std::string name = irgen::binding_var_name_for_quote(bind_expr);
        const Value bound = irgen::capture_quote_binding_value(vm, bind_expr);
        captured.emplace_back(name, irgen::value_to_quote_binding_ast(bound));
    }

    irgen::RuntimeAstNode body = irgen::value_as_ast(args[2]);
    return irgen::make_ast_value(irgen::quote_ast(hygienic, captured, std::move(body)));
}

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

void lang::init_builtins(irgen::SymbolTable& symbols, irgen::CellPool& pool) {
    symbols.set(irgen::g_string_pool.add("true"), Value(true), pool);
    symbols.set(irgen::g_string_pool.add("false"), Value(false), pool);
    symbols.set(irgen::g_string_pool.add("input"), Value(irgen::FunctionType(input)), pool);
    symbols.set(irgen::g_string_pool.add("print"), Value(irgen::FunctionType(print)), pool);
    symbols.set(irgen::g_string_pool.add("rational"), Value(irgen::FunctionType(rational)), pool);
    symbols.set(irgen::g_string_pool.add("now"), Value(irgen::FunctionType(now)), pool);
    symbols.set(irgen::g_string_pool.add("floatstring"), Value(irgen::FunctionType(floatstring)), pool);
    symbols.set(irgen::g_string_pool.add("len"), Value(irgen::FunctionType(len)), pool);
    symbols.set(irgen::g_string_pool.add("type"), Value(irgen::FunctionType(type_of)), pool);
    symbols.set(irgen::g_string_pool.add("str"), Value(irgen::FunctionType(str_of)), pool);
    symbols.set(irgen::g_string_pool.add("int"), Value(irgen::FunctionType(int_of)), pool);
    symbols.set(irgen::g_string_pool.add("convert"), Value(irgen::FunctionType(convert_fn)), pool);
    symbols.set(irgen::g_string_pool.add("iter"), Value(irgen::FunctionType(iter_fn)), pool);
    symbols.set(irgen::g_string_pool.add("next"), Value(irgen::FunctionType(next_builtin)), pool);
    symbols.set(irgen::g_string_pool.add("exit"), Value(irgen::FunctionType(exit)), pool);
    symbols.set(irgen::g_string_pool.add("gc"), Value(irgen::FunctionType(gc)), pool);
    symbols.set(irgen::g_string_pool.add("help"), Value(irgen::FunctionType(help)), pool);
    symbols.set(irgen::g_string_pool.add("copyright"), Value(irgen::FunctionType(copyright)), pool);
    symbols.set(irgen::g_string_pool.add("dict"), Value(irgen::FunctionType(dict_create)), pool);
    symbols.set(irgen::g_string_pool.add("eval"), Value(irgen::FunctionType(eval_fn)), pool);
    symbols.set(irgen::g_string_pool.add("quote"), Value(irgen::FunctionType(quote_fn)), pool);
}