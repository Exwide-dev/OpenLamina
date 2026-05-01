#include "opcode.hpp"
#include "../tools/lang/builtins.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>

namespace irgen {
    std::string Value::toString() const {
        switch (type) {
            case Type::None: return "None";
            case Type::Int: return asNumber().toString();
            case Type::Bool: return (asBool() ? "true" : "false");
            case Type::String: return "\"" + asString() + "\"";
            case Type::Function: return std::format("<function at 0x{:x}>", reinterpret_cast<uintptr_t>(this));
            default: return "<__UNKNOWN_ValueType>";
        }
    }

    Value SymbolTable::get(const size_t id) const {
        const auto it = symbols.find(id);
        if (it == symbols.end()) {
            throw RuntimeError("Variable not found with id: " + std::to_string(id));
        }
        return it->second;
    }

    void SymbolTable::set(const size_t id, const Value &value) {
        symbols[id] = value;
    }

    // 初始化内置函数
    void VM::init_builtins() {
        lang::init_builtins(symbols);
    }
}

