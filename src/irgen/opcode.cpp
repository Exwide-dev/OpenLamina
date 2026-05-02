#include "opcode.hpp"
#include "../tools/lang/builtins.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>

#include "generator.hpp"

namespace irgen {
    std::string Value::toString() const {
        switch (type) {
            case Type::None: return "None";
            case Type::Number: return asNumber().toString();
            case Type::Bool: return (asBool() ? "true" : "false");
            case Type::String: return "\"" + asString() + "\"";
            case Type::Function: return std::format("<function at 0x{:x}>", reinterpret_cast<uintptr_t>(this));
            default: return "<__UNKNOWN_ValueType>";
        }
    }

    std::optional<Value> SymbolTable::get(const size_t id) const noexcept {
        const auto it = symbols.find(id);
        if (it == symbols.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void SymbolTable::set(const size_t id, const Value &value) {
        symbols[id] = value;
    }

    // 初始化内置函数
    void VM::init_builtins() {
        lang::init_builtins(symbols);
        
        auto std_module = std::make_shared<ModuleObject>(lang::standard_mod);
        symbols.set(g_string_pool.add("std"), Value(std_module));
    }

    void Number::reduce() {
        if (denominator == 0) {
            throw RuntimeError("Division by zero");
        }
        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }
        ptrdiff_t g = gcd(std::abs(numerator), std::abs(denominator));
        numerator /= g;
        denominator /= g;
    }

    ptrdiff_t Number::gcd(ptrdiff_t a, ptrdiff_t b) const {
        while (b != 0) {
            const ptrdiff_t t = b;
            b = a % b;
            a = t;
        }
        return a;
    }

    void VM::run() {
        try {
            scan_labels();
            symbol_stack.push_back(symbols);
            for (; pc < code.size(); pc++) {
                std::visit([&](auto &op) -> void {
                    LOG("Exec " << pc << " | " << op.name() << " " << op.stringArgs());
                    op.emit(*this);
                    LOG("VM " << op_stack.toString());
                }, code[pc]);
            }
            if (!op_stack.empty()) {
                const Value top = op_stack.popValue();
                op_stack.clear();
                op_stack.push(top);
            }
        } catch ([[maybe_unused]] const std::exception &e) {
            op_stack.clear();
            throw;
        }
    }

    inline void PUSH::emit(VM &vm) const {
        vm.op_stack.push(operands[0]);
    }

    inline void ADD::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a + b);
    }

    inline void MUL::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a * b);
    }

    inline void SUB::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a - b);
    }

    inline void DIV::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a / b);
    }

    inline void NEG::emit(VM &vm) {
        auto value = vm.op_stack.popValue();
        vm.op_stack.push(-value);
    }

    inline void NOT::emit(VM &vm) {
        auto value = vm.op_stack.popValue();
        vm.op_stack.push(!value);
    }

    inline void AND::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a && b);
    }

    inline void OR::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a || b);
    }

    inline void EQ::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(Value(a == b));
    }

    inline void NEQ::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(Value(a != b));
    }

    inline void LT::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a < b);
    }

    inline void LTE::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a <= b);
    }

    inline void GT::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a > b);
    }

    inline void GTE::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a >= b);
    }

    inline void STORE::emit(VM &vm) const {
        const auto& value = vm.op_stack.popValue();
        const auto var_id = static_cast<size_t>(operands[0].asInt());

        vm.cache.add(var_id, value);
        vm.symbols.set(var_id, value);
    }

    inline void LOAD::emit(VM &vm) const {
        const auto var_id = static_cast<size_t>(operands[0].asInt());

        if (const auto found = vm.cache.get(var_id)) {
            vm.op_stack.push(*found);
            return;
        }

        if (vm.symbols.exists(var_id)) {
            vm.op_stack.push(vm.symbols.get(var_id).value());
            return;
        }

        for (auto it = vm.symbol_stack.rbegin(); it != vm.symbol_stack.rend(); ++it) {
            const auto& symbol_table = *it;
            if (symbol_table.exists(var_id)) {
                auto k = symbol_table.get(var_id);
                if (k.has_value()) vm.op_stack.push(k.value());
                else throw RuntimeError("Var not found: " + std::to_string(var_id));
                return;
            }
        }

        throw RuntimeError("Variable not found: " + g_string_pool.get_string(var_id));
    }

    inline void LABEL::emit(VM &) {}

    inline void GOTO::emit(VM &vm) const {
        const auto label_id = static_cast<size_t>(operands[0].asInt());
        if (not vm.label_table.contains(label_id)) {
            throw RuntimeError("Unknown label: " + std::to_string(label_id));
        }
        vm.pc = vm.label_table[label_id];
    }

    inline void IFTRUEGOTO::emit(VM &vm) const {
        if (vm.op_stack.popValue().asBool()) {
            GOTO(operands[0]).emit(vm);
        }
    }

    inline void ENTER_SCOPE::emit(VM &vm) {
        vm.symbol_stack.push_back(vm.symbols);
        vm.symbols = SymbolTable();
        vm.cache.enter_scope();
    }

    inline void LEAVE_SCOPE::emit(VM &vm) {
        if (!vm.symbol_stack.empty()) {
            vm.symbols = vm.symbol_stack.back();
            vm.symbol_stack.pop_back();
        }
        vm.cache.leave_scope();
    }

    inline void CALL::emit(VM &vm) const {
        Value func = vm.op_stack.popValue();

        if (!func.isFunction()) {
            throw RuntimeError("Not a function");
        }

        if (func.isUserFunction()) {
            auto func_obj = func.asFunctionObject();
            vm.call_stack.push(Value(vm.pc));

            if (vm.label_table.contains(func_obj->location)) {
                vm.pc = vm.label_table[func_obj->location];
            } else {
                throw RuntimeError("Function label not found: " + std::to_string(func_obj->location));
            }
        } else {
            auto arg_count = operands[0].asInt();
            std::vector<Value> args;
            args.reserve(arg_count);
            for (ptrdiff_t i = 0; i < arg_count; ++i) {
                args.emplace_back(vm.op_stack.popValue());
            }

            auto builtin_func = func.asFunction();
            auto result = builtin_func(vm, args);
            vm.op_stack.push(result);
        }
    }

    inline void RET::emit(VM &vm) {
        if (!vm.call_stack.empty()) {
            auto return_addr = vm.call_stack.popValue().asInt();
            vm.pc = static_cast<size_t>(return_addr);
        } else {
            vm.pc = vm.code.size();
        }
    }

    inline void FINDMOD::emit(VM &vm) {
        const std::string module_name = g_string_pool.get_string(operands[0].asInt());
        const size_t module_name_id = operands[0].asInt();

        if (vm.symbols.exists(module_name_id)) {
            vm.op_stack.push(vm.symbols.get(module_name_id).value());
            return;
        }

        if (module_name == "std") {
            auto std_module = std::make_shared<ModuleObject>(lang::standard_mod);
            vm.symbols.set(module_name_id, Value(std_module));
            vm.op_stack.push(Value(std_module));
            return;
        }

        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::current_path(),
            std::filesystem::current_path() / "modules",
            std::filesystem::current_path() / "lib"
        };

        for (const auto& path : search_paths) {
            std::filesystem::path module_path = path / (module_name + ".lm");
            if (std::filesystem::exists(module_path)) {
                std::ifstream file(module_path);
                if (file.is_open()) {
                    std::string code((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                    auto module_obj = std::make_shared<ModuleObject>(code);
                    vm.symbols.set(module_name_id, Value(module_obj));
                    vm.op_stack.push(Value(module_obj));
                    return;
                }
            }
        }

        throw RuntimeError("Module not found: " + module_name);
    }

    inline void ATTR::emit(VM &vm) {

    }

    inline void GETATTR::emit(VM &vm) const {
        const std::string attr_name = g_string_pool.get_string(operands[0].asInt());
        const size_t attr_name_id = operands[0].asInt();

        Value obj = vm.op_stack.popValue();
        if (obj.getType() != Value::Type::Module) {
            throw RuntimeError("GETATTR requires a module object");
        }

        auto module = obj.asModule();
        if (module->vm.symbols.exists(attr_name_id)) {
            vm.op_stack.push(module->vm.symbols.get(attr_name_id).value());
        } else {
            throw RuntimeError("Attribute not found: " + attr_name);
        }
    }


    inline void LABEL::set_label(VM &vm, const std::optional<size_t> on) const {
        vm.label_table[static_cast<size_t>(operands[0].asInt())] = on.value_or(vm.pc);
    }

    template <StringType string>
    ModuleObject::ModuleObject(string code) : is_user(true) {
        auto codes = lm::irgen::Generator(parse(code)).gen();
        vm.code.insert(vm.code.begin(), codes.begin(), codes.end());
    }
}

