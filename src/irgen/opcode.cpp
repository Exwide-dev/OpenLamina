#include "opcode.hpp"

#include <algorithm>

#include "../tools/lang/builtins.hpp"
#include "../tools/lang/type_methods.hpp"
#include "../tools/debug.hpp"
#include "cell_pool.hpp"
#include <fstream>
#include <filesystem>
#include <ranges>

#include "generator.hpp"
#include "struct_types.hpp"
#include "../front-end/front_end.hpp"

namespace irgen {

namespace {

[[nodiscard]] std::string vm_current_scope(const VM& vm) {
    if (vm.call_func_stack.empty()) {
        return "global-scope";
    }
    return vm.call_func_stack.back()->name;
}

[[nodiscard]] TraceFrame vm_opcode_site(const VM& vm, const size_t at_pc = SIZE_MAX) {
    TraceFrame frame;
    frame.filename = vm.source_filename;
    frame.column = 0;
    frame.scope = vm_current_scope(vm);
    const size_t pc = at_pc == SIZE_MAX ? vm.pc : at_pc;
    if (pc < vm.code.size()) {
        frame.line = get_opcode_line_no(vm.code[pc]);
        frame.source_line = get_opcode_line(vm.code[pc]);
    }
    return frame;
}

[[nodiscard]] std::vector<TraceFrame> build_traceback(const VM& vm) {
    std::vector<TraceFrame> frames;
    frames.reserve(vm.call_stack.size());
    size_t i = 0;
    for (const size_t ret_pc : vm.call_stack) {
        TraceFrame frame = vm_opcode_site(vm, ret_pc);
        if (i == 0) {
            frame.scope = "global-scope";
        } else if (i - 1 < vm.call_func_stack.size()) {
            frame.scope = vm.call_func_stack[i - 1]->name;
        }
        frames.push_back(std::move(frame));
        ++i;
    }
    return frames;
}

} // namespace

#define VM_RUNTIME_ERROR(vm, msg) \
    throw RuntimeError((msg), vm_opcode_site((vm)), build_traceback((vm)))

Value Value::makeEmptyRef(CellPool& pool) {
    return Value(Ref(pool.allocate(), false, &pool));
}

Value Value::makeRef(Value&& val, CellPool& pool) {
    return Value(Ref(pool.allocateValue(std::move(val)), false, &pool));
}

Value Value::makeRef(const Value& val, CellPool& pool) {
    return Value(Ref(pool.allocateCopy(val), false, &pool));
}

Value Value::makeRef(std::shared_ptr<Value> val_ptr, CellPool& pool) {
    return Value(Ref(std::move(val_ptr), false, &pool));
}

Ref::Ref(std::shared_ptr<Value> ptr, const bool is_opaque, CellPool* pool)
    : value_ptr(std::move(ptr)), opaque(is_opaque) {
    if (!opaque && value_ptr->isReference() && pool != nullptr) {
        value_ptr = pool->allocateCopy(value_ptr->deref());
    }
}

std::string Value::displayString() const {
    if (type == Type::Reference && asReference().opaque) {
        return std::format(
            "<pointer to 0x{:x}>",
            reinterpret_cast<uintptr_t>(asReference().value_ptr.get())
        );
    }
    const Value& self = deref();
    switch (self.type) {
        case Type::None: return "None";
        case Type::Number: return asNumber().toString();
        case Type::Bool: return asBool() ? "true" : "false";
        case Type::String: return "\"" + asString() + "\"";
        case Type::Function: return std::format("<function at 0x{:x}>", reinterpret_cast<uintptr_t>(this));
        case Type::Rational: return asRational().toString();
        case Type::Vector: {
            std::string result = "vec[";
            const auto& elements = asVector();
            for (size_t i = 0; i < elements.size(); ++i) {
                if (i > 0) result += ", ";
                result += elements[i]->displayString();
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
                result.append(key->displayString() + ": " + value->displayString());
                ++i;
            }
            result += "}";
            return result;
        }
        case Type::Reference: {
            auto t = asReference();
            return t.get().displayString();
        }
        case Type::StructObject: {
            const auto inst = asStruct();
            std::string result = inst->type->name + "{";
            for (size_t i = 0; i < inst->slots.size(); ++i) {
                if (i > 0) {
                    result += ", ";
                }
                result += inst->type->fields[i].name + "=" + inst->slots[i].displayString();
            }
            result += "}";
            return result;
        }
        case Type::Module: {
            const auto t = asModule();
            return std::format(
                "Module {}: {{ \n{}",
                t->full_name,
                [&] -> std::string {
                    std::string result;
                    for (const auto& [a, b] : t->exports) {
                        result.append(std::format("  {}: {}\n", a, b.displayString()));
                    }
                    return result + "}";
                }()
            );
        }
        default: return "<__UNKNOWN_Value>";
    }
}

std::string Value::printString() const {
    if (type == Type::Reference && asReference().opaque) {
        return displayString();
    }
    const Value& self = deref();
    switch (self.type) {
        case Type::None: return "None";
        case Type::Number: return asNumber().toString();
        case Type::Bool: return asBool() ? "true" : "false";
        case Type::String: return asString();
        case Type::Function: return std::format("<function at 0x{:x}>", reinterpret_cast<uintptr_t>(this));
        case Type::Rational: return asRational().toString();
        case Type::Vector: {
            std::string result = "vec[";
            const auto& elements = asVector();
            for (size_t i = 0; i < elements.size(); ++i) {
                if (i > 0) result += ", ";
                result += elements[i]->printString();
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
                result.append(key->printString() + ": " + value->printString());
                ++i;
            }
            result += "}";
            return result;
        }
        case Type::Reference: {
            auto t = asReference();
            return t.get().printString();
        }
        case Type::StructObject: {
            const auto inst = asStruct();
            std::string result = inst->type->name + "{";
            for (size_t i = 0; i < inst->slots.size(); ++i) {
                if (i > 0) {
                    result += ", ";
                }
                result += inst->type->fields[i].name + "=" + inst->slots[i].printString();
            }
            result += "}";
            return result;
        }
        case Type::Module: {
            const auto t = asModule();
            return std::format(
                "Module {}: {{ \n{}",
                t->full_name,
                [&] -> std::string {
                    std::string result;
                    for (const auto& [a, b] : t->exports) {
                        result.append(std::format("  {}: {}\n", a, b.printString()));
                    }
                    return result + "}";
                }()
            );
        }
        default: return "<__UNKNOWN_Value>";
    }
}

std::string Value::toString() const {
    return displayString();
}

std::optional<std::shared_ptr<Value>> SymbolTable::get(const size_t id) const noexcept {
    return symbols.try_get(id);
}

void SymbolTable::set(const size_t id, const Value& value, CellPool& pool) {
    symbols.set(id, pool.allocateCopy(value));
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


namespace {

void rebindValueAfterMove(Value& val, VM& old_vm, VM& new_vm) {
    if (val.isReference()) {
        if (val.asReference().value_ptr) {
            rebindValueAfterMove(*val.asReference().value_ptr, old_vm, new_vm);
        }
        return;
    }

    Value& self = val;

    if (self.isUserFunction()) {
        auto& func = *self.asFunctionObject();
        if (func.owner_vm == &old_vm) {
            func.owner_vm = &new_vm;
        }
        for (SymbolTable& table : func.closure) {
            for (const auto& [id, ptr] : table.symbols) {
                (void)id;
                if (ptr) {
                    rebindValueAfterMove(*ptr, old_vm, new_vm);
                }
            }
        }
        return;
    }

    if (self.getType() == Value::Type::Iterator) {

        if (auto iter_ptr = self.asIterator()) {
            auto& iter = *iter_ptr;
            if (iter.cell_pool == &old_vm.cell_pool) {
                iter.cell_pool = &new_vm.cell_pool;
            }
            if (iter.iterable) {
                rebindValueAfterMove(*iter.iterable, old_vm, new_vm);
            }
        }
        return;
    }

    if (self.isVector()) {
        for (const auto& elem : self.asVector()) {
            if (elem) {
                rebindValueAfterMove(*elem, old_vm, new_vm);
            }
        }
        return;
    }

    if (self.isDictionary()) {
        for (const auto& [key, elem] : self.asDictionary()) {
            if (key) {
                rebindValueAfterMove(*key, old_vm, new_vm);
            }
            if (elem) {
                rebindValueAfterMove(*elem, old_vm, new_vm);
            }
        }
        return;
    }

    if (self.isStruct()) {
        if (const auto& obj = self.asStruct()) {
            for (Value& slot : obj->slots) {
                rebindValueAfterMove(slot, old_vm, new_vm);
            }
        }
        return;
    }

    if (self.getType() == Value::Type::Module) {
        if (const auto mod = self.asModule()) {
            if (mod->owner_vm == &old_vm) {
                mod->owner_vm = &new_vm;
            }
            for (auto& [name, export_val] : mod->exports) {
                (void)name;
                rebindValueAfterMove(export_val, old_vm, new_vm);
            }
            for (auto& [name, sub] : mod->submodules) {
                (void)name;
                if (sub && sub->owner_vm == &old_vm) {
                    sub->owner_vm = &new_vm;
                }
                if (sub) {
                    for (auto& [sub_name, export_val] : sub->exports) {
                        (void)sub_name;
                        rebindValueAfterMove(export_val, old_vm, new_vm);
                    }
                }
            }
        }
    }
}

void rebindModuleTreeAfterMove(ModuleObject& module, VM& old_vm, VM& new_vm) {
    if (module.owner_vm == &old_vm) {
        module.owner_vm = &new_vm;
    }
    for (auto& [name, export_val] : module.exports) {
        (void)name;
        rebindValueAfterMove(export_val, old_vm, new_vm);
    }
    for (auto& [name, sub] : module.submodules) {
        (void)name;
        if (sub) {
            rebindModuleTreeAfterMove(*sub, old_vm, new_vm);
        }
    }
}

} // namespace

// 初始化内置函数
VM::VM() {
    cell_pool.bindOwner(this);
    symbol_stack.reserve(256);
    call_stack.reserve(256);
    main_module = std::make_shared<ModuleObject>("__main__", this);
    init_builtins();
}

VM::VM(std::vector<Opcode> c) : code(std::move(c)) {
    cell_pool.bindOwner(this);
    symbol_stack.reserve(256);
    call_stack.reserve(256);
    main_module = std::make_shared<ModuleObject>("__main__", this);
    init_builtins();
}

VM::VM(VM&& other) noexcept
    : cell_pool(std::move(other.cell_pool)),
      op_stack(std::move(other.op_stack)),
      call_stack(std::move(other.call_stack)),
      source_filename(std::move(other.source_filename)),
      call_func_stack(std::move(other.call_func_stack)),
      code(std::move(other.code)),
      label_scan_end(other.label_scan_end),
      symbol_stack(std::move(other.symbol_stack)),
      locals_stack(std::move(other.locals_stack)),
      cache(std::move(other.cache)),
      label_table(std::move(other.label_table)),
      pc(other.pc),
      label_counter(other.label_counter),
      main_module(std::move(other.main_module)) {
    rebindAfterMove(other);
}

VM& VM::operator=(VM&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    cell_pool = std::move(other.cell_pool);
    op_stack = std::move(other.op_stack);
    call_stack = std::move(other.call_stack);
    source_filename = std::move(other.source_filename);
    call_func_stack = std::move(other.call_func_stack);
    code = std::move(other.code);
    label_scan_end = other.label_scan_end;
    symbol_stack = std::move(other.symbol_stack);
    locals_stack = std::move(other.locals_stack);
    cache = std::move(other.cache);
    label_table = std::move(other.label_table);
    pc = other.pc;
    label_counter = other.label_counter;
    main_module = std::move(other.main_module);

    rebindAfterMove(other);
    return *this;
}

void VM::rebindAfterMove(VM& source) noexcept {
    cell_pool.bindOwner(this);

    for (Value& val : op_stack) {
        rebindValueAfterMove(val, source, *this);
    }
    for (auto& frame : locals_stack) {
        for (Value& val : frame) {
            rebindValueAfterMove(val, source, *this);
        }
    }
    for (SymbolTable& table : symbol_stack) {
        for (const auto& [id, ptr] : table.symbols) {
            (void)id;
            if (ptr) {
                rebindValueAfterMove(*ptr, source, *this);
            }
        }
    }
    for (const auto& func : call_func_stack) {
        if (!func) {
            continue;
        }
        if (func->owner_vm == &source) {
            func->owner_vm = this;
        }
        for (SymbolTable& table : func->closure) {
            for (const auto& [id, ptr] : table.symbols) {
                (void)id;
                if (ptr) {
                    rebindValueAfterMove(*ptr, source, *this);
                }
            }
        }
    }
    for (const auto& [id, ptr] : cache.allEntries()) {
        (void)id;
        if (ptr) {
            rebindValueAfterMove(*ptr, source, *this);
        }
    }
    if (main_module) {
        rebindModuleTreeAfterMove(*main_module, source, *this);
    }

    source.cell_pool.bindOwner(&source);
}

void VM::init_builtins() {
    SymbolTable temp_symbols;
    lang::init_builtins(temp_symbols, cell_pool);

    for (const auto& [id, val] : temp_symbols.symbols) {
        main_module->set_attr(g_string_pool.get_string(id), *val);
    }

    auto std_module = std::make_shared<ModuleObject>(lang::standard_mod);
    std_module->name = "std";
    std_module->full_name = "std";
    main_module->set_attr("std", Value(std_module));
}

void VM::run() {
#ifdef ANALYSE
    std::unordered_map<std::string, int> callmap{};
#endif
    size_t ops_since_gc = 0;
    try {
        scan_labels();
        for (; pc < code.size(); pc++) {
            std::visit(
                [&](auto& op) -> void {
                    LOG("Exec " << pc << " | " << op.name() << " " << op.stringArgs());
                    op.emit(*this);
#ifdef ANALYSE
                    ++callmap[op.name()];
#endif
                },
                code[pc]
            );

            if (++ops_since_gc >= 1000000 && gc_suppress_depth == 0) {
                collectGarbage();
                ops_since_gc = 0;
            }
        }

#ifdef ANALYSE
        for (const auto& [callkey, calls] : callmap) {
            std::cerr <<
                    std::format("{}: {}", callkey, calls)
                    << std::endl;
        }
#endif

        if (!op_stack.empty()) {
            const Value top = op_stack.popValue();
            op_stack.clear();
            op_stack.push(top);
        }
    } catch ([[maybe_unused]] const std::exception& e) {
        op_stack.clear();
        throw;
    }
}

void VM::collectGarbage() {
    cell_pool.collectGarbage(*this);
}

std::optional<Value> VM::get_symbol(const std::string& name) const {
    return main_module->get_attr(name);
}

void VM::set_symbol(const std::string& name, const Value& value) {
    main_module->set_attr(name, value);
}

LOAD::LOAD(const std::string& name) : var_id(g_string_pool.add(name)) {}

STORE_ARG::STORE_ARG(const std::string& name) : var_id(g_string_pool.add(name)) {}

NEW_VAR::NEW_VAR(const std::string& name) : var_id(g_string_pool.add(name)) {}

NEW_CONST::NEW_CONST(const std::string& name) : var_id(g_string_pool.add(name)) {}

NEW_INTERN_VAR::NEW_INTERN_VAR(const std::string& name) : var_id(g_string_pool.add(name)) {}

NEW_INTERN_CONST::NEW_INTERN_CONST(const std::string& name) : var_id(g_string_pool.add(name)) {}

NEW_VAR_OR_LOAD::NEW_VAR_OR_LOAD(const std::string& name) : var_id(g_string_pool.add(name)) {}

FINDMOD::FINDMOD(const std::string& name) : module_id(g_string_pool.add(name)) {}

GETATTR::GETATTR(const std::string& name) : name_id(g_string_pool.add(name)) {}

SET_FIELD::SET_FIELD(const std::string& field_name) : name_id(g_string_pool.add(field_name)) {}

STRUCT_NEW::STRUCT_NEW(const std::string& struct_name, const size_t arg_count)
    : struct_id(g_string_pool.add(struct_name)), arg_count(arg_count) {}

BIND_FAST::BIND_FAST(const size_t slot, const std::string& name)
    : slot(slot), var_id(g_string_pool.add(name)) {}

inline void PUSH::emit(VM& vm) const {
    if (val.getType() == Value::Type::Function) {
        try {
            auto func = val.asFunctionObject();
            LOG("PUSH Function: name=" << func->name << ", location=" << func->location);
            LOG("  needs_closure=" << func->needs_closure);
            if (func->needs_closure && func->closure.empty()) {
                LOG("  Capturing closure from " << vm.symbol_stack.size() << " scopes");
                if (!vm.symbol_stack.empty()) {
                    for (size_t i = 0; i < vm.symbol_stack.size(); i++) {
                        func->closure.push_back(vm.symbol_stack[i]);
                        LOG("    Captured scope " << i << ": " << vm.symbol_stack[i].toString());
                    }
                } else {
                    LOG("    No scopes available");
                }
            } else if (!func->needs_closure) {
                LOG("  No closure needed, skipping capture");
            } else {
                LOG("  Closure already captured with " << func->closure.size() << " scopes");
            }
        } catch ([[maybe_unused]] const RuntimeError& e) {
            LOG("  Not a user-defined function");
        }
    }
    vm.op_stack.push(val);
}

inline void ADD::emit(VM& vm) {
    Value rhs = vm.op_stack.popValue();
    Value lhs = vm.op_stack.popValue();
    vm.op_stack.push(lhs.deref() + rhs.deref());
}

inline void MUL::emit(VM& vm) {
    Value rhs = vm.op_stack.popValue();
    Value lhs = vm.op_stack.popValue();
    vm.op_stack.push(lhs.deref() * rhs.deref());
}

inline void SUB::emit(VM& vm) {
    Value rhs = vm.op_stack.popValue();
    Value lhs = vm.op_stack.popValue();
    vm.op_stack.push(lhs.deref() - rhs.deref());
}

inline void DIV::emit(VM& vm) {
    Value rhs = vm.op_stack.popValue();
    Value lhs = vm.op_stack.popValue();
    vm.op_stack.push(lhs.deref() / rhs.deref());
}

inline void NEG::emit(VM& vm) {
    auto value = vm.op_stack.popValue();
    vm.op_stack.push(-value.deref());
}

inline void DEREF::emit(VM& vm) {
    const Value ref = vm.op_stack.popValue();
    vm.op_stack.push(Value(ref.deref()));
}

inline void ADDR_OF::emit(VM& vm) const {
    const Value ref_slot = vm.op_stack.popValue();
    if (!ref_slot.isReference()) {
        VM_RUNTIME_ERROR(vm, "address-of requires an lvalue");
    }
    vm.op_stack.push(Value::makeAddressRef(ref_slot.asReference().value_ptr));
}

inline void DEREF_PTR::emit(VM& vm) const {
    const Value ptr_val = vm.op_stack.popValue();
    const Value& slot = ptr_val.deref();
    if (!slot.isAddressRef()) {
        VM_RUNTIME_ERROR(vm, "dereference requires a pointer");
    }
    vm.op_stack.push(Value(*slot.asReference().value_ptr));
}

inline void PTR_TO_REF::emit(VM& vm) const {
    const Value ptr_val = vm.op_stack.popValue();
    const Value& slot = ptr_val.deref();
    if (!slot.isAddressRef()) {
        VM_RUNTIME_ERROR(vm, "dereference requires a pointer");
    }
    vm.op_stack.push(Value::makeRef(slot.asReference().value_ptr, vm.cell_pool));
}

inline void NOT::emit(VM& vm) {
    auto value = vm.op_stack.popValue();
    vm.op_stack.push(!value.deref());
}

inline void AND::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(a.deref() && b.deref());
}

inline void OR::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(a.deref() || b.deref());
}

inline void EQ::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(Value(a.deref() == b.deref()));
}

inline void NEQ::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(Value(a.deref() != b.deref()));
}

inline void LT::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(a.deref() < b.deref());
}

inline void LTE::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(a.deref() <= b.deref());
}

inline void GT::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(a.deref() > b.deref());
}

inline void GTE::emit(VM& vm) {
    auto b = vm.op_stack.popValue();
    auto a = vm.op_stack.popValue();
    vm.op_stack.push(a.deref() >= b.deref());
}

std::string SymbolTable::toString() const {
    std::string k;
    for (const auto& [a, b] : *this) {
        k.append(std::format("{} : {}({}), \n", g_string_pool.get_string(a), b->toString(), b->type_name()));
    }
    return k.substr(0, k.size() - 3);
}

inline void STORE::emit(VM& vm) const {
    // 栈顶为引用槽（NEW_VAR / NEW_VAR_OR_LOAD 压入），次栈顶为待写入的值
    Value ref_slot = vm.op_stack.popValue();
    Value data = vm.op_stack.popValue();

    if (ref_slot.isReference()) {
        bool has_value = false;
        bool is_const = false;
        const auto& ref_ptr = ref_slot.asReference();
        if (ref_ptr.value_ptr && !vm.symbol_stack.empty()) {
            for (auto& symbol_table : vm.symbol_stack) {
                for (const auto& [id, val] : symbol_table.symbols) {
                    if (val.get() == ref_ptr.value_ptr.get()) {
                        is_const = symbol_table.is_constant(id);
                        has_value = val->getType() != Value::Type::None;
                        break;
                    }
                }
            }
        }

        if (is_const && has_value) {
            VM_RUNTIME_ERROR(vm,"Cannot modify constant");
        }

        ref_slot.set(data);
    }

    LOG("Done, now symbol: " << vm.symbol_stack.back().toString());
}

inline void LOAD::emit(VM& vm) const {
    const std::string& var_name = g_string_pool.get_string(var_id);

    LOG("LOAD: var_id=" << var_id << ", var_name=\"" << var_name << "\"");
    LOG("LOAD: symbol_stack size=" << vm.symbol_stack.size());

    // 1. 先从 Cache 查找
    LOG("Finding in Cache");
    const auto cached = vm.cache.get(var_id);
    if (cached.has_value()) {
        const auto& value_ptr = *cached;
        LOG("Found in Cache");
        if (value_ptr->isReference()) {
            vm.op_stack.push(*value_ptr);
        } else {
            vm.op_stack.push(Value::makeRef(value_ptr, vm.cell_pool));
        }
        return;
    }

    std::shared_ptr<Value> value_ptr;

    // 2. 从符号栈查找（包括闭包作用域）
    LOG("Finding in Symbol Stack (including closures)");
    for (size_t i = vm.symbol_stack.size(); i > 0; i--) {
        const auto& symbol_table = vm.symbol_stack[i - 1];
        LOG("  Scope " << (vm.symbol_stack.size() - i) << ": " << symbol_table.toString());
        if (symbol_table.exists(var_id)) {
            auto k = symbol_table.get(var_id);
            if (k.has_value()) {
                value_ptr = *k;
                vm.cache.add(var_id, value_ptr);
                LOG("  Found in scope " << (vm.symbol_stack.size() - i));
                break;
            }
        }
    }

    // 3. 符号栈未找到，从模块查找
    if (!value_ptr) {
        LOG("Finding in Module");
        if (vm.main_module) {
            auto attr = vm.main_module->get_attr(var_name);
            if (attr.has_value()) {
                value_ptr = vm.cell_pool.allocateCopy(*attr);
                vm.cache.add(var_id, value_ptr);
                LOG("Found in Module");
            }
        }
    }

    // 4. 仍未找到，报错
    if (!value_ptr) {
        LOG("Still not found");
        VM_RUNTIME_ERROR(vm,"Var not found: " + var_name);
    }

    // 5. 压栈
    if (value_ptr->isReference()) {
        vm.op_stack.push(*value_ptr);
    } else {
        vm.op_stack.push(Value::makeRef(value_ptr, vm.cell_pool));
    }
}

inline void LABEL::emit(VM&) {
}

inline void GOTO::emit(VM& vm) const {
    if (not vm.label_table.contains(label_id)) {
        VM_RUNTIME_ERROR(vm,"Unknown label: " + std::to_string(label_id));
    }
    size_t target_pc = vm.label_table[label_id];
    LOG(
        "GOTO: label_id=" << label_id << ", target_pc=" << target_pc << ", symbol_stack size=" << vm.symbol_stack.size()
    );
    vm.pc = target_pc;
}

inline void GOTOIF::emit(VM& vm) const {
    if (vm.op_stack.popValue().asBool()) {
        if (not vm.label_table.contains(label_id)) {
            VM_RUNTIME_ERROR(vm,"Unknown label: " + std::to_string(label_id));
        }
        vm.pc = vm.label_table[label_id];
    }
}

inline void GOTOIFNOT::emit(VM& vm) const {
    if (!vm.op_stack.popValue().asBool()) {
        if (not vm.label_table.contains(label_id)) {
            VM_RUNTIME_ERROR(vm,"Unknown label: " + std::to_string(label_id));
        }
        vm.pc = vm.label_table[label_id];
    }
}

inline void ENTER_SCOPE::emit(VM& vm) {
    LOG("ENTER_SCOPE: symbol_stack before=" << vm.symbol_stack.size());
    vm.symbol_stack.emplace_back();
    vm.locals_stack.emplace_back();
    vm.cache.enter_scope();
    LOG("ENTER_SCOPE: symbol_stack after=" << vm.symbol_stack.size());
}

inline void LEAVE_SCOPE::emit(VM& vm) {
    LOG("LEAVE_SCOPE: symbol_stack before=" << vm.symbol_stack.size());
    if (!vm.symbol_stack.empty()) {
        vm.symbol_stack.pop_back();
    }
    if (!vm.locals_stack.empty()) {
        vm.locals_stack.pop_back();
    }
    vm.cache.leave_scope();
    LOG("LEAVE_SCOPE: symbol_stack after=" << vm.symbol_stack.size());
}

namespace {

Value eval_param_default(VM& vm, const std::vector<Opcode>& ir) {
    const size_t stack_depth = vm.op_stack.size();
    for (auto op : ir) {
        std::visit([&](auto& opcode) { opcode.emit(vm); }, op);
    }
    if (vm.op_stack.size() <= stack_depth) {
        throw RuntimeError("parameter default did not produce a value");
    }
    return vm.op_stack.popValue();
}

std::vector<Value> pop_positional_args(VM& vm, const size_t count) {
    std::vector<Value> positional;
    positional.reserve(count);
    for (size_t i = count; i > 0; --i) {
        positional.push_back(vm.op_stack.popValue());
    }
    std::ranges::reverse(positional);
    return positional;
}

Value pop_kwargs_dict(VM& vm, const bool has_kwargs) {
    if (!has_kwargs) {
        return Value(std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>{});
    }
    return vm.op_stack.popValue();
}

std::vector<Value> resolve_user_function_args(
    VM& vm,
    const FunctionObject& func_obj,
    const std::vector<Value>& positional,
    const Value& kwargs_value
) {
    const size_t param_count = func_obj.params.size();
    if (func_obj.param_default_ir.size() != param_count) {
        VM_RUNTIME_ERROR(vm, "internal error: parameter metadata mismatch");
    }

    std::vector<Value> resolved(param_count);
    std::vector<bool> filled(param_count, false);

    if (positional.size() > param_count) {
        VM_RUNTIME_ERROR(
            vm,
            std::format(
                "function {} expects at most {} argument(s), got {} positional",
                func_obj.name,
                param_count,
                positional.size()
            )
        );
    }

    for (size_t i = 0; i < positional.size(); ++i) {
        resolved[i] = positional[i];
        filled[i] = true;
    }

    const Value& kwargs = kwargs_value.deref();
    if (kwargs.getType() != Value::Type::Dictionary) {
        VM_RUNTIME_ERROR(vm, "internal error: keyword arguments must be a dictionary");
    }

    for (const auto& [key_ptr, value_ptr] : kwargs.asDictionary()) {
        const Value& key = key_ptr->deref();
        if (key.getType() != Value::Type::String) {
            VM_RUNTIME_ERROR(vm, "keyword argument names must be strings");
        }
        const std::string& kw_name = key.asString();
        auto it = std::ranges::find(func_obj.params, kw_name);
        if (it == func_obj.params.end()) {
            VM_RUNTIME_ERROR(
                vm,
                std::format("function {} has no parameter named '{}'", func_obj.name, kw_name)
            );
        }
        const size_t index = static_cast<size_t>(std::distance(func_obj.params.begin(), it));
        if (filled[index]) {
            VM_RUNTIME_ERROR(
                vm,
                std::format("multiple values for argument '{}'", kw_name)
            );
        }
        resolved[index] = *value_ptr;
        filled[index] = true;
    }

    for (size_t i = 0; i < param_count; ++i) {
        if (filled[i]) {
            continue;
        }
        if (func_obj.param_default_ir[i].empty()) {
            VM_RUNTIME_ERROR(
                vm,
                std::format(
                    "function {} missing required argument '{}'",
                    func_obj.name,
                    func_obj.params[i]
                )
            );
        }
        resolved[i] = eval_param_default(vm, func_obj.param_default_ir[i]);
        filled[i] = true;
    }

    return resolved;
}

void invoke_user_function_with_args(VM& vm, const std::shared_ptr<FunctionObject>& func_obj, std::vector<Value> args) {
    if (!func_obj->owner_vm) {
        func_obj->owner_vm = &vm;
    }

    if (func_obj->owner_vm != &vm) {
        const Value result = func_obj->call(vm, args);
        vm.op_stack.push(result);
        return;
    }

    vm.call_func_stack.push_back(func_obj);
    vm.call_stack.push_back(vm.pc);

    for (auto& arg : args) {
        vm.op_stack.push(std::move(arg));
    }

    if (!func_obj->closure.empty()) {
        for (const auto& scope : func_obj->closure) {
            vm.symbol_stack.push_back(scope);
        }
    }

    const auto label_it = vm.label_table.find(func_obj->location);
    if (label_it == vm.label_table.end()) {
        VM_RUNTIME_ERROR(vm, "Function label not found: " + std::to_string(func_obj->location));
    }
    vm.pc = label_it->second;
}

} // namespace

inline void CALL::emit(VM& vm) const {
    Value func = vm.op_stack.popValue();
    LOG(ITIS(,func,.toString()) << ", true val type: " << func.deref().type_name());

    if (!func.isFunction()) {
        VM_RUNTIME_ERROR(vm,"Not a function");
    }

    if (func.isUserFunction()) {
        auto func_obj = func.asFunctionObject();
        const Value kwargs = pop_kwargs_dict(vm, has_kwargs);
        std::vector<Value> positional = pop_positional_args(vm, arg_count);
        std::vector<Value> resolved = resolve_user_function_args(vm, *func_obj, positional, kwargs);
        invoke_user_function_with_args(vm, func_obj, std::move(resolved));
        return;
    }

    if (has_kwargs) {
        VM_RUNTIME_ERROR(vm, "keyword arguments are not supported for builtin functions");
    }

    std::vector<Value> args;
    args.reserve(arg_count);
    for (ptrdiff_t i = 0; i < static_cast<ptrdiff_t>(arg_count); ++i) {
        args.emplace_back(vm.op_stack.popValue());
    }

    auto builtin_func = func.asFunction();
    auto result = builtin_func(vm, args);
    vm.op_stack.push(result);
}

inline void RET::emit(VM& vm) {
    LOG("RET: call_stack size=" << vm.call_stack.size() << ", call_func_stack size=" << vm.call_func_stack.size());
    if (!vm.call_stack.empty()) {
        vm.pc = vm.call_stack.back();
        vm.call_stack.pop_back();
        if (!vm.call_func_stack.empty()) {
            const auto& func = vm.call_func_stack.back();
            if (!func->closure.empty()) {
                for (size_t i = 0; i < func->closure.size(); i++) {
                    if (!vm.symbol_stack.empty()) {
                        vm.symbol_stack.pop_back();
                    }
                }
            }
            vm.call_func_stack.pop_back();
        }
    } else {
        VM_RUNTIME_ERROR(vm,"RET when call stack is empty");
    }
}

inline void FINDMOD::emit(VM& vm) const {
    const std::string& module_name = g_string_pool.get_string(module_id);

    if (module_name.empty()) {
        VM_RUNTIME_ERROR(vm,"Empty module name");
    }

    Value result = vm.main_module->import(module_name);
    vm.op_stack.push(result);
}


inline void GETATTR::emit(VM& vm) const {
    const std::string& attr_name = g_string_pool.get_string(name_id);
    Value receiver = vm.op_stack.popValue();

    const auto push_module_attr = [&](const std::shared_ptr<ModuleObject>& module) {
        const auto result = module->get_attr(attr_name);
        if (result.has_value()) {
            vm.op_stack.push(Value::makeRef(vm.cell_pool.allocateCopy(*result), vm.cell_pool));
            return;
        }
        VM_RUNTIME_ERROR(vm, "Attribute not found: " + attr_name + ", in module: " + module->name);
    };

    if (receiver.isReference() && !receiver.asReference().opaque) {
        const std::shared_ptr<Value> cell = receiver.asReference().value_ptr;
        const Value& val = cell->deref();

        if (auto method = lang::bind_method(cell, attr_name)) {
            vm.op_stack.push(Value(*method));
            return;
        }
        if (val.getType() == Value::Type::StructObject) {
            if (auto method = struct_try_bind_method(vm, Value(*cell), attr_name)) {
                vm.op_stack.push(*method);
                return;
            }
            vm.op_stack.push(struct_get_field(Value(*cell), attr_name));
            return;
        }
        if (val.getType() == Value::Type::Module) {
            push_module_attr(val.asModule());
            return;
        }
        VM_RUNTIME_ERROR(
            vm,
            std::format("Attribute '{}' not found on {}", attr_name, val.type_name())
        );
    }

    const Value& val = receiver.deref();
    if (val.getType() == Value::Type::StructObject) {
        if (auto method = struct_try_bind_method(vm, receiver, attr_name)) {
            vm.op_stack.push(*method);
            return;
        }
        vm.op_stack.push(struct_get_field(receiver, attr_name));
        return;
    }
    if (val.getType() == Value::Type::Module) {
        push_module_attr(val.asModule());
        return;
    }

    const auto temp = vm.cell_pool.allocateCopy(val);
    if (auto method = lang::bind_method(temp, attr_name)) {
        vm.op_stack.push(Value(*method));
        return;
    }

    VM_RUNTIME_ERROR(
        vm,
        std::format("Attribute '{}' not found on {}", attr_name, val.type_name())
    );
}

void STRUCT_NEW::emit(VM& vm) const {
    const std::string& struct_name = g_string_pool.get_string(struct_id);

    std::vector<Value> args;
    args.reserve(arg_count);
    for (size_t i = 0; i < arg_count; ++i) {
        args.emplace_back(vm.op_stack.popValue());
    }
    std::ranges::reverse(args);

    vm.op_stack.push(make_struct_instance(struct_name, std::move(args)));
}

void SET_FIELD::emit(VM& vm) const {
    const std::string& field_name = g_string_pool.get_string(name_id);
    Value obj = vm.op_stack.popValue();
    Value value = vm.op_stack.popValue();
    struct_set_field(obj, field_name, value);
    vm.op_stack.push(obj);
}

inline void IS_VECTOR::emit(VM& vm) const {
    const Value value = vm.op_stack.popValue();
    vm.op_stack.push(Value(value.deref().isVector()));
}

inline void VEC_LEN::emit(VM& vm) const {
    const Value value = vm.op_stack.popValue();
    const Value& vec_val = value.deref();
    if (!vec_val.isVector()) {
        VM_RUNTIME_ERROR(vm, "VEC_LEN requires a vector");
    }
    vm.op_stack.push(Value(static_cast<int64_t>(vec_val.asVector().size())));
}

namespace {
bool match_values_equal(const Value& a, const Value& b) {
    const Value& left = a.deref();
    const Value& right = b.deref();
    if (left.isNumber() && right.isNumber()) {
        return left.asNumber() == right.asNumber();
    }
    if (left.getType() == Value::Type::Bool && right.getType() == Value::Type::Bool) {
        return left.asBool() == right.asBool();
    }
    if (left.getType() == Value::Type::String && right.getType() == Value::Type::String) {
        return left.asString() == right.asString();
    }
    return false;
}
} // namespace

inline void MATCH_EQ::emit(VM& vm) const {
    const Value b = vm.op_stack.popValue();
    const Value a = vm.op_stack.popValue();
    vm.op_stack.push(Value(match_values_equal(a, b)));
}

inline void VEC_NEW::emit(VM& vm) const {
    const auto element_count = count;
    std::vector<std::shared_ptr<Value>> elements;
    elements.reserve(element_count);
    const VmGcSuppress gc_guard{vm};

    for (size_t i = 0; i < element_count; ++i) {
        elements.push_back(vm.cell_pool.allocateCopy(vm.op_stack.popValue()));
    }

    vm.op_stack.push(Value(std::move(elements)));
}

inline void DICT_NEW::emit(VM& vm) const {
    auto entry_count = count;
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> dict;
    const VmGcSuppress gc_guard{vm};

    for (size_t i = 0; i < entry_count; ++i) {
        Value value = vm.op_stack.popValue();
        Value key_val = vm.op_stack.popValue();
        dict[vm.cell_pool.allocateCopy(key_val)] = vm.cell_pool.allocateCopy(value);
    }

    vm.op_stack.push(Value(std::move(dict)));
}

inline void INDEX::emit(VM& vm) const {
    Value index_val = vm.op_stack.popValue();
    Value obj = vm.op_stack.popValue();

    // 解引用对象
    Value& obj_deref = obj.deref();

    if (obj_deref.isVector()) {
        const Value& idx_deref = index_val.deref();
        ptrdiff_t idx = idx_deref.asInt();

        auto& vec = obj_deref.asVector();
        if (idx < 0 or static_cast<size_t>(idx) >= vec.size()) {
            VM_RUNTIME_ERROR(vm,"Index out of range");
        }

        vm.op_stack.push(Value::makeRef(vec[static_cast<size_t>(idx)], vm.cell_pool));
    } else if (obj_deref.isDictionary()) {
        const auto& key = index_val.deref();

        auto& dict = obj_deref.asDictionary();
        for (const auto& [tkey, tval] : dict) {
            if (*tkey == key) {
                vm.op_stack.push(Value::makeRef(tval, vm.cell_pool));
                return;
            }
        }
        VM_RUNTIME_ERROR(vm,"key " + key.toString() + " not found");
    } else {
        VM_RUNTIME_ERROR(vm,"INDEX requires a vector or dictionary object");
    }
}

inline void STORE_ARG::emit(VM& vm) const {
    const Value value = vm.op_stack.popValue();

    const auto value_ptr = vm.cell_pool.allocateCopy(value);
    vm.symbol_stack.back().set(var_id, value_ptr);
    vm.cache.add(var_id, value_ptr);
    LOG("STORE_ARG Done, now " << ITIS(,vm.symbol_stack.back(),.toString()));
}

void NEW_VAR::emit(VM& vm) const {
    Value var = Value::makeEmptyRef(vm.cell_pool);

    if (!vm.symbol_stack.empty()) {
        vm.symbol_stack.back().set(var_id, var.getRefValuePtr());
        vm.symbol_stack.back().set_constant(var_id, false);
        vm.cache.add(var_id, var.getRefValuePtr());
    }
    vm.op_stack.push(var);
}

void NEW_CONST::emit(VM& vm) const {
    Value var = Value::makeEmptyRef(vm.cell_pool);

    if (!vm.symbol_stack.empty()) {
        vm.symbol_stack.back().set(var_id, var.getRefValuePtr());
        vm.symbol_stack.back().set_constant(var_id, true);
        vm.cache.add(var_id, var.getRefValuePtr());
    }
    vm.op_stack.push(var);
}

void NEW_INTERN_VAR::emit(VM& vm) const {
    Value var = Value::makeEmptyRef(vm.cell_pool);

    if (!vm.symbol_stack.empty()) {
        vm.symbol_stack.back().set(var_id, var.getRefValuePtr());
        vm.symbol_stack.back().set_constant(var_id, false);
        vm.cache.add(var_id, var.getRefValuePtr());
    }
    vm.op_stack.push(var);
}

void NEW_INTERN_CONST::emit(VM& vm) const {
    Value var = Value::makeEmptyRef(vm.cell_pool);

    if (!vm.symbol_stack.empty()) {
        vm.symbol_stack.back().set(var_id, var.getRefValuePtr());
        vm.symbol_stack.back().set_constant(var_id, true);
        vm.cache.add(var_id, var.getRefValuePtr());
    }
    vm.op_stack.push(var);
}

void NEW_VAR_OR_LOAD::emit(VM& vm) const {
#ifdef DEBUG
    const std::string& var_name = g_string_pool.get_string(var_id);
#endif

    LOG("NEW_VAR_OR_LOAD: var_id=" << var_id << ", var_name=\"" << var_name << "\"");

    const auto cached = vm.cache.get(var_id);
    if (cached.has_value()) {
        const auto& value_ptr = *cached;
        LOG("NEW_VAR_OR_LOAD: Found in Cache, pushing ref");
        if (value_ptr->isReference()) {
            vm.op_stack.push(*value_ptr);
        } else {
            vm.op_stack.push(Value::makeRef(value_ptr, vm.cell_pool));
        }
        return;
    }

    if (!vm.symbol_stack.empty()) {
        const auto found = vm.symbol_stack.back().get(var_id);
        if (found.has_value()) {
            LOG("NEW_VAR_OR_LOAD: Found in symbol stack");
            vm.op_stack.push(Value::makeRef(*found, vm.cell_pool));
            vm.cache.add(var_id, *found);
            return;
        }
    }

    LOG("NEW_VAR_OR_LOAD: Variable not found, creating new");
    Value var = Value::makeEmptyRef(vm.cell_pool);
    if (!vm.symbol_stack.empty()) {
        vm.symbol_stack.back().set(var_id, var.getRefValuePtr());
        vm.symbol_stack.back().set_constant(var_id, false);
        vm.cache.add(var_id, var.getRefValuePtr());
    }
    vm.op_stack.push(var);
}

void RET_THEN_LEAVE_SCOPE::emit(VM& vm) const {
    RET().emit(vm);
    LEAVE_SCOPE().emit(vm);
}

inline void LOAD_FAST::emit(VM& vm) const {
    LOG("LOAD_FAST: slot_index=" << slot);

    if (vm.locals_stack.empty()) {
        VM_RUNTIME_ERROR(vm,"LOAD_FAST: No locals scope available");
    }

    const auto& locals = vm.locals_stack.back();
    if (slot >= locals.size()) {
        VM_RUNTIME_ERROR(vm,"LOAD_FAST: slot index out of range: " + std::to_string(slot));
    }

    vm.op_stack.push(locals[slot]);
}

inline void STORE_FAST::emit(VM& vm) const {
    LOG("STORE_FAST: slot_index=" << slot);

    if (vm.locals_stack.empty()) {
        VM_RUNTIME_ERROR(vm,"STORE_FAST: No locals scope available");
    }

    Value value = vm.op_stack.popValue();
    auto& locals = vm.locals_stack.back();

    if (slot >= locals.size()) {
        locals.resize(slot + 1);
    }
    locals[slot] = value;
}

void BIND_FAST::emit(VM& vm) const {
    if (!vm.call_func_stack.empty()) {
        const auto& func = vm.call_func_stack.back();
        if (!func->needs_closure && !func->needs_symbol_bind) {
            return;
        }
    }

    if (vm.locals_stack.empty() || vm.symbol_stack.empty()) {
        VM_RUNTIME_ERROR(vm,"BIND_FAST: No scope available");
    }

    auto& locals = vm.locals_stack.back();
    if (slot >= locals.size()) {
        VM_RUNTIME_ERROR(vm,"BIND_FAST: slot index out of range");
    }

    Value& local = locals[slot];
    std::shared_ptr<Value> value_ptr;
    if (local.isReference()) {
        value_ptr = local.asReference().value_ptr;
    } else {
        value_ptr = vm.cell_pool.allocateCopy(local);
        local = Value::makeRef(value_ptr, vm.cell_pool);
    }

    vm.symbol_stack.back().set(var_id, value_ptr);
    vm.symbol_stack.back().set_constant(var_id, false);
    vm.cache.add(var_id, value_ptr);
}

namespace {

std::vector<std::string> split_module_path(const std::string& module_name) {
    std::vector<std::string> path_components;
    std::string current_component;
    for (const char c : module_name) {
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
    return path_components;
}

std::optional<Value> resolve_module_path(
    const ModuleObject* root,
    const std::vector<std::string>& path_components
) {
    if (path_components.empty() || root == nullptr) {
        return std::nullopt;
    }

    std::optional<Value> current = root->get_attr(path_components.front());
    if (!current.has_value()) {
        return std::nullopt;
    }

    for (size_t i = 1; i < path_components.size(); ++i) {
        const Value& value = current->deref();
        if (value.getType() != Value::Type::Module) {
            return std::nullopt;
        }
        const auto mod = value.asModule();
        const auto next = mod->get_attr(path_components[i]);
        if (!next.has_value()) {
            return std::nullopt;
        }
        current = next;
    }

    return current;
}

} // namespace

Value ModuleObject::import(const std::string& module_name) {
    const auto path_components = split_module_path(module_name);
    if (path_components.empty()) {
        throw RuntimeError("Invalid module name: " + module_name);
    }

    if (const auto resolved = resolve_module_path(this, path_components)) {
        if (resolved->deref().getType() == Value::Type::Module) {
            return *resolved;
        }
    }

    const std::string& last_component = path_components.back();

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

        std::filesystem::path file_path = module_path / (last_component + ".lm");
        std::filesystem::path dir_path = module_path / last_component / "main.lm";

        if (std::filesystem::exists(file_path)) {
            std::ifstream file(file_path);
            if (file.is_open()) {
                std::string code_str(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
                auto module_obj = std::make_shared<ModuleObject>(code_str);
                module_obj->name = last_component;
                module_obj->full_name = this->full_name.empty() ? module_name : this->full_name + "." + module_name;
                set_attr(last_component, Value(module_obj));
                return Value(module_obj);
            }
        } else if (std::filesystem::exists(dir_path)) {
            std::ifstream file(dir_path);
            if (file.is_open()) {
                std::string code_str(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
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


inline void LABEL::set_label(VM& vm, const std::optional<size_t> on) const {
    vm.label_table[label_id] = on.value_or(vm.pc);
}

Value FunctionObject::call(VM& caller_vm, const std::vector<Value>& args) {
    if (!owner_vm) {
        VM_RUNTIME_ERROR(caller_vm, "Function has no owner VM");
    }

    const Value empty_kwargs(
        std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>{}
    );
    const std::vector<Value> resolved = resolve_user_function_args(caller_vm, *this, args, empty_kwargs);

    VM& target_vm = *owner_vm;

    size_t old_pc = target_vm.pc;

    for (const auto& arg : resolved) {
        target_vm.op_stack.push(arg);
    }

    target_vm.call_stack.push_back(target_vm.code.size());

    for (const auto& scope : closure) {
        target_vm.symbol_stack.push_back(scope);
    }

    if (target_vm.label_table.contains(location)) {
        target_vm.pc = target_vm.label_table[location];
    } else {
        VM_RUNTIME_ERROR(target_vm, "Function label not found: " + std::to_string(location));
    }

    target_vm.run();

    for (size_t i = 0; i < closure.size(); i++) {
        if (!target_vm.symbol_stack.empty()) {
            target_vm.symbol_stack.pop_back();
        }
    }

    if (!target_vm.call_stack.empty()) {
        target_vm.call_stack.pop_back();
    }

    target_vm.pc = old_pc;

    if (!target_vm.op_stack.empty()) {
        Value result = target_vm.op_stack.popValue();
        target_vm.op_stack.clear();
        return result;
    }
    return {};
}

template<StringType string>
ModuleObject::ModuleObject(string code) : is_user(true) {
    const auto codes = lm::irgen::Generator(parse(code)).gen();
    owner_vm = new VM(codes);
    owner_vm->run();

    for (const auto& [expname, value] : owner_vm->main_module->exports) {
        if (value.isUserFunction()) {
            value.asFunctionObject()->owner_vm = owner_vm;
        }
        exports[expname] = value;
    }

    for (const auto& [modname, submod] : owner_vm->main_module->submodules) {
        submodules[modname] = submod;
    }
}
}

bool irgen::IteratorObject::next(Value& out) {
    Value& iterable_ref = iterable->deref();

    if (iterable_ref.isVector()) {
        auto& vec = iterable_ref.asVector();
        if (index < vec.size()) {
            if (cell_pool == nullptr) {
                throw RuntimeError("iterator has no cell pool");
            }
            out = Value::makeRef(vec[index++], *cell_pool);
            return true;
        }
    } else if (iterable_ref.isString()) {
        const std::string& str = iterable_ref.asString();
        if (index < str.size()) {
            out = Value(std::string(1, str[index++]));
            return true;
        }
    } else if (iterable_ref.isDictionary()) {
        auto& dict = iterable_ref.asDictionary();
        size_t i = 0;
        for (const auto& key : dict | std::views::keys) {
            if (i == index) {
                if (cell_pool == nullptr) {
                    throw RuntimeError("iterator has no cell pool");
                }
                out = Value::makeRef(key, *cell_pool);
                index++;
                return true;
            }
            i++;
        }
    }

    return false;
}

inline void irgen::ITER_NEW::emit(VM& vm) const {
    Value iterable = vm.op_stack.popValue();
    auto iter_obj = std::make_shared<IteratorObject>(vm.cell_pool.allocateValue(std::move(iterable)));
    iter_obj->cell_pool = &vm.cell_pool;
    vm.op_stack.push(Value(iter_obj));
}

inline void irgen::ITER_NEXT::emit(VM& vm) const {
    Value iter_val = vm.op_stack.popValue();
    Value& iter_deref = iter_val.deref();

    if (iter_deref.getType() != Value::Type::Iterator) {
        VM_RUNTIME_ERROR(vm,"ITER_NEXT requires an iterator object");
    }

    auto iter = iter_deref.asIterator();
    Value result;
    bool has_next = iter->next(result);

    vm.op_stack.push(iter_val);
    vm.op_stack.push(result);
    vm.op_stack.push(Value(has_next));
}

inline void irgen::ITER_END::emit(VM& vm) const {
    vm.op_stack.pop();
}

