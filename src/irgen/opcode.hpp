#pragma once
#include <cmath>
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
#include <map>
#ifdef _WIN32
namespace std {
    template<typename K, typename V>
    using flat_map = map<K, V>;
}
#else
#include <flat_map>
#endif
#include <memory>

#include "../tools/debug.hpp"
#include "../tools/error.hpp"
#include "../tools/lang/builtins.hpp"
#include "front-end/front_end.hpp"

#define COMMON(ClassName) \
[[nodiscard]] std::string name() const { return #ClassName; } \
[[nodiscard]] std::string stringArgs() const { \
std::string s; \
for (size_t i = 0; i < operands.size(); ++i) { \
if (i > 0) s += ' '; \
s += operands[i].toString(); \
} \
return s; \
} \
[[nodiscard]] std::string toString() const { \
return std::format("{} {}", name(), stringArgs()); \
}

namespace irgen {
    class ModuleObject;
    class Number;
    class Value;
    class VM;
    class PUSH;
    class ADD;
    class MUL;
    class SUB;
    class DIV;
    class NEG;
    class NOT;
    class AND;
    class OR;
    class EQ;
    class NEQ;
    class LT;
    class LTE;
    class GT;
    class GTE;
    class STORE;
    class LOAD;
    class LABEL;
    class GOTO;
    class IFTRUEGOTO;
    class ENTER_SCOPE;
    class LEAVE_SCOPE;
    class CALL;
    class RET;
    class FINDMOD;
    class ATTR;
    class GETATTR;

    template<typename T>
    concept IntegerType = std::is_integral_v<T> and !std::same_as<T, bool>;

    template<typename T>
    concept StringType = std::convertible_to<T, std::string> and !IntegerType<T>;

    template<typename T>
    concept FloatType = std::floating_point<T> and !std::same_as<T, bool> and !IntegerType<T>;

    using Opcode = std::variant<
        PUSH,
        ADD,
        MUL,
        SUB,
        DIV,
        NEG,
        NOT,
        AND,
        OR,
        EQ,
        NEQ,
        LT,
        LTE,
        GT,
        GTE,
        STORE,
        LOAD,
        LABEL,
        GOTO,
        IFTRUEGOTO,
        ENTER_SCOPE,
        LEAVE_SCOPE,
        CALL,
        RET,
        FINDMOD,
        ATTR,
        GETATTR
    >;

    struct FunctionObject {
        std::vector<std::string> params;
        std::vector<Opcode> body;
        size_t location;
    };

    using FunctionType = std::function<Value(VM &, const std::vector<Value> &)>;

    class Number {
        ptrdiff_t numerator;
        ptrdiff_t denominator;

        void reduce();

        [[nodiscard]] ptrdiff_t gcd(ptrdiff_t a, ptrdiff_t b) const;

        template <FloatType T>
        static int get_decimal_places(T num) {
            LOG("get_decimal_places: " << num);
            std::string str = std::to_string(num);
            LOG("to_string: " << str);
            const size_t dot_pos = str.find('.');
            if (dot_pos == std::string::npos) return 0;

            // 去除尾随的零
            LOG("remove following 0s");
            size_t end = str.length() - 1;
            while (end > dot_pos && str[end] == '0') {
                ++end;
                LOG("++end");
            }
            LOG("Return!");
            return static_cast<int>(end - dot_pos);
        }

    public:
        Number() : numerator(0), denominator(1) {}

        template <IntegerType T>
        explicit(false) Number(const T num) : numerator(num), denominator(1) {}

        template <IntegerType A, IntegerType B>
        Number(const A num, const B den)
            : numerator(num), denominator(den) { reduce(); }

        template <StringType T>
        Number(const T& string) {

        }

        template <FloatType T>
        explicit Number(const T num) {
            // 处理特殊情况
            if (num == 0.0) {
                numerator = 0;
                denominator = 1;
                return;
            }

            // 确定浮点数的精度
            const int decimal_places = get_decimal_places(num);
            LOG(ITIS(decimal_places));
            uint64_t precision = std::pow(10, decimal_places);
            LOG(ITIS(precision));

            // 转换为整数分子分母
            numerator = static_cast<int64_t>(std::llround(num * precision));
            denominator = precision;

            reduce();  // 约分
        }

        [[nodiscard]] Number add(const Number& other) const {
            if (denominator == 1 and other.denominator == 1) {
                return {numerator + other.numerator};
            }
            const ptrdiff_t new_num = numerator * other.denominator + other.numerator * denominator;
            const ptrdiff_t new_den = denominator * other.denominator;
            return {new_num, new_den};
        }

        [[nodiscard]] Number sub(const Number& other) const {
            const ptrdiff_t new_num = numerator * other.denominator - other.numerator * denominator;
            const ptrdiff_t new_den = denominator * other.denominator;
            return {new_num, new_den};
        }

        [[nodiscard]] Number mul(const Number& other) const {
            return Number(numerator * other.numerator, denominator * other.denominator);
        }

        [[nodiscard]] Number div(const Number& other) const {
            if (other.numerator == 0) {
                throw RuntimeError("Division by zero");
            }
            return {numerator * other.denominator, denominator * other.numerator};
        }

        [[nodiscard]] Number neg() const {
            return {-numerator, denominator};
        }

        [[nodiscard]] bool eq(const Number& other) const {
            return numerator == other.numerator && denominator == other.denominator;
        }

        [[nodiscard]] bool neq(const Number& other) const {
            return !eq(other);
        }

        [[nodiscard]] bool lt(const Number& other) const {
            return numerator * other.denominator < other.numerator * denominator;
        }

        [[nodiscard]] bool lte(const Number& other) const {
            return numerator * other.denominator <= other.numerator * denominator;
        }

        [[nodiscard]] bool gt(const Number& other) const {
            return numerator * other.denominator > other.numerator * denominator;
        }

        [[nodiscard]] bool gte(const Number& other) const {
            return numerator * other.denominator >= other.numerator * denominator;
        }

        [[nodiscard]] std::string toString() const {
            if (denominator == 1) {
                return std::to_string(numerator);
            }
            return std::format("{} / {}", numerator, denominator);
        }

        [[nodiscard]] ptrdiff_t asInt() const {
            return numerator / denominator;
        }

        [[nodiscard]] ptrdiff_t getNumerator() const { return numerator; }
        [[nodiscard]] ptrdiff_t getDenominator() const { return denominator; }
        [[nodiscard]] bool isInteger() const { return denominator == 1; }
    };

    // Value 类型，支持多种类型的值
    class Value {
    public:
        enum class Type {
            None,
            Number,
            Bool,
            String,
            Function,
            Module
        };

    private:
        Type type;
        std::variant<
            Number,
            bool,
            std::string,
            FunctionType,
            std::shared_ptr<FunctionObject>,
            std::shared_ptr<ModuleObject>
        > data;

    public:
        Value() : type(Type::None) {}

        explicit Value(const Number& value)
            : type(Type::Number), data(std::move(value)) {}

        explicit Value(Number&& value)
            : type(Type::Number), data(value) {}

        template<IntegerType T>
        explicit Value(T value)
            : type(Type::Number), data(Number(static_cast<ptrdiff_t>(value))) {}




        template<StringType T>
        explicit Value(T value)
            : type(Type::String), data(static_cast<std::string>(std::move(value))) {}

        explicit Value(FunctionType value)
            : type(Type::Function), data(value) {}

        explicit Value(std::shared_ptr<FunctionObject> value)
            : type(Type::Function), data(value) {}

        explicit Value(bool value)
            : type(Type::Bool), data(value) {}

        explicit Value(std::shared_ptr<ModuleObject> value)
            : type(Type::Module), data(value) {}

        Value operator()(VM &vm, const std::vector<Value> &args) const {
            if (type != Type::Function) {
                throw RuntimeError("Value is not a function");
            }
            if (std::holds_alternative<FunctionType>(data)) {
                return std::get<FunctionType>(data)(vm, args);
            }
            throw RuntimeError("User-defined functions should be called via CALL instruction");
        }

        [[nodiscard]] Type getType() const { return type; }

        [[nodiscard]] std::string toString() const;

#define DEFINE_AS_METHOD(FieldName, EnumValue, CppType, ErrorMsg) \
[[nodiscard]] CppType as##FieldName() const { \
if (type != Type::EnumValue) { \
throw RuntimeError("Value is not " ErrorMsg); \
} \
return std::get<CppType>(data); \
}

        [[nodiscard]] const Number& asNumber() const {
            if (type != Type::Number) {
                throw RuntimeError("Value is not a number");
            }
            return std::get<Number>(data);
        }

        [[nodiscard]] Number& asNumber() {
            if (type != Type::Number) {
                throw RuntimeError("Value is not a number");
            }
            return std::get<Number>(data);
        }

        [[nodiscard]] ptrdiff_t asInt() const {
            return asNumber().asInt();
        }
        DEFINE_AS_METHOD(Bool, Bool, bool, "a boolean")
        DEFINE_AS_METHOD(String, String, std::string, "a string")
        DEFINE_AS_METHOD(Function, Function, FunctionType, "a function")
        DEFINE_AS_METHOD(Module, Module, std::shared_ptr<ModuleObject>, "a module")

        Value operator+(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().add(other.asNumber()));
            }
            if (type == Type::String && other.type == Type::String) {
                return Value(asString() + other.asString());
            }
            throw RuntimeError("Unsupported + operation");
        }

        Value operator-(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().sub(other.asNumber()));
            }
            throw RuntimeError("Unsupported - operation");
        }

        Value operator*(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().mul(other.asNumber()));
            }
            throw RuntimeError("Unsupported * operation");
        }

        Value operator/(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().div(other.asNumber()));
            }
            throw RuntimeError("Unsupported / operation");
        }

        Value operator-() const {
            if (type == Type::Number) {
                return Value(asNumber().neg());
            }
            throw RuntimeError("Unsupported unary - operation");
        }

        Value operator!() const {
            if (type == Type::Bool) {
                return Value(!asBool());
            }
            throw RuntimeError("Unsupported ! operation");
        }

        Value operator&&(const Value& other) const {
            if (type == Type::Bool && other.type == Type::Bool) {
                return Value(asBool() && other.asBool());
            }
            throw RuntimeError("Unsupported && operation");
        }

        Value operator||(const Value& other) const {
            if (type == Type::Bool && other.type == Type::Bool) {
                return Value(asBool() || other.asBool());
            }
            throw RuntimeError("Unsupported || operation");
        }

        Value operator<(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().lt(other.asNumber()));
            }
            throw RuntimeError("Unsupported < operation");
        }

        Value operator<=(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().lte(other.asNumber()));
            }
            throw RuntimeError("Unsupported <= operation");
        }

        Value operator>(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().gt(other.asNumber()));
            }
            throw RuntimeError("Unsupported > operation");
        }

        Value operator>=(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().gte(other.asNumber()));
            }
            throw RuntimeError("Unsupported >= operation");
        }

        bool operator==(const Value& other) {
            if (type == Type::Number && other.type == Type::Number) {
                return asNumber().eq(other.asNumber());
            }
            if (type == Type::Bool && other.type == Type::Bool) {
                return asBool() == other.asBool();
            }
            if (type == Type::String && other.type == Type::String) {
                return asString() == other.asString();
            }
            throw RuntimeError("Unsupported == operation");
        }

        Value operator!=(const Value& other) const {
            if (type == Type::Number && other.type == Type::Number) {
                return Value(asNumber().neq(other.asNumber()));
            }
            if (type == Type::Bool && other.type == Type::Bool) {
                return Value(asBool() != other.asBool());
            }
            if (type == Type::String && other.type == Type::String) {
                return Value(asString() != other.asString());
            }
            throw RuntimeError("Unsupported != operation");
        }

        [[nodiscard]] std::shared_ptr<FunctionObject> asFunctionObject() const {
            if (type != Type::Function) {
                throw RuntimeError("Value is not a function");
            }
            if (std::holds_alternative<std::shared_ptr<FunctionObject> >(data)) {
                return std::get<std::shared_ptr<FunctionObject>>(data);
            }
            throw RuntimeError("Value is not a user-defined function");
        }

        [[nodiscard]] bool isUserFunction() const {
            return type == Type::Function and std::holds_alternative<std::shared_ptr<FunctionObject> >(data);
        }

        [[nodiscard]] bool isBuiltinFunction() const {
            return type == Type::Function and std::holds_alternative<FunctionType>(data);
        }

#undef DEFINE_AS_METHOD

        [[nodiscard]] bool isNone() const { return type == Type::None; }
        [[nodiscard]] bool isInt() const { return type == Type::Number; }
        [[nodiscard]] bool isBool() const { return type == Type::Bool; }
        [[nodiscard]] bool isString() const { return type == Type::String; }
        [[nodiscard]] bool isFunction() const { return type == Type::Function; }

        friend std::ostream &operator<<(std::ostream &os, const Value &value) {
            os << value.toString();
            return os;
        }
    };

    class SymbolTable {
        std::flat_map<size_t, Value> symbols;

    public:
        SymbolTable() = default;

        explicit SymbolTable(std::flat_map<size_t, Value> symbols)
            : symbols(std::move(symbols)) {}

        void set(size_t id, const Value &value);

        std::optional<Value> get(size_t id) const noexcept;

        bool exists(const size_t id) const {
            return symbols.contains(id);
        }
    };

    template<typename Stackable>
    class Stack {
        std::vector<Stackable> data;

    public:
        Stack() = default;

        explicit Stack(std::vector<Stackable> data) : data(std::move(data)) {}

        void push(const Stackable &value) {
            data.push_back(value);
        }

        void push(Stackable &&value) {
            data.push_back(std::move(value));
        }

        void pop() {
            data.pop_back();
        }

        [[nodiscard]] Stackable popValue() {
            Stackable value = data.back();
            data.pop_back();
            return value;
        }
        [[nodiscard]] const Stackable &top() const {
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
                       [i = data.size() - 1](std::string acc, const auto &elem) mutable {
                           return std::move(acc) + std::format("{} | {}\n", i--, elem.toString());
                       }
                   );
        }
    };

    class Register {
    public:
        std::vector<Value> data;

        explicit Register(std::vector<Value> const &data = std::vector<Value>())
            : data(data) {
        }

        Value operator[](const size_t index) {
            if (index >= data.size()) {
                std::stringstream ss;
                ss << "Register[]: index out of range, size=" << data.size() << ", try to access " << index;
                throw std::out_of_range(ss.str());
            }
            return data[index];
        }
    };

    class Cache {
        static constexpr int n = 16;

        struct Var {
            size_t id;
            Value val;
        };

        std::vector<std::array<Var, n>> cache;
        int pos = 0;
    public:
        Cache() : cache({std::array<Var, n>{}}) {}

        Value operator[](const size_t id) const {
            for (const auto& [var_id, val] : cache.back()) {
                if (var_id == id) {
                    return val;
                }
            }
            throw std::out_of_range("No such variable with id: " + std::to_string(id));
        }

        void add(const size_t id, const Value& val) {
            LOG("cache add: " << id);
            if (get(id).has_value()) {
                cache.back()[pos] = Var{id, val};
                pos++;
                pos %= n;
            } else {
                int j = 0;
                for (const auto&[var_id, var_val] : cache.back()) {
                    if (var_id == id) {
                        cache.back()[j] = Var{id, val};
                    }
                    j++;
                }
            }
        }

        [[nodiscard]] std::optional<Value> get(const size_t id) const noexcept {
            LOG("cache get!");
            for (const auto& [var_id, val] : cache.back()) {
                if (var_id == id) {
                    return val;
                }
            }
            return std::nullopt;
        }

        void enter_scope() {
            cache.emplace_back(std::array<Var, n>{});
        }

        void leave_scope() {
            cache.pop_back();
        }

        void clear() {
            LOG("cache clear!");
            cache = std::vector({std::array<Var, n>{}});
        }
    };

    // 定义所有指令类（emit 只声明）
    class PUSH {
    public:
        COMMON(PUSH)
        std::vector<Value> operands;

        explicit PUSH(const Value &v) {
            operands.push_back(v);
        }

        void emit(VM &vm) const;
    };

    class ADD {
    public:
        COMMON(ADD)
        std::vector<Value> operands;

        ADD() = default;

        void emit(VM &vm);
    };

    class MUL {
    public:
        COMMON(MUL)
        std::vector<Value> operands;

        MUL() = default;

        void emit(VM &vm);
    };

    class SUB {
    public:
        COMMON(SUB)
        std::vector<Value> operands;

        SUB() = default;

        void emit(VM &vm);
    };

    class DIV {
    public:
        COMMON(DIV)
        std::vector<Value> operands;

        DIV() = default;

        void emit(VM &vm);
    };

    class NEG {
    public:
        COMMON(NEG)
        std::vector<Value> operands;

        NEG() = default;

        void emit(VM &vm);
    };

    class NOT {
    public:
        COMMON(NOT)
        std::vector<Value> operands;

        NOT() = default;

        void emit(VM &vm);
    };

    class AND {
    public:
        COMMON(AND)
        std::vector<Value> operands;

        AND() = default;

        void emit(VM &vm);
    };

    class OR {
    public:
        COMMON(OR)
        std::vector<Value> operands;

        OR() = default;

        void emit(VM &vm);
    };

    class EQ {
    public:
        COMMON(EQ)
        std::vector<Value> operands;

        EQ() = default;

        void emit(VM &vm);
    };

    class NEQ {
    public:
        COMMON(NEQ)
        std::vector<Value> operands;

        NEQ() = default;

        void emit(VM &vm);
    };

    class LT {
    public:
        COMMON(LT)
        std::vector<Value> operands;

        LT() = default;

        void emit(VM &vm);
    };

    class LTE {
    public:
        COMMON(LTE)
        std::vector<Value> operands;

        LTE() = default;

        void emit(VM &vm);
    };

    class GT {
    public:
        COMMON(GT)
        std::vector<Value> operands;

        GT() = default;

        void emit(VM &vm);
    };

    class GTE {
    public:
        COMMON(GTE)
        std::vector<Value> operands;

        GTE() = default;

        void emit(VM &vm);
    };

    class STORE {
    public:
        COMMON(STORE)
        std::vector<Value> operands;

        explicit STORE(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class LOAD {
    public:
        COMMON(LOAD)
        std::vector<Value> operands;

        explicit LOAD(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class LABEL {
    public:
        COMMON(LABEL)
        std::vector<Value> operands;

        explicit LABEL(size_t label_id) {
            operands.emplace_back(static_cast<ptrdiff_t>(label_id));
        }

        explicit LABEL(const Value &label_id) {
            operands.emplace_back(label_id);
        }

        void emit(VM &vm);

        void set_label(VM &vm, std::optional<size_t> on = std::nullopt) const;
    };

    class GOTO {
    public:
        COMMON(GOTO)
        std::vector<Value> operands;

        explicit GOTO(size_t label_id) {
            operands.emplace_back(static_cast<ptrdiff_t>(label_id));
        }

        explicit GOTO(const Value &label_id) {
            operands.emplace_back(label_id);
        }

        void emit(VM &vm) const;
    };

    class IFTRUEGOTO {
    public:
        COMMON(IFTRUEGOTO)
        std::vector<Value> operands;

        explicit IFTRUEGOTO(size_t label_id) {
            operands.emplace_back(static_cast<ptrdiff_t>(label_id));
        }

        explicit IFTRUEGOTO(const Value &label_id) {
            operands.emplace_back(label_id);
        }

        void emit(VM &vm) const;
    };

    class ENTER_SCOPE {
    public:
        COMMON(ENTER_SCOPE)
        std::vector<Value> operands;

        ENTER_SCOPE() = default;

        void emit(VM &vm);
    };

    class LEAVE_SCOPE {
    public:
        COMMON(LEAVE_SCOPE)
        std::vector<Value> operands;

        LEAVE_SCOPE() = default;

        void emit(VM &vm);
    };

    class CALL {
    public:
        COMMON(CALL)
        std::vector<Value> operands;

        explicit CALL(const std::string &name, const size_t arg_count) {
            operands.emplace_back(name);
            operands.emplace_back(static_cast<ptrdiff_t>(arg_count));
        }
        explicit CALL(const size_t arg_count) {
            operands.emplace_back(static_cast<ptrdiff_t>(arg_count));
        }

        void emit(VM &vm) const;
    };

    class RET {
    public:
        COMMON(RET)
        std::vector<Value> operands;

        RET() = default;

        void emit(VM &vm);
    };

    class FINDMOD {
    public:
        COMMON(FINDMOD)
        std::vector<Value> operands;

        explicit FINDMOD(const Value& val) {
            operands.emplace_back(val);
        };

        void emit(VM &vm);
    };

    class ATTR {
    public:
        COMMON(ATTR)
        std::vector<Value> operands;

        ATTR() = default;

        void emit(VM &vm);
    };

    class GETATTR {
    public:
        COMMON(GETATTR)
        std::vector<Value> operands;

        explicit GETATTR(const Value& attr_name) {
            operands.emplace_back(attr_name);
        }

        void emit(VM &vm) const;
    };

    class StringPool {
        std::unordered_map<std::string, size_t> string_to_id;
        std::vector<std::string> id_to_string;
        size_t counter = 0;
    public:
        StringPool() = default;

        size_t add(const std::string& name) {
            const auto it = string_to_id.find(name);
            if (it != string_to_id.end()) {
                return it->second;
            }
            string_to_id[name] = counter;
            id_to_string.push_back(name);
            return counter++;
        }

        bool exists(const std::string& name) const {
            return string_to_id.contains(name);
        }

        size_t get_id(const std::string& name) const {
            const auto it = string_to_id.find(name);
            if (it == string_to_id.end()) {
                throw RuntimeError("String not found in pool: " + name);
            }
            return it->second;
        }

        const std::string& get_string(size_t id) const {
            if (id >= id_to_string.size()) {
                throw RuntimeError("String ID out of range: " + std::to_string(id));
            }
            return id_to_string[id];
        }

        size_t size() const {
            return id_to_string.size();
        }

        bool empty() const {
            return id_to_string.empty();
        }

        void clear() {
            string_to_id.clear();
            id_to_string.clear();
            counter = 0;
        }
    };

    inline StringPool g_string_pool{};

    class VM {
    public:
        Stack<Value> op_stack{}, call_stack{};
        std::vector<Opcode> code{};
        std::vector<SymbolTable> symbol_stack{};
        SymbolTable symbols{};
        Cache cache{};
        std::unordered_map<size_t, size_t> label_table{};
        size_t pc = 0;
        size_t label_counter = 0;

        void init_builtins();

        VM() {
            init_builtins();
        }

        explicit VM(std::vector<Opcode> c) : code(std::move(c)) {
            init_builtins();
        }

        void scan_labels() {
            for (size_t i = pc; i < code.size(); i++) {
                std::visit([&]<typename VT>(VT &op) -> void {
                    if constexpr (std::is_same_v<std::decay_t<VT>, LABEL>) {
                        label_table[static_cast<size_t>(op.operands[0].asInt())] = i;
                    }
                }, code[i]);
            }
        }

        void run();
    };

    class ModuleObject {
        bool is_user;
    public:
        VM vm;

        template <StringType string>
        explicit ModuleObject(string code);

        explicit(false) ModuleObject(const SymbolTable& symbols) : is_user(false) {
            vm.symbols = symbols;
        }

        explicit ModuleObject(const std::vector<SymbolTable> &symbol_stack) : is_user(true) {
            vm.symbol_stack = std::move(symbol_stack);
        }

        void init() {
            vm.run();
        }
    };
}
#undef NAME
