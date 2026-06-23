#include "std_modules.hpp"

#include "builtins.hpp"

#include <fstream>
#include <iostream>
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

#undef arg_must
#undef arg_at_least

} // namespace lang
