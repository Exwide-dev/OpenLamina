#pragma once
#include <format>
#include <functional>
#include <iostream>
#include <numeric>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <variant>
#include <unordered_map>

#include "../tools/debug.hpp"
#include "../tools/error.hpp"
#include "../tools/lang/builtins.hpp"

namespace irgen {
    class Value;
    class VM;
    class LABEL;

    template<typename T>
    concept IntegerType = std::is_integral_v<T> && !std::same_as<T, bool>;

    template<typename T>
    concept StringType = std::convertible_to<T, std::string> && !IntegerType<T>;

    using FunctionType = std::function<Value(VM&, const std::vector<Value>&)>;

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
        std::variant<ptrdiff_t, bool, std::string, FunctionType> data;

    public:
        Value() : type(Type::None) {}

        template<IntegerType T>
        explicit Value(T value)
            : type(Type::Int), data(static_cast<ptrdiff_t>(value)) {}

        template<StringType T>
        explicit Value(T value)
            : type(Type::String), data(static_cast<std::string>(std::move(value))) {}

        explicit Value(FunctionType value) : type(Type::Function), data(value) {}
        explicit Value(bool value) : type(Type::Bool), data(value) {}

        Value operator()(VM& vm, const std::vector<Value>& args) const {
            if (type != Type::Function) {
                throw RuntimeError("Value is not a function");
            }
            return std::get<FunctionType>(data)(vm, args);
        }

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
        throw RuntimeError("Value is not " ErrorMsg); \
    } \
    return std::get<CppType>(data); \
}

        DEFINE_AS_METHOD(Int, Int, ptrdiff_t, "an integer")
        DEFINE_AS_METHOD(Bool, Bool, bool, "a boolean")
        DEFINE_AS_METHOD(String, String, std::string, "a string")
        DEFINE_AS_METHOD(Function, Function, FunctionType, "a function")

#undef DEFINE_AS_METHOD

        [[nodiscard]] bool isNone() const { return type == Type::None; }
        [[nodiscard]] bool isInt() const { return type == Type::Int; }
        [[nodiscard]] bool isBool() const { return type == Type::Bool; }
        [[nodiscard]] bool isString() const { return type == Type::String; }
        [[nodiscard]] bool isFunction() const { return type == Type::Function; }

        friend std::ostream& operator<<(std::ostream& os, const Value& value) {
            LOG("access <<");
            os << value.toString();
            return os;
        }
    };

    // 符号表，用于存储变量和函数
    class SymbolTable {
        std::unordered_map<std::string, Value> symbols;

    public:
        SymbolTable() = default;
        explicit SymbolTable(std::unordered_map<std::string, Value> symbols) : symbols(std::move(symbols)) {};

        void set(const std::string& name, const Value& value) {
            symbols[name] = value;
        }

        Value get(const std::string& name) const {
            const auto it = symbols.find(name);
            if (it == symbols.end()) {
                throw RuntimeError("Variable not found: " + name);
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
            LOG("Pushing back: " << value.toString());
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
                data.rbegin(), data.rend(),
                std::string{},
                [i = data.size() - 1](std::string acc, const auto& elem) mutable {
                    return std::move(acc) + std::format("{} | {}\n", i--, elem.toString());
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

        [[nodiscard]] std::string stringArgs() const {
            return operands
                | std::views::transform([](auto& k) { return k.toString(); })
                | std::views::join_with(' ')
                | std::ranges::to<std::string>();
        }

        [[nodiscard]] std::string toString() const {
            return std::format("{} {}", name(), stringArgs());
        }

        virtual void emit(VM& vm) = 0;
    };

    class VM {
    public:
        Stack op_stack, call_stack;
        std::vector<Opcode*> code;
        Register reg;
        std::vector<SymbolTable> symbol_stack;
        SymbolTable symbols;
        
        // 初始化内置函数
        void init_builtins();
        std::unordered_map<std::string, size_t> label_table;
        size_t pc = 0;

        VM() {
            init_builtins();
        }
        explicit VM(std::vector<Opcode*> c) : code(std::move(c)) {
            init_builtins();
        }

        void run() {
            try {
                scan_labels();
                symbol_stack.push_back(symbols); // 压入全局符号表
                for (;pc < code.size(); pc++) {
                    LOG("VM " << op_stack.toString());
                    LOG("Exec " << pc << " | " << code[pc]->name() << " " << code[pc]->stringArgs() << std::endl);
                    code[pc]->emit(*this);
                }
                LOG("Now done a term. VM " << op_stack.toString());
                LOG("Clear the op_stack, save the top");
                const Value top = op_stack.popValue();
                op_stack.clear();
                op_stack.push(top);
            } catch ([[maybe_unused]] const std::exception& e) {
                op_stack.clear();
                throw;
            }
        }

        void scan_labels();
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
            LOG("PUSH " << operands[0].toString());
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
            auto b = vm.op_stack.popValue().asInt();
            auto a = vm.op_stack.popValue().asInt();
            vm.op_stack.push(Value(a * b));
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
            operands.emplace_back(name);
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

    class LABEL final : public Opcode {
        public:
        [[nodiscard]] std::string name() const override { return "LABEL"; }
        explicit LABEL(const std::string& name) {
            operands.emplace_back(name);
        }
        explicit LABEL(const Value& name) {
            operands.emplace_back(name);
        }
        void emit(VM &vm) override {
            // Do nothing, because the setting will be acted in
            // void set_label(VM& vm, const std::optional<size_t> on = std::nullopt) const.
        }
        void set_label(VM& vm, const std::optional<size_t> on = std::nullopt) const {
            vm.label_table[operands[0].asString()] = on.value_or(vm.pc);
        }
    };

    class GOTO final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override { return "GOTO"; }
        explicit GOTO(const std::string& name) {
            operands.emplace_back(name);
        }
        explicit GOTO(const Value& name) {
            operands.emplace_back(name);
        }
        void emit(VM &vm) override {
            LOG("Before goto, pc = " << vm.pc);
            if (not vm.label_table.contains(operands[0].asString())) {
                throw RuntimeError("Unknown label: " + operands[0].asString());
            }
            vm.pc = vm.label_table[operands[0].asString()];
            LOG("After goto, pc = " << vm.pc);
        }
    };

    class IFTRUEGOTO final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override { return "IFTRUEGOTO"; }
        explicit IFTRUEGOTO(const std::string& name) {
            operands.emplace_back(name);
        }
        explicit IFTRUEGOTO(const Value& name) {
            operands.emplace_back(name);
        }
        void emit(VM &vm) override {
            if (vm.op_stack.popValue().asBool()) {
                (new GOTO(operands[0]))->emit(vm);
            }
        }
    };

    class ENTER_SCOPE final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override { return "ENTER_SCOPE"; }
        ENTER_SCOPE() = default;

        void emit(VM& vm) override {
            // 保存当前符号表并创建新的作用域
            vm.symbol_stack.push_back(vm.symbols);
            vm.symbols = SymbolTable();
        }
    };

    class LEAVE_SCOPE final : public Opcode {
    public:
        [[nodiscard]] std::string name() const override { return "LEAVE_SCOPE"; }
        LEAVE_SCOPE() = default;

        void emit(VM& vm) override {
            // 恢复上一个作用域的符号表
            if (vm.symbol_stack.empty()) {
                throw RuntimeError("LEAVE_SCOPE: no active scope");
            }
            vm.symbols = vm.symbol_stack.back();
            vm.symbol_stack.pop_back();
        }
    };

    class CALL final : public Opcode {
        public:
        [[nodiscard]] std::string name() const override { return "CALL"; }
        explicit CALL(const std::string& name, size_t arg_count) {
            operands.emplace_back(name);
            operands.emplace_back(static_cast<ptrdiff_t>(arg_count));
        }
        void emit(VM &vm) override {
            auto func_name = operands[0].asString();
            auto arg_count = operands[1].asInt();
            auto func = vm.symbols.get(func_name);
            if (func.isFunction()) {
                // 从栈中取出参数
                std::vector<Value> args;
                for (size_t i = 0; i < static_cast<size_t>(arg_count); i++) {
                    if (vm.op_stack.empty()) {
                        throw RuntimeError("CALL: not enough arguments on stack");
                    }
                    args.insert(args.begin(), vm.op_stack.popValue());
                }
                // 进入函数作用域
                vm.symbol_stack.push_back(vm.symbols);
                vm.symbols = SymbolTable();
                // 调用函数
                const auto result = func(vm, args);
                // 离开函数作用域
                if (!vm.symbol_stack.empty()) {
                    vm.symbols = vm.symbol_stack.back();
                    vm.symbol_stack.pop_back();
                }
                // 将结果压入栈
                vm.op_stack.push(result);
            }
        }
    };

    inline void VM::scan_labels()  {
        for (size_t i = pc; i < code.size(); i++) {
            LOG("Now i(init by pc): " << i << ", Now code: " << code[i]->toString());
            if (const auto* label = dynamic_cast<LABEL*>(code[i])) {
                LOG("Emit!");
                label->set_label(*this, i);
            }
        }
        LOG("Initializing Done. labels: " <<
            [&]() -> std::string {
                std::string s;
                for (auto it = label_table.begin(); it != label_table.end(); ++it) {
                    s += "| " + it->first + ": " + std::to_string(it->second) + "\n";
                }
                return s;
            }()
        );
    }

}
#undef NAME
