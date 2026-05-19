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
#include <memory>
#include <unordered_set>
#include <initializer_list>

#include "../tools/debug.hpp"
#include "../tools/error.hpp"
#include "../tools/lang/number.hpp"
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
    class GOTOIF;
    class ENTER_SCOPE;
    class LEAVE_SCOPE;
    class CALL;
    class RET;
    class FINDMOD;
    class ATTR;
    class GETATTR;
    class VEC_NEW;
    class DICT_NEW;
    class INDEX;
    class STORE_ARG;
    class NEW_VAR;
    class NEW_CONST;
    class NEW_INTERN_VAR;
    class NEW_INTERN_CONST;
    class RET_THEN_LEAVE_SCOPE;

    template<typename T>
    class ArrMap {
        std::vector<T> data;
        std::vector<bool> has;
        T default_value;
        size_t element_count = 0;

    public:
        class Iterator {
            const std::vector<T> *data_ptr;
            const std::vector<bool> *has_ptr;
            size_t pos;

            void next_valid() {
                while (pos < data_ptr->size() && !(*has_ptr)[pos]) {
                    ++pos;
                }
            }

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::pair<size_t, T>;
            using difference_type = std::ptrdiff_t;
            using pointer = const value_type *;
            using reference = const value_type;

            Iterator(const std::vector<T> *d, const std::vector<bool> *h, size_t p)
                : data_ptr(d), has_ptr(h), pos(p) {
                next_valid();
            }

            Iterator &operator++() {
                ++pos;
                next_valid();
                return *this;
            }

            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            value_type operator*() const {
                return {pos, (*data_ptr)[pos]};
            }

            bool operator==(const Iterator &other) const {
                return pos == other.pos;
            }

            bool operator!=(const Iterator &other) const {
                return !(*this == other);
            }
        };

        ArrMap() : has(false), default_value(T{}) {}

        explicit ArrMap(const T &def) : has(false), default_value(def) {}

        ArrMap(const ArrMap &other)
            : data(other.data), has(other.has),
              default_value(other.default_value),
              element_count(other.element_count) {}

        ArrMap(ArrMap &&other) noexcept
            : data(std::move(other.data)), has(std::move(other.has)),
              default_value(std::move(other.default_value)),
              element_count(other.element_count) {
            other.element_count = 0;
        }

        template<typename MapType>
        explicit ArrMap(const MapType &map, T def = T{}) : has(false), default_value(std::move(def)) {
            for (const auto &[key, value]: map) {
                set(key, value);
            }
        }

        ArrMap(std::initializer_list<std::pair<size_t, T>> init, const T &def = T{})
            : default_value(def) {
            for (const auto &kv: init) {
                set(kv.first, kv.second);
            }
        }

        ArrMap &operator=(const ArrMap &other) {
            if (this != &other) {
                data = other.data;
                has = other.has;
                default_value = other.default_value;
                element_count = other.element_count;
            }
            return *this;
        }

        ArrMap &operator=(ArrMap &&other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                has = std::move(other.has);
                default_value = std::move(other.default_value);
                element_count = other.element_count;
                other.element_count = 0;
            }
            return *this;
        }

        [[nodiscard]] Iterator begin() const {
            return Iterator(&data, &has, 0);
        }

        [[nodiscard]] Iterator end() const {
            return Iterator(&data, &has, data.size());
        }

        [[nodiscard]] bool empty() const {
            return element_count == 0;
        }

        void set(size_t key, const T &value) {
            if (key >= data.size()) {
                data.resize(key + 1, default_value);
                has.resize(key + 1, false);
            }
            if (!has[key]) {
                ++element_count;
            }
            data[key] = value;
            has[key] = true;
        }

        template<typename... Args>
        void emplace(size_t key, Args &&... args) {
            if (key >= data.size()) {
                data.resize(key + 1, default_value);
                has.resize(key + 1, false);
            }
            if (!has[key]) {
                ++element_count;
            }
            data[key] = T(std::forward<Args>(args)...);
            has[key] = true;
        }

        bool erase(size_t key) {
            if (key >= data.size() || !has[key]) {
                return false;
            }
            has[key] = false;
            --element_count;
            data[key] = default_value;
            return true;
        }

        [[nodiscard]] T get(size_t key) const {
            if (key >= data.size() || !has[key]) {
                throw std::out_of_range("ArrMap::get: key " + std::to_string(key) + " not found");
            }
            return data[key];
        }

        [[nodiscard]] T find(size_t key) const {
            if (key >= data.size() || !has[key]) {
                throw std::out_of_range("ArrMap::find: key " + std::to_string(key) + " not found");
            }
            return data[key];
        }

        [[nodiscard]] std::optional<T> try_get(size_t key) const {
            if (key >= data.size() || !has[key]) {
                return std::nullopt;
            }
            return data[key];
        }

        T &operator[](size_t key) {
            if (key >= data.size() || !has[key]) {
                throw std::out_of_range("ArrMap::operator[]: key " + std::to_string(key) + " not found");
            }
            return data[key];
        }

        const T &operator[](size_t key) const {
            if (key >= data.size() || !has[key]) {
                throw std::out_of_range("ArrMap::operator[]: key " + std::to_string(key) + " not found");
            }
            return data[key];
        }

        [[nodiscard]] bool contains(size_t key) const {
            return key < data.size() && has[key];
        }

        void clear() {
            data.clear();
            has.clear();
            element_count = 0;
        }

        [[nodiscard]] size_t size() const {
            return element_count;
        }

        [[nodiscard]] size_t capacity() const {
            return data.size();
        }

        [[nodiscard]] size_t count() const {
            return element_count;
        }
    };

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
        GOTOIF,
        ENTER_SCOPE,
        LEAVE_SCOPE,
        CALL,
        RET,
        FINDMOD,
        GETATTR,
        VEC_NEW,
        DICT_NEW,
        INDEX,
        STORE_ARG,
        NEW_VAR,
        NEW_CONST,
        NEW_INTERN_VAR,
        NEW_INTERN_CONST,
        RET_THEN_LEAVE_SCOPE
    >;
    
    class SymbolTable;
    
    struct FunctionObject {
        std::vector<std::string> params;
        std::vector<Opcode> body;
        size_t location;
        std::string name = "<anonymous>";
        VM* owner_vm = nullptr;
        std::vector<SymbolTable> closure;
        
        Value call(VM& caller_vm, const std::vector<Value>& args);
    };

    using FunctionType = std::function<Value(VM &, const std::vector<Value> &)>;

    struct Ref {
        std::shared_ptr<Value> value_ptr;

        explicit Ref(std::shared_ptr<Value> ptr);

        Value &get() {
            if (!value_ptr) {
                throw RuntimeError("Null reference");
            }
            return *value_ptr;
        }

        [[nodiscard]] const Value &get() const {
            if (!value_ptr) {
                throw RuntimeError("Null reference");
            }
            return *value_ptr;
        }
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
            Module,
            Vector,
            Dictionary,
            Reference
        };

    private:
        Type type;
        std::variant<
            lang::lammp::Number,
            bool,
            std::string,
            FunctionType,
            std::shared_ptr<FunctionObject>,
            std::shared_ptr<ModuleObject>,
            std::vector<std::shared_ptr<Value>>,
            std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>,
            Ref
        > data;

    public:
        Value() : type(Type::None) {
        }

        explicit Value(const lang::lammp::Number &value)
            : type(Type::Number), data(value) {
        }

        explicit Value(lang::lammp::Number &&value)
            : type(Type::Number), data(std::move(value)) {
        }

        template<IntegerType T>
        explicit Value(T value)
            : type(Type::Number), data(lang::lammp::Number(static_cast<int64_t>(value))) {
        }

        template<StringType T>
        explicit Value(T value)
            : type(Type::String), data(static_cast<std::string>(std::move(value))) {
        }

        explicit Value(FunctionType value)
            : type(Type::Function), data(value) {
        }

        explicit Value(std::shared_ptr<FunctionObject> value)
            : type(Type::Function), data(value) {
        }

        explicit Value(bool value)
            : type(Type::Bool), data(value) {
        }

        explicit Value(std::shared_ptr<ModuleObject> value)
            : type(Type::Module), data(value) {
        }

        explicit Value(std::vector<std::shared_ptr<Value>> value)
            : type(Type::Vector), data(std::move(value)) {
        }

        explicit Value(std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> value)
            : type(Type::Dictionary), data(std::move(value)) {
        }

        Value(const std::initializer_list<Value> init)
            : type(Type::Vector) {
            std::vector<std::shared_ptr<Value>> vec;
            vec.reserve(init.size());
            for (const auto &val: init) {
                vec.push_back(std::make_shared<Value>(val));
            }
            data = std::move(vec);
        }

        explicit Value(Ref ref)
            : type(Type::Reference), data(std::move(ref)) {
        }

        static Value makeRef(Value &&val) {
            return Value(Ref(std::make_shared<Value>(std::move(val))));
        }

        static Value makeRef(const Value &val) {
            return Value(Ref(std::make_shared<Value>(val)));
        }

        static Value makeRef(std::shared_ptr<Value> val_ptr) {
            return Value(Ref(std::move(val_ptr)));
        }

        static Value makeEmptyRef() {
            return Value(Ref(std::make_shared<Value>()));
        }

        Value operator()(VM &vm, const std::vector<Value> &args) const {
            if (type != Type::Function) {
                throw RuntimeError("Value is not a function");
            }
            if (std::holds_alternative<FunctionType>(data)) {
                return std::get<FunctionType>(data)(vm, args);
            }
            throw RuntimeError("User-defined functions should be called via CALL instruction");
        }

        [[nodiscard]] Type getType() const {
            if (type == Type::Reference) {
                return asReference().get().getType();
            }
            return type;
        }

        [[nodiscard]] std::string type_name() const {
            switch (type) {
                case Type::None:
                    return "None";
                case Type::String:
                    return "String";
                case Type::Number:
                    return "Number";
                case Type::Bool:
                    return "Bool";
                case Type::Function:
                    return "Function";
                case Type::Module:
                    return "Module";
                case Type::Vector:
                    return "Vector";
                case Type::Dictionary:
                    return "Dictionary";
                case Type::Reference:
                    return "Reference";
                default:
                    return "<Unknown_Type>";
            }
        }

        [[nodiscard]] std::string toString() const;

#define DEFINE_AS_METHOD(FieldName, EnumValue, CppType, ErrorMsg) \
[[nodiscard]] CppType as##FieldName() const { \
if (type == Type::Reference) { \
return asReference().get().as##FieldName(); \
} \
if (type != Type::EnumValue) { \
throw RuntimeError("Value is not " ErrorMsg); \
} \
return std::get<CppType>(data); \
}

        [[nodiscard]] const lang::lammp::Number &asNumber() const {
            if (type == Type::Reference) {
                return asReference().get().asNumber();
            }
            if (type != Type::Number) {
                throw RuntimeError("Value is not a number");
            }
            return std::get<lang::lammp::Number>(data);
        }

        [[nodiscard]] lang::lammp::Number &asNumber() {
            if (type == Type::Reference) {
                return const_cast<lang::lammp::Number &>(asReference().get().asNumber());
            }
            if (type != Type::Number) {
                throw RuntimeError("Value is not a number");
            }
            return std::get<lang::lammp::Number>(data);
        }

        [[nodiscard]] ptrdiff_t asInt() const {
            return asNumber().toInt64();
        }

        DEFINE_AS_METHOD(Bool, Bool, bool, "a boolean")
        DEFINE_AS_METHOD(String, String, std::string, "a string")
        DEFINE_AS_METHOD(Function, Function, FunctionType, "a function")
        DEFINE_AS_METHOD(Module, Module, std::shared_ptr<ModuleObject>, "a module")

        [[nodiscard]] const std::vector<std::shared_ptr<Value>> &asVector() const {
            const Value &self = deref();
            if (self.type != Type::Vector) {
                throw RuntimeError("Value is not a vector");
            }
            return std::get<std::vector<std::shared_ptr<Value>>>(self.data);
        }

        [[nodiscard]] std::vector<std::shared_ptr<Value>> &asVector() {
            Value &self = deref();
            if (self.type != Type::Vector) {
                throw RuntimeError("Value is not a vector");
            }
            return std::get<std::vector<std::shared_ptr<Value>>>(self.data);
        }

        [[nodiscard]] const std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> &asDictionary() const {
            const Value &self = deref();
            if (self.type != Type::Dictionary) {
                throw RuntimeError("Value is not a dictionary");
            }
            return std::get<std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>>(self.data);
        }

        [[nodiscard]] std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> &asDictionary() {
            Value &self = deref();
            if (self.type != Type::Dictionary) {
                throw RuntimeError("Value is not a dictionary");
            }
            return std::get<std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>>(self.data);
        }

        [[nodiscard]] Ref &asReference() {
            if (type != Type::Reference) {
                throw RuntimeError("Value is not a reference");
            }
            return std::get<Ref>(data);
        }

        [[nodiscard]] const Ref &asReference() const {
            if (type != Type::Reference) {
                throw RuntimeError("Value is not a reference");
            }
            return std::get<Ref>(data);
        }

        std::shared_ptr<Value> getRefValuePtr() {
            if (type != Type::Reference) {
                throw RuntimeError("Value is not a reference");
            }
            return std::get<Ref>(data).value_ptr;
        }

        Value operator+(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() + b.asNumber());
            }
            if (a.type == Type::String && b.type == Type::String) {
                return Value(a.asString() + b.asString());
            }
            throw RuntimeError("Unsupported + operation");
        }

        Value operator-(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() - b.asNumber());
            }
            throw RuntimeError("Unsupported - operation");
        }

        Value operator*(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() * b.asNumber());
            }
            throw RuntimeError("Unsupported * operation");
        }

        Value operator/(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() / b.asNumber());
            }
            throw RuntimeError("Unsupported / operation");
        }

        Value operator-() const {
            const Value &self = deref();
            if (self.type == Type::Number) {
                return Value(-self.asNumber());
            }
            throw RuntimeError("Unsupported unary - operation");
        }

        Value operator!() const {
            const Value &self = deref();
            if (self.type == Type::Bool) {
                return Value(!self.asBool());
            }
            throw RuntimeError("Unsupported ! operation");
        }

        Value operator&&(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Bool && b.type == Type::Bool) {
                return Value(a.asBool() && b.asBool());
            }
            throw RuntimeError("Unsupported && operation");
        }

        Value operator||(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Bool && b.type == Type::Bool) {
                return Value(a.asBool() || b.asBool());
            }
            throw RuntimeError("Unsupported || operation");
        }

        Value operator<(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() < b.asNumber());
            }
            throw RuntimeError("Unsupported < operation");
        }

        Value operator<=(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() <= b.asNumber());
            }
            throw RuntimeError("Unsupported <= operation");
        }

        Value operator>(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() > b.asNumber());
            }
            throw RuntimeError("Unsupported > operation");
        }

        Value operator>=(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() >= b.asNumber());
            }
            throw RuntimeError("Unsupported >= operation");
        }

        bool operator==(const Value &other) {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return a.asNumber() == b.asNumber();
            }
            if (a.type == Type::Bool && b.type == Type::Bool) {
                return a.asBool() == b.asBool();
            }
            if (a.type == Type::String && b.type == Type::String) {
                return a.asString() == b.asString();
            }
            throw RuntimeError(std::format("Unsupported == operation, left = {}, right = {}", a.type_name(), b.type_name()));
        }

        Value operator!=(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() != b.asNumber());
            }
            if (a.type == Type::Bool && b.type == Type::Bool) {
                return Value(a.asBool() != b.asBool());
            }
            if (a.type == Type::String && b.type == Type::String) {
                return Value(a.asString() != b.asString());
            }
            throw RuntimeError("Unsupported != operation");
        }

        [[nodiscard]] std::shared_ptr<FunctionObject> asFunctionObject() const {
            const Value &self = deref();
            if (self.type != Type::Function) {
                throw RuntimeError("Value is not a function");
            }
            if (self.data.index() == 4) {
                return std::get<std::shared_ptr<FunctionObject>>(self.data);
            }
            throw RuntimeError("Value is not a user-defined function");
        }

        [[nodiscard]] bool isUserFunction() const {
            const Value &self = deref();
            return self.type == Type::Function &&
                   self.data.index() == 4;
        }

        [[nodiscard]] bool isBuiltinFunction() const {
            const Value &self = deref();
            return self.type == Type::Function &&
                   self.data.index() == 3;
        }

#undef DEFINE_AS_METHOD

        [[nodiscard]] bool isNone() const {
            return deref().type == Type::None;
        }

        [[nodiscard]] bool isInt() const {
            return deref().type == Type::Number;
        }

        [[nodiscard]] bool isBool() const {
            return deref().type == Type::Bool;
        }

        [[nodiscard]] bool isString() const {
            return deref().type == Type::String;
        }

        [[nodiscard]] bool isFunction() const {
            return deref().type == Type::Function;
        }

        [[nodiscard]] bool isVector() const {
            return deref().type == Type::Vector;
        }

        [[nodiscard]] bool isDictionary() const {
            return deref().type == Type::Dictionary;
        }

        [[nodiscard]] bool isReference() const { return type == Type::Reference; }

        void set(const Value &value) const {
            if (type != Type::Reference) {
                throw RuntimeError("Cannot set a non-reference value");
            }
            *std::get<Ref>(data).value_ptr = value.deref();
        }

        [[nodiscard]] const Value &deref() const {
            if (type == Type::Reference) {
                return std::get<Ref>(data).value_ptr->deref();
            }
            return *this;
        }

        Value &deref() {
            auto current = this;
            while (current->type == Type::Reference) {
                current = std::get<Ref>(current->data).value_ptr.get();
            }
            return *current;
        }

        Value& safe_deref() {
            std::unordered_set<Value*> visited;  // 用裸指针，避免 shared_ptr 拷贝
            auto current = this;

            while (current->type == Type::Reference) {
                if (visited.contains(current)) {  // 先检查当前节点
                    throw RuntimeError("Circular reference detected");
                }
                visited.insert(current);
                current = std::get<Ref>(current->data).value_ptr.get();
            }

            return *current;
        }

        friend std::ostream &operator<<(std::ostream &os, const Value &value) {
            os << value.toString();
            return os;
        }

        bool isNumber() const {
            return deref().type == Type::Number;
        };
    };

    inline Ref::Ref(std::shared_ptr<Value> ptr) : value_ptr(std::move(ptr)) {
        if (value_ptr->isReference()) {
            value_ptr = std::make_shared<Value>(value_ptr->deref());
        }
    }


    class SymbolTable {
    public:
        ArrMap<std::shared_ptr<Value>> symbols;
        ArrMap<bool> constants;

        SymbolTable() = default;

        explicit SymbolTable(const std::map<size_t, std::shared_ptr<Value>> &symbols)
            : symbols(symbols) {
        }

        explicit SymbolTable(std::map<size_t, std::shared_ptr<Value>> &&symbols)
            : symbols(symbols) {
        }

        void set(size_t id, const Value &value);

        void set(size_t id, const std::shared_ptr<Value> &value);

        void set_constant(size_t id, bool is_constant);

        [[nodiscard]] bool is_constant(size_t id) const noexcept;

        [[nodiscard]] std::string toString() const;

        [[nodiscard]] std::optional<std::shared_ptr<Value>> get(size_t id) const noexcept;

        [[nodiscard]] bool exists(const size_t id) const {
            return symbols.contains(id);
        }

        [[nodiscard]] auto begin() const { return symbols.begin(); }
        [[nodiscard]] auto end() const { return symbols.end(); }
        [[nodiscard]] bool empty() const { return symbols.empty(); }
    };

    template<typename Stackable, size_t reserve = 256>
    class Stack {
        std::vector<Stackable> data;

    public:
        Stack() {
            data.reserve(reserve);
        }

        explicit Stack(std::vector<Stackable> data) : data(std::move(data)) {
            data.reserve(reserve);
        }

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
        static constexpr size_t SLOT_COUNT = 16;

        struct Scope {
            std::array<std::pair<size_t, std::shared_ptr<Value>>, SLOT_COUNT> slots;
            std::unordered_map<size_t, size_t> id_to_index;
            size_t next_slot = 0;

            Scope() {
                for (auto &id: slots | std::views::keys) {
                    id = 0;
                }
            }
        };

        std::vector<Scope> scopes;

    public:
        Cache() {
            scopes.emplace_back();
        }

        void add(size_t id, const std::shared_ptr<Value> &val) {
            auto &scope = scopes.back();
            auto it = scope.id_to_index.find(id);
            if (it != scope.id_to_index.end()) {
                scope.slots[it->second].second = val;
                return;
            }

            size_t idx = scope.next_slot;
            size_t old_id = scope.slots[idx].first;
            if (old_id != 0) {
                scope.id_to_index.erase(old_id);
            }
            scope.slots[idx] = {id, val};
            scope.id_to_index[id] = idx;
            scope.next_slot = (idx + 1) % SLOT_COUNT;
        }

        [[nodiscard]] std::optional<std::shared_ptr<Value>> get(size_t id) const {
            const auto &scope = scopes.back();
            const auto it = scope.id_to_index.find(id);
            if (it != scope.id_to_index.end()) {
                return scope.slots[it->second].second;
            }
            return std::nullopt;
        }

        [[nodiscard]] bool contains(size_t id) const {
            const auto &scope = scopes.back();
            return scope.id_to_index.contains(id);
        }

        void enter_scope() {
            scopes.emplace_back();
        }

        void leave_scope() {
            scopes.pop_back();
        }

        void clear() {
            scopes.clear();
            scopes.emplace_back();
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

        STORE() = default;

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

    class GOTOIF {
    public:
        COMMON(IFTRUEGOTO)
        std::vector<Value> operands;

        explicit GOTOIF(size_t label_id) {
            operands.emplace_back(static_cast<ptrdiff_t>(label_id));
        }

        explicit GOTOIF(const Value &label_id) {
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

        explicit FINDMOD(const Value &val) {
            operands.emplace_back(val);
        };

        void emit(VM &vm);
    };


    class GETATTR {
    public:
        COMMON(GETATTR)
        std::vector<Value> operands;

        explicit GETATTR(const Value &attr_name) {
            operands.emplace_back(attr_name);
        }

        void emit(VM &vm) const;
    };

    class VEC_NEW {
    public:
        COMMON(VEC_NEW)
        std::vector<Value> operands;

        explicit VEC_NEW(const size_t element_count) {
            operands.emplace_back(static_cast<ptrdiff_t>(element_count));
        }

        void emit(VM &vm) const;
    };

    class DICT_NEW {
    public:
        COMMON(DICT_NEW)
        std::vector<Value> operands;

        explicit DICT_NEW(const size_t entry_count) {
            operands.emplace_back(static_cast<ptrdiff_t>(entry_count));
        }

        void emit(VM &vm) const;
    };

    class INDEX {
    public:
        COMMON(INDEX)
        std::vector<Value> operands;

        INDEX() = default;

        void emit(VM &vm) const;
    };

    class STORE_ARG {
    public:
        COMMON(STORE_ARG)
        std::vector<Value> operands;

        explicit STORE_ARG(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class NEW_VAR {
    public:
        COMMON(NEW_VAR)
        std::vector<Value> operands;

        explicit NEW_VAR(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class NEW_CONST {
    public:
        COMMON(NEW_CONST)
        std::vector<Value> operands;

        explicit NEW_CONST(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class NEW_INTERN_VAR {
    public:
        COMMON(NEW_INTERN_VAR)
        std::vector<Value> operands;

        explicit NEW_INTERN_VAR(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class NEW_INTERN_CONST {
    public:
        COMMON(NEW_INTERN_CONST)
        std::vector<Value> operands;

        explicit NEW_INTERN_CONST(const std::string &name) {
            operands.emplace_back(name);
        }

        void emit(VM &vm) const;
    };

    class RET_THEN_LEAVE_SCOPE {
    public:
        COMMON(RET_THEN_LEAVE_SCOPE)
        std::vector<Value> operands;

        RET_THEN_LEAVE_SCOPE() = default;

        void emit(VM &vm) const;
    };

    class StringPool {
        std::unordered_map<std::string, size_t> string_to_id;
        std::vector<std::string> id_to_string;
        size_t counter = 0;

    public:
        StringPool() = default;

        size_t add(const std::string &name) {
            const auto it = string_to_id.find(name);
            if (it != string_to_id.end()) {
                return it->second;
            }
            string_to_id[name] = counter;
            id_to_string.push_back(name);
            return counter++;
        }

        bool exists(const std::string &name) const {
            return string_to_id.contains(name);
        }

        size_t get_id(const std::string &name) const {
            const auto it = string_to_id.find(name);
            if (it == string_to_id.end()) {
                throw RuntimeError("String not found in pool: " + name);
            }
            return it->second;
        }

        const std::string &get_string(size_t id) const {
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
        std::vector<std::string> traceback{};
        std::vector<FunctionObject> call_func_stack{};
        std::vector<Opcode> code{};
        std::vector<SymbolTable> symbol_stack{SymbolTable()};
        Cache cache{};
        std::unordered_map<size_t, size_t> label_table{};
        size_t pc = 0;
        size_t label_counter = 0;
        std::shared_ptr<ModuleObject> main_module;

        void init_builtins();

        VM();

        explicit VM(std::vector<Opcode> c);

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

        std::optional<Value> get_symbol(const std::string &name) const;

        void set_symbol(const std::string &name, const Value &value);
    };

    class ModuleObject : public std::enable_shared_from_this<ModuleObject> {
        bool is_user;

    public:
        std::string name;
        std::string full_name;
        std::unordered_map<std::string, Value> exports;
        std::unordered_map<std::string, std::shared_ptr<ModuleObject>> submodules;
        VM *owner_vm = nullptr;

        template<StringType string>
        explicit ModuleObject(string code);

        explicit(false) ModuleObject(const SymbolTable &symbols) : is_user(false) {
            for (const auto &[id, val]: symbols.symbols) {
                exports[g_string_pool.get_string(id)] = *val;
            }
            LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
            for (const auto &[atname, val]: exports) {
                LOG(atname << " : " << val);
            }
        }

        explicit ModuleObject(const std::vector<SymbolTable> &symbol_stack) : is_user(true) {
            for (const auto &table: symbol_stack) {
                for (const auto &[id, val]: table.symbols) {
                    exports[g_string_pool.get_string(id)] = *val;
                }
            }
            LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
            for (const auto &[atname, val]: exports) {
                LOG(atname << " : " << val);
            }
        }

        explicit ModuleObject(std::string n, VM *vm)
            : is_user(true), name(std::move(n)), full_name(name), owner_vm(vm) {
            LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
            for (const auto &[atname, val]: exports) {
                LOG(atname << " : " << val);
            }
        }


        std::optional<Value> get_attr(const std::string &attrname) const {
            LOG("Finding " + attrname);

            LOG("exports: ");
            for (const auto &[atname, val]: exports) {
                LOG(atname << " : " << val);
            }

            if (exports.contains(attrname)) {
                const auto it = exports.find(attrname);
                LOG("Found it in itself");
                return it->second;
            }
            if (submodules.contains(attrname)) {
                const auto mod_it = submodules.find(attrname);
                LOG("Found it in submodule");
                return Value(mod_it->second);
            }
            LOG("Not found it");
            return std::nullopt;
        }

        void set_attr(const std::string &attrname, const Value &value) {
            if (value.getType() == Value::Type::Module) {
                auto mod = value.asModule();
                mod->full_name = this->full_name.empty() ? attrname : this->full_name + "." + attrname;
                submodules[attrname] = mod;
            }
            exports[attrname] = value;
        }

        Value import(const std::string &module_name);
    };
}
#undef NAME
