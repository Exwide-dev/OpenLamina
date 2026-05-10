#include "opcode.hpp"
#include "../tools/lang/builtins.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <ranges>

#include "generator.hpp"

namespace irgen {
    std::string Value::toString() const {
        const Value& self = deref();
        switch (self.type) {
            case Type::None: return "None";
            case Type::Number: return asNumber().toString();
            case Type::Bool: return (asBool() ? "true" : "false");
            case Type::String: return "\"" + asString() + "\"";
            case Type::Function: return std::format("<function at 0x{:x}>", reinterpret_cast<uintptr_t>(this));
            case Type::Vector: {
                std::string result = "vec[";
                const auto& elements = asVector();
                for (size_t i = 0; i < elements.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += elements[i]->toString();
                }
                result += "]";
                return result;
            }
            case Type::Dictionary: {
                std::string result = "{";
                const auto& entries = asDictionary();
                size_t i = 0;
                for (const auto& [key, value] : entries) {
                    if (i > 0) result += ", ";
                    result += "\"" + key + "\": " + value->toString();
                    ++i;
                }
                result += "}";
                return result;
            }
            case Type::Reference: {
                auto t = asReference();
                return t->get().toString();
            }
            case Type::Module: {
                const auto t = asModule();
                return std::format("Module {}: {{ \n{}", t->full_name, [&] -> std::string {
                    std::string result;
                    for (const auto& [a, b] : t->exports) {
                        result.append(std::format("  {}: {}\n", a, b.toString()));
                    }
                    return result + "}";
                }());
            }
            default: return "<__UNKNOWN_Value>";
        }
    }

    std::optional<std::shared_ptr<Value>> SymbolTable::get(const size_t id) const noexcept {
        return symbols.try_get(id);
    }

    void SymbolTable::set(const size_t id, const Value &value) {
        symbols.set(id, std::make_shared<Value>(value));
    }

    void SymbolTable::set(const size_t id, const std::shared_ptr<Value>& value) {
        symbols.set(id, value);
    }

    void SymbolTable::set_constant(size_t id, bool is_constant) {
        constants.set(id, is_constant);
    }

    bool SymbolTable::is_constant(const size_t id) const noexcept {
        return constants.get(id);
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
            main_module->set_attr(g_string_pool.get_string(id), *val);
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
        vm.op_stack.push(a.deref() + b.deref());
    }

    inline void MUL::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() * b.deref());
    }

    inline void SUB::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() - b.deref());
    }

    inline void DIV::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() / b.deref());
    }

    inline void NEG::emit(VM &vm) {
        auto value = vm.op_stack.popValue();
        vm.op_stack.push(-value.deref());
    }

    inline void NOT::emit(VM &vm) {
        auto value = vm.op_stack.popValue();
        vm.op_stack.push(!value.deref());
    }

    inline void AND::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() && b.deref());
    }

    inline void OR::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() || b.deref());
    }

    inline void EQ::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(Value(a.deref() == b.deref()));
    }

    inline void NEQ::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(Value(a.deref() != b.deref()));
    }

    inline void LT::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() < b.deref());
    }

    inline void LTE::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() <= b.deref());
    }

    inline void GT::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() > b.deref());
    }

    inline void GTE::emit(VM &vm) {
        auto b = vm.op_stack.popValue();
        auto a = vm.op_stack.popValue();
        vm.op_stack.push(a.deref() >= b.deref());
    }

    std::string SymbolTable::toString() const {
        std::string k;
        for (const auto& [a, b] : *this) {
            k.append(std::format("{} : {}({})", a, b->toString(), b->type_name()));
        }
        return k;
    }

    inline void STORE::emit(VM &vm) const {
        Value value = vm.op_stack.popValue();
        Value ref = vm.op_stack.popValue();
        
        bool is_const = false;
        bool has_value = false;
        
        if (ref.isReference()) {
            const auto& ref_ptr = ref.asReference();
            if (ref_ptr->value_ptr && !vm.symbol_stack.empty()) {
                // 在修改之前先检查常量状态
                for (auto& symbol_table : vm.symbol_stack) {
                    for (const auto& [id, val] : symbol_table.symbols) {
                        if (val.get() == ref_ptr->value_ptr.get()) {
                            is_const = symbol_table.is_constant(id);
                            // 直接检查类型，避免 deref() 的问题
                            has_value = val->getType() != Value::Type::None;
                            break;
                        }
                    }
                }
            }
            
            // 如果是常量且已有值，不允许修改
            if (is_const && has_value) {
                throw RuntimeError("Cannot modify constant");
            }
            
            // 执行赋值
            ref.set(value);
            
            if (ref_ptr->value_ptr && vm.main_module) {
                for (auto& symbol_table : vm.symbol_stack) {
                    for (const auto& [id, val] : symbol_table.symbols) {
                        if (val.get() == ref_ptr->value_ptr.get()) {
                            vm.main_module->set_attr(g_string_pool.get_string(id), *val);
                        }
                    }
                }
            }
        }

        LOG("Done, now symbol: " << vm.symbol_stack.back().toString());
    }

    inline void LOAD::emit(VM &vm) const {
        const auto var_id = static_cast<size_t>(operands[0].asInt());
        const std::string& var_name = g_string_pool.get_string(var_id);

        std::shared_ptr<Value> value_ptr;
        
        // 先从符号栈查找
        for (auto & symbol_table : std::ranges::reverse_view(vm.symbol_stack)) {
            if (symbol_table.exists(var_id)) {
                auto k = symbol_table.get(var_id);
                if (k.has_value()) {
                    value_ptr = *k;
                    break;
                }
            }
        }

        // 找不到则从模块查找或创建新值
        if (!value_ptr) {
            if (vm.main_module) {
                auto attr = vm.main_module->get_attr(var_name);
                if (attr.has_value()) {
                    value_ptr = std::make_shared<Value>(*attr);
                }
            }
            
            // 如果仍然找不到，创建一个新的变量（不是常量）
            if (!value_ptr) {
                value_ptr = std::make_shared<Value>(std::make_shared<Ref>(std::make_shared<Value>()));
                if (!vm.symbol_stack.empty()) {
                    vm.symbol_stack.back().set(var_id, value_ptr);
                    vm.symbol_stack.back().set_constant(var_id, false);
                }
                vm.op_stack.push(*value_ptr);
                return;
            }
        }

        // 如果值已经是引用，直接返回，否则包装成引用
        if (value_ptr->isReference()) {
            vm.op_stack.push(*value_ptr);
        } else {
            vm.op_stack.push(Value(std::make_shared<Ref>(value_ptr)));
        }
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
        LOG("Enter scope");
        vm.symbol_stack.emplace_back();
        vm.cache.enter_scope();
    }

    inline void LEAVE_SCOPE::emit(VM &vm) {
        LOG("Leave scope");
        if (!vm.symbol_stack.empty()) {
            vm.symbol_stack.pop_back();
        }
        vm.cache.leave_scope();
    }

    inline void CALL::emit(VM &vm) const {
        Value func = vm.op_stack.popValue();
        LOG(ITIS(,func,.toString()) << ", type: " << func.type_name());

        if (not func.isFunction()) {
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


    inline void GETATTR::emit(VM &vm) const {
        const std::string attr_name = g_string_pool.get_string(operands[0].asInt());

        Value obj = vm.op_stack.popValue();
        const Value& module_val = obj.deref();
        if (module_val.getType() != Value::Type::Module) {
            throw RuntimeError("GETATTR requires a module object");
        }

        auto module = module_val.asModule();
        auto result = module->get_attr(attr_name);
        
        if (result.has_value()) {
            // 对于模块属性，我们创建一个共享的值并返回引用
            auto value_ptr = std::make_shared<Value>(*result);
            vm.op_stack.push(Value(std::make_shared<Ref>(value_ptr)));
        } else {
            throw RuntimeError("Attribute not found: " + attr_name + ", in module: " + module->name);
        }
    }

    inline void VEC_NEW::emit(VM &vm) const {
        size_t element_count = static_cast<size_t>(operands[0].asInt());
        std::vector<std::shared_ptr<Value>> elements;
        elements.reserve(element_count);
        
        for (size_t i = 0; i < element_count; ++i) {
            elements.push_back(std::make_shared<Value>(vm.op_stack.popValue()));
        }
        
        vm.op_stack.push(Value(std::move(elements)));
    }

    inline void DICT_NEW::emit(VM &vm) const {
        size_t entry_count = static_cast<size_t>(operands[0].asInt());
        std::unordered_map<std::string, std::shared_ptr<Value>> dict;
        
        for (size_t i = 0; i < entry_count; ++i) {
            Value value = vm.op_stack.popValue();
            Value key_val = vm.op_stack.popValue();
            std::string key = key_val.deref().asString();
            dict[key] = std::make_shared<Value>(value);
        }
        
        vm.op_stack.push(Value(std::move(dict)));
    }

    inline void INDEX::emit(VM &vm) const {
        Value index_val = vm.op_stack.popValue();
        Value obj = vm.op_stack.popValue();
        
        // 解引用对象
        Value& obj_deref = obj.deref();
        
        if (obj_deref.isVector()) {
            const Value& idx_deref = index_val.deref();
            ptrdiff_t idx = idx_deref.asInt();
            
            auto& vec = obj_deref.asVector();
            if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                throw RuntimeError("Index out of range");
            }
            
            vm.op_stack.push(Value(std::make_shared<Ref>(vec[static_cast<size_t>(idx)])));
        } else if (obj_deref.isDictionary()) {
            std::string key = index_val.deref().asString();
            
            auto& dict = obj_deref.asDictionary();
            auto it = dict.find(key);
            if (it == dict.end()) {
                throw RuntimeError("Key not found: " + key);
            }
            
            vm.op_stack.push(Value(std::make_shared<Ref>(it->second)));
        } else {
            throw RuntimeError("INDEX requires a vector or dictionary object");
        }
    }

    inline void STORE_ARG::emit(VM &vm) const {
        const Value value = vm.op_stack.popValue();
        const auto var_id = static_cast<size_t>(operands[0].asInt());
        
        /*if (vm.symbol_stack.empty()) {
            throw RuntimeError("No symbol table available");
        }*/
        
        vm.symbol_stack.back().set(var_id, value);
    }

    void NEW_VAR::emit(VM &vm) const {
        auto ref_val = std::make_shared<Ref>(std::make_shared<Value>());
        Value var(ref_val);
        const auto var_id = static_cast<size_t>(operands[0].asInt());
        const auto var_name = g_string_pool.get_string(var_id);
        
        if (!vm.symbol_stack.empty()) {
            vm.symbol_stack.back().set(var_id, ref_val->value_ptr);
            vm.symbol_stack.back().set_constant(var_id, false);
        }
        vm.main_module->set_attr(var_name, var);
        vm.op_stack.push(var);
    }

    void NEW_CONST::emit(VM &vm) const {
        auto ref_val = std::make_shared<Ref>(std::make_shared<Value>());
        Value var(ref_val);
        const auto var_id = static_cast<size_t>(operands[0].asInt());
        const auto var_name = g_string_pool.get_string(var_id);
        
        if (!vm.symbol_stack.empty()) {
            vm.symbol_stack.back().set(var_id, ref_val->value_ptr);
            vm.symbol_stack.back().set_constant(var_id, true);
        }
        vm.main_module->set_attr(var_name, var);
        vm.op_stack.push(var);
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

        std::vector search_paths = {
            std::filesystem::current_path(),
            std::filesystem::current_path() / "modules",
            std::filesystem::current_path() / "lib",
            std::filesystem::current_path() / "src",
            std::filesystem::current_path() / "..",
            std::filesystem::current_path() / ".." / "modules",
            std::filesystem::current_path() / ".." / "lib",
            std::filesystem::current_path() / ".." / "src"
        };

        for (const auto& base_path : search_paths) {
            LOG("Search in path: " + base_path.string());
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

