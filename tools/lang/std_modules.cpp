#include "std_modules.hpp"

#include "builtins.hpp"

#include "irgen/typing.hpp"

#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "irgen/iterator_ops.hpp"
#include "../tools/error.hpp"
#include "../utf8_io.hpp"

namespace lang {

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

Value enumerate_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("enumerate", 1);
    const Value iter = irgen::make_iter(vm, args[0]);
    std::vector<std::shared_ptr<Value>> pairs;
    int64_t index = 0;

    while (true) {
        Value item;
        if (!irgen::iterator_advance(vm, iter, item)) {
            break;
        }
        pairs.push_back(vm.cell_pool.allocateCopy(
            Value(std::vector<std::shared_ptr<Value>>{
                vm.cell_pool.allocateCopy(Value(index)),
                vm.cell_pool.allocateCopy(item.deref()),
            })
        ));
        ++index;
    }

    return Value(std::move(pairs));
}

Value chain_fn(VM& vm, const std::vector<Value>& args) {
    arg_at_least("chain", 1);
    std::vector<std::shared_ptr<Value>> merged;
    for (const Value& arg : args) {
        const Value iter = irgen::make_iter(vm, arg);
        while (true) {
            Value item;
            if (!irgen::iterator_advance(vm, iter, item)) {
                break;
            }
            merged.push_back(vm.cell_pool.allocateCopy(item.deref()));
        }
    }
    return Value(std::move(merged));
}

Value to_list_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("to_list", 1);
    const Value iter = irgen::make_iter(vm, args[0]);
    std::vector<std::shared_ptr<Value>> items;
    while (true) {
        Value item;
        if (!irgen::iterator_advance(vm, iter, item)) {
            break;
        }
        items.push_back(vm.cell_pool.allocateCopy(item.deref()));
    }
    return Value(std::move(items));
}

Value read_line_fn(VM&, const std::vector<Value>& args) {
    (void)args;
    return Value(lm::utf8_io::read_line_utf8());
}

Value read_file_fn(VM&, const std::vector<Value>& args) {
    arg_must("read_file", 1);
    const Value& path_val = args[0].deref();
    if (!path_val.isString()) {
        throw RuntimeError("read_file requires a string path");
    }
    return Value(lm::utf8_io::read_file_utf8(path_val.asString()));
}

Value write_file_fn(VM&, const std::vector<Value>& args) {
    arg_must("write_file", 2);
    const Value& path_val = args[0].deref();
    const Value& content_val = args[1].deref();
    if (!path_val.isString()) {
        throw RuntimeError("write_file path must be text");
    }
    lm::utf8_io::write_file_utf8(path_val.asString(), content_val.printString());
    return {};
}

Value write_line_fn(VM&, const std::vector<Value>& args) {
    for (const Value& arg : args) {
        std::cout << arg.deref().printString();
    }
    std::cout << std::endl;
    return {};
}

Value eprint_fn(VM&, const std::vector<Value>& args) {
    for (const Value& arg : args) {
        std::cerr << arg.deref().printString() << ' ';
    }
    std::cerr << std::endl;
    return {};
}

namespace {

std::string format_template(const std::string& tmpl, const std::vector<Value>& args) {
    std::string result;
    result.reserve(tmpl.size());
    size_t arg_idx = 0;

    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (i + 1 < tmpl.size() && tmpl[i] == '{' && tmpl[i + 1] == '}') {
            if (arg_idx >= args.size()) {
                throw RuntimeError(
                    std::format("format: not enough arguments for template (need at least {})", arg_idx + 1)
                );
            }
            result += args[arg_idx].deref().printString();
            ++arg_idx;
            ++i;
        } else {
            result += tmpl[i];
        }
    }

    if (arg_idx < args.size()) {
        throw RuntimeError(
            std::format("format: {} unused argument(s)", args.size() - arg_idx)
        );
    }

    return result;
}

} // namespace

Value format_fn(VM&, const std::vector<Value>& args) {
    arg_at_least("format", 1);
    const Value& tmpl_val = args[0].deref();
    if (!tmpl_val.isString()) {
        throw RuntimeError("format template must be text");
    }
    std::vector<Value> rest;
    rest.reserve(args.size() - 1);
    for (size_t i = 1; i < args.size(); ++i) {
        rest.push_back(args[i]);
    }
    return Value(format_template(tmpl_val.asString(), rest));
}

Value join_fn(VM& vm, const std::vector<Value>& args) {
    arg_must("join", 2);
    const Value& sep_val = args[0].deref();
    if (!sep_val.isString()) {
        throw RuntimeError("join separator must be text");
    }
    const std::string& sep = sep_val.asString();
    const Value iter = irgen::make_iter(vm, args[1]);
    std::string result;
    bool first = true;
    while (true) {
        Value item;
        if (!irgen::iterator_advance(vm, iter, item)) {
            break;
        }
        if (!first) {
            result += sep;
        }
        first = false;
        result += item.deref().printString();
    }
    return Value(std::move(result));
}

namespace {

std::mt19937_64& random_engine() {
    static std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

} // namespace

Value randint_fn(VM&, const std::vector<Value>& args) {
    arg_must("randint", 2);
    const Value& lo_val = args[0].deref();
    const Value& hi_val = args[1].deref();
    if (!lo_val.isNumber() || !hi_val.isNumber()) {
        throw RuntimeError("randint requires integer bounds");
    }
    const int64_t lo = lo_val.asInt();
    const int64_t hi = hi_val.asInt();
    if (lo > hi) {
        throw RuntimeError("randint lower bound must not exceed upper bound");
    }
    std::uniform_int_distribution<int64_t> dist(lo, hi);
    return Value(dist(random_engine()));
}

Value randstring_fn(VM&, const std::vector<Value>& args) {
    arg_must("randstring", 1);
    const Value& len_val = args[0].deref();
    if (!len_val.isNumber()) {
        throw RuntimeError("randstring requires an integer length");
    }
    const int64_t len = len_val.asInt();
    if (len < 0) {
        throw RuntimeError("randstring length must be non-negative");
    }
    static constexpr char kCharset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static constexpr size_t kCharsetSize = sizeof(kCharset) - 1;
    std::uniform_int_distribution<size_t> dist(0, kCharsetSize - 1);
    std::string out;
    out.reserve(static_cast<size_t>(len));
    for (int64_t i = 0; i < len; ++i) {
        out.push_back(kCharset[dist(random_engine())]);
    }
    return Value(std::move(out));
}

irgen::ModuleObject build_module(
    std::initializer_list<std::pair<const char*, irgen::FunctionType>> entries
) {
    std::map<size_t, std::shared_ptr<irgen::Value>> symbols;
    for (const auto& [name, fn] : entries) {
        symbols[irgen::g_string_pool.add(name)] = std::make_shared<irgen::Value>(irgen::Value(fn));
    }
    return irgen::ModuleObject(irgen::SymbolTable(symbols));
}

irgen::ModuleObject make_iter_module() {
    return build_module({
        {"iter", iter_fn},
        {"next", next_builtin},
        {"enumerate", enumerate_fn},
        {"chain", chain_fn},
        {"to_list", to_list_fn},
    });
}

irgen::ModuleObject make_io_module() {
    return build_module({
        {"read_line", read_line_fn},
        {"read_file", read_file_fn},
        {"write_file", write_file_fn},
        {"write_line", write_line_fn},
        {"eprint", eprint_fn},
    });
}

irgen::ModuleObject make_format_module() {
    return build_module({
        {"format", format_fn},
        {"join", join_fn},
    });
}

irgen::ModuleObject make_random_module() {
    return build_module({
        {"randint", randint_fn},
        {"randstring", randstring_fn},
    });
}

irgen::ModuleObject make_typing_module() {
    irgen::register_typing_constructors();
    std::map<size_t, std::shared_ptr<irgen::Value>> symbols;
    symbols[irgen::g_string_pool.add("Union")] =
        std::make_shared<irgen::Value>(irgen::make_type_value(irgen::get_type_constructor("Union")));
    symbols[irgen::g_string_pool.add("Maybe")] =
        std::make_shared<irgen::Value>(irgen::make_type_value(irgen::get_type_constructor("Maybe")));
    symbols[irgen::g_string_pool.add("Covariant")] =
        std::make_shared<irgen::Value>(irgen::make_type_value(irgen::get_type_constructor("Covariant")));
    symbols[irgen::g_string_pool.add("Contravariant")] =
        std::make_shared<irgen::Value>(irgen::make_type_value(irgen::get_type_constructor("Contravariant")));
    symbols[irgen::g_string_pool.add("Invariant")] =
        std::make_shared<irgen::Value>(irgen::make_type_value(irgen::get_type_constructor("Invariant")));
    return irgen::ModuleObject(irgen::SymbolTable(symbols));
}

#undef arg_must
#undef arg_at_least

} // namespace lang
