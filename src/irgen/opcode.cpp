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
    VM::VM() {
        main_module = std::make_shared<ModuleObject>("__main__", this);
        init_builtins();
    }

    VM::VM(std::vector<Opcode> c) : code(std::move(c)) {
        main_module = std::make_shared<ModuleObject>("__main__", this);
        init_builtins();
    }

    void VM::init_builtins() {
        SymbolTable temp_symbols;
        lang::init_builtins(temp_symbols);
        
        for (const auto& [id, val] : temp_symbols.symbols) {
            main_module->set_attr(g_string_pool.get_string(id), val);
        }
        
        auto std_module = std::make_shared<ModuleObject>(lang::standard_mod);
        std_module->name = "std";
        std_module->full_name = "std";
        main_module->set_attr("std", Value(std_module));
    }

    void VM::run() {
        try {
            scan_labels();
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

    std::optional<Value> VM::get_symbol(const std::string& name) const {
        return main_module->get_attr(name);
    }

    void VM::set_symbol(const std::string& name, const Value& value) {
        main_module->set_attr(name, value);
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
        const std::string var_name = g_string_pool.get_string(operands[0].asInt());
        const auto var_id = static_cast<size_t>(operands[0].asInt());

        vm.cache.add(var_id, value);
        vm.main_module->set_attr(var_name, value);
    }

    inline void LOAD::emit(VM &vm) const {
        const std::string& var_name = g_string_pool.get_string(operands[0].asInt());
        const auto var_id = static_cast<size_t>(operands[0].asInt());

        if (const auto found = vm.cache.get(var_id)) {
            vm.op_stack.push(*found);
            return;
        }

        auto result = vm.main_module->get_attr(var_name);
        if (result.has_value()) {
            vm.op_stack.push(*result);
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
        vm.symbol_stack.emplace_back();
        vm.cache.enter_scope();
    }

    inline void LEAVE_SCOPE::emit(VM &vm) {
        if (!vm.symbol_stack.empty()) {
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

        if (module_name.empty()) {
            throw RuntimeError("Empty module name");
        }

        Value result = vm.main_module->import(module_name);
        vm.op_stack.push(result);
    }

    inline void ATTR::emit(VM &vm) {

    }

    inline void GETATTR::emit(VM &vm) const {
        const std::string attr_name = g_string_pool.get_string(operands[0].asInt());

        Value obj = vm.op_stack.popValue();
        if (obj.getType() != Value::Type::Module) {
            throw RuntimeError("GETATTR requires a module object");
        }

        auto module = obj.asModule();
        auto result = module->get_attr(attr_name);
        
        if (result.has_value()) {
            vm.op_stack.push(*result);
        } else {
            throw RuntimeError("Attribute not found: " + attr_name + " in module: " + module->name);
        }
    }

    Value ModuleObject::import(const std::string& module_name) {
        auto existing = get_attr(module_name);
        if (existing.has_value()) {
            return *existing;
        }

        std::vector<std::string> path_components;
        std::string current_component;
        for (char c : module_name) {
            if (c == '.') {
                if (!current_component.empty()) {
                    path_components.push_back(current_component);
                    current_component.clear();
                }
            } else {
                current_component += c;
            }
        }
        if (!current_component.empty()) {
            path_components.push_back(current_component);
        }

        if (path_components.empty()) {
            throw RuntimeError("Invalid module name: " + module_name);
        }

        std::vector<std::filesystem::path> search_paths = {
            std::filesystem::current_path(),
            std::filesystem::current_path() / "modules",
            std::filesystem::current_path() / "lib",
            std::filesystem::current_path() / "src"
        };

        for (const auto& base_path : search_paths) {
            std::filesystem::path module_path = base_path;
            for (size_t i = 0; i < path_components.size() - 1; ++i) {
                module_path /= path_components[i];
            }
            
            std::string last_component = path_components.back();
            std::filesystem::path file_path = module_path / (last_component + ".lm");
            std::filesystem::path dir_path = module_path / last_component / "main.lm";

            if (std::filesystem::exists(file_path)) {
                std::ifstream file(file_path);
                if (file.is_open()) {
                    std::string code_str((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                    auto module_obj = std::make_shared<ModuleObject>(code_str);
                    module_obj->name = last_component;
                    module_obj->full_name = this->full_name.empty() ? module_name : this->full_name + "." + module_name;
                    set_attr(last_component, Value(module_obj));
                    return Value(module_obj);
                }
            } else if (std::filesystem::exists(dir_path)) {
                std::ifstream file(dir_path);
                if (file.is_open()) {
                    std::string code_str((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                    auto module_obj = std::make_shared<ModuleObject>(code_str);
                    module_obj->name = last_component;
                    module_obj->full_name = this->full_name.empty() ? module_name : this->full_name + "." + module_name;
                    set_attr(last_component, Value(module_obj));
                    return Value(module_obj);
                }
            }
        }

        throw RuntimeError("Module not found: " + module_name);
    }


    inline void LABEL::set_label(VM &vm, const std::optional<size_t> on) const {
        vm.label_table[static_cast<size_t>(operands[0].asInt())] = on.value_or(vm.pc);
    }

    template <StringType string>
    ModuleObject::ModuleObject(string code) : is_user(true) {
        const auto codes = lm::irgen::Generator(parse(code)).gen();
        owner_vm = new VM(codes);
        owner_vm->run();
        
        for (const auto& [name, value] : owner_vm->main_module->exports) {
            exports[name] = value;
        }
        
        for (const auto& [name, submod] : owner_vm->main_module->submodules) {
            submodules[name] = submod;
        }
    }
}

