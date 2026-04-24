#pragma once
#include <format>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace irgen {
    class VM;


    // Value 类型，支持多种类型的值
    class Value {
    public:
        enum class Type {
            None,
            Int,
            Bool,
            String,
            Function
        };

    private:
        Type type;
        std::variant<int, bool, std::string, void*> data;

    public:
        Value() : type(Type::None) {}
        explicit Value(int value) : type(Type::Int), data(value) {}
        explicit Value(bool value) : type(Type::Bool), data(value) {}
        explicit Value(const std::string& value) : type(Type::String), data(value) {}
        explicit Value(void* value) : type(Type::Function), data(value) {}

        [[nodiscard]] Type getType() const { return type; }

        [[nodiscard]] std::string toString() const {
            switch (type) {
                case Type::None:
                    return "None";
                case Type::Int:
                    return std::to_string(asInt());
                case Type::Bool:
                    return (asBool() ? "true" : "false");
                case Type::String:
                    return "\"" + asString() + "\"";
                case Type::Function:
                    return "<function>";
                default:
                    return "<__UNKNOWN_ValueType>";
            }
        };

#define DEFINE_AS_METHOD(FieldName, EnumValue, CppType, ErrorMsg) \
[[nodiscard]] CppType as##FieldName() const { \
    if (type != Type::EnumValue) { \
        throw std::runtime_error("Value is not " ErrorMsg); \
    } \
    return std::get<CppType>(data); \
}

        DEFINE_AS_METHOD(Int, Int, int, "an integer")
        DEFINE_AS_METHOD(Bool, Bool, bool, "a boolean")
        DEFINE_AS_METHOD(String, String, std::string, "a string")
        DEFINE_AS_METHOD(Function, Function, void*, "a function")

#undef DEFINE_AS_METHOD

        [[nodiscard]] bool isNone() const { return type == Type::None; }
        [[nodiscard]] bool isInt() const { return type == Type::Int; }
        [[nodiscard]] bool isBool() const { return type == Type::Bool; }
        [[nodiscard]] bool isString() const { return type == Type::String; }
        [[nodiscard]] bool isFunction() const { return type == Type::Function; }

        friend std::ostream& operator<<(std::ostream& os, const Value& value) {
            std::cerr << "Access <<" << std::endl;
            os << value.toString();
            return os;
        }
    };

    // 符号表，用于存储变量和函数
    class SymbolTable {
        std::unordered_map<std::string, Value> symbols;

    public:
        SymbolTable() = default;

        void set(const std::string& name, const Value& value) {
            symbols[name] = value;
        }

        Value get(const std::string& name) const {
            const auto it = symbols.find(name);
            if (it == symbols.end()) {
                throw std::runtime_error("Variable not found: " + name);
            }
            return it->second;
        }

        bool exists(const std::string& name) const {
            return symbols.contains(name);
        }
    };

    class Stack {
        std::vector<Value> data;

    public:
        void push(const Value& value) {
            data.push_back(value);
        }

        void push(Value&& value) {
            std::cerr << "Pushing back: " << value.toString() << std::endl;
            data.push_back(std::move(value));
        }

        void pop() {
            if (data.empty()) {
                throw std::out_of_range("Stack::pop(): stack is empty");
            }
            data.pop_back();
        }

        Value popValue() {
            if (data.empty()) {
                throw std::out_of_range("Stack::popValue(): stack is empty");
            }
            Value value = data.back();
            data.pop_back();
            return value;
        }

        Value &top() {
            if (data.empty()) {
                throw std::out_of_range("Stack::top(): stack is empty");
            }
            return data.back();
        }

        [[nodiscard]] const Value &top() const {
            if (data.empty()) {
                throw std::out_of_range("Stack::top(): stack is empty");
            }
            return data.back();
        }

        [[nodiscard]] size_t size() const {
            return data.size();
        }

        [[nodiscard]] bool empty() const {
            return data.empty();
        }

        void clear() {
            data.clear();
        }

        auto begin() { return data.begin(); }
        auto end() { return data.end(); }
        [[nodiscard]] auto begin() const { return data.begin(); }
        [[nodiscard]] auto end() const { return data.end(); }

        [[nodiscard]] std::string toString() const {
            return "Stack:\n" + std::accumulate(
                data.begin(), data.end(),
                std::string{},
                [i = 0](std::string acc, const auto& elem) mutable {
                    return std::move(acc) + std::format("{} | {}\n", i++, elem.toString());
                }
            );
        }
    };

    class Register {
    public:
        std::vector<Value> data;
        explicit Register(std::vector<Value> const &data = std::vector<Value>())
            : data(data) {}

        Value operator[](const size_t index) {
            if (index >= data.size()) {
                std::stringstream ss;
                ss << "Register[]: index out of range, size=" << data.size() << ", try to access " << index;
                throw std::out_of_range(ss.str());
            }
            return data[index];
        }
    };

    class Opcode {
    public:
        virtual ~Opcode() = default;
        [[nodiscard]] virtual std::string name() const = 0;
        std::vector<Value> operands;

        Value& operator[](const size_t index) {
            return operands[index];
        }

        const Value& operator[](const size_t index) const {
            return operands[index];
        }

        virtual void emit(VM& vm) = 0;
    };

    class VM {
    public:
        Stack op_stack, call_stack;
        std::vector<Opcode*> code;
        Register reg;
        SymbolTable symbols;
        size_t pc = 0;

        VM() = default;
        explicit VM(std::vector<Opcode*> c) : code(std::move(c)) {}

        void run() {
            for (;pc < code.size(); pc++) {
                std::cerr << op_stack.toString() << std::endl;
                code[pc]->emit(*this);
            }
        }
    };

    class PUSH final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "PUSH";
        }
        explicit PUSH(const Value& v) {
            operands.push_back(v);
        }

        void emit(VM& vm) override {
            std::cerr << "PUSH " << operands[0].toString() << std::endl;
            vm.op_stack.push(operands[0]);
        }
    };

    class ADD final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "ADD";
        }
        ADD() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            std::cerr << "ADD OK" << std::endl;
            vm.op_stack.push(Value(a + b));
        }
    };

    class MUL final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "MUL";
        }
        MUL() = default;

        void emit(VM& vm) override {
            std::cerr << "MUL" << std::endl;
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            std::cerr << "OK" << std::endl;
            vm.op_stack.push(Value(a * b));
            std::cerr << "DONE" << std::endl;
        }
    };

    class SUB final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "SUB";
        }
        SUB() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a - b));
        }
    };

    class DIV final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "DIV";
        }
        DIV() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a / b));
        }
    };

    class NEG final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "NEG";
        }
        NEG() = default;

        void emit(VM& vm) override {
            auto value = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(-value));
        }
    };

    class NOT final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "NOT";
        }
        NOT() = default;

        void emit(VM& vm) override {
            auto value = vm.op_stack.popValue().asBool();
            vm.op_stack.push(Value(!value));
        }
    };

    class AND final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "AND";
        }
        AND() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asBool();
            auto a = vm.op_stack.popValue().asBool();
            vm.op_stack.push(Value(a && b));
        }
    };

    class OR final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "OR";
        }
        OR() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asBool();
            auto a = vm.op_stack.popValue().asBool();
            vm.op_stack.push(Value(a || b));
        }
    };

    class EQ final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "EQ";
        }
        EQ() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a == b));
        }
    };

    class NEQ final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "NEQ";
        }
        NEQ() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a != b));
        }
    };

    class LT final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "LT";
        }
        LT() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a < b));
        }
    };

    class LTE final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "LTE";
        }
        LTE() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a <= b));
        }
    };

    class GT final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "GT";
        }
        GT() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a > b));
        }
    };

    class GTE final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "GTE";
        }
        GTE() = default;

        void emit(VM& vm) override {
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a >= b));
        }
    };

    class STORE final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "STORE";
        }
        explicit STORE(const std::string& name) {
            operands.push_back(Value(name));
        }

        void emit(VM& vm) override {
            auto value = vm.op_stack.popValue();
            auto name = operands[0].asString();
            vm.symbols.set(name, value);
        }
    };

    class LOAD final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override {
            return "LOAD";
        }
        explicit LOAD(const std::string& name) {
            operands.emplace_back(name);
        }

        void emit(VM& vm) override {
            const auto name = operands[0].asString();
            const auto value = vm.symbols.get(name);
            vm.op_stack.push(value);
        }
    };
}
#undef NAME
