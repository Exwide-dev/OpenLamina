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
#include "../tools/lang/rational.hpp"
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

namespace lm::compiler {
    class ModuleManager;
}

namespace irgen {
    class ModuleObject;
    class Value;
    class VM;
    struct IteratorObject;

    class PUSH;
    class ADD;
    class MUL;
    class SUB;
    class DIV;
    class NEG;
    class DEREF;
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
    class NEW_VAR_OR_LOAD;
    class RET_THEN_LEAVE_SCOPE;
    class LOAD_FAST;
    class STORE_FAST;
    class ITER_NEW;
    class ITER_NEXT;
    class ITER_END;

    /**
     * @brief 数组映射模板类，使用索引访问的稀疏数组
     * @tparam T 值类型
     */
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
        DEREF,
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
        LOAD_FAST,
        STORE_FAST,
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
        NEW_VAR_OR_LOAD,
        RET_THEN_LEAVE_SCOPE,
        ITER_NEW,
        ITER_NEXT,
        ITER_END
    >;
    
    class SymbolTable;
    
    /**
     * @brief 函数对象，存储用户定义函数的信息
     */
    struct FunctionObject {
        std::vector<std::string> params;           ///< 函数参数列表
        std::vector<Opcode> body;                  ///< 函数体的IR指令序列
        size_t location;                            ///< 函数在源码中的位置
        std::string name = "<anonymous>";          ///< 函数名称
        VM* owner_vm = nullptr;                    ///< 所属虚拟机
        std::vector<SymbolTable> closure;          ///< 闭包捕获的变量
        bool needs_closure = false;                ///< 是否需要闭包
        
        /**
         * @brief 调用函数
         * @param caller_vm 调用者虚拟机
         * @param args 参数列表
         * @return 返回值
         */
        Value call(VM& caller_vm, const std::vector<Value>& args);
    };

    /**
     * @brief 函数类型，内置函数的签名
     */
    using FunctionType = std::function<Value(VM &, const std::vector<Value> &)>;

    /**
     * @brief 引用类型，用于值的间接引用
     */
    struct Ref {
        std::shared_ptr<Value> value_ptr;          ///< 指向实际值的智能指针

        /**
         * @brief 构造函数
         * @param ptr 值的智能指针
         */
        explicit Ref(std::shared_ptr<Value> ptr);

        /**
         * @brief 获取引用指向的值
         * @return 值的引用
         */
        Value &get() {
            if (!value_ptr) {
                throw RuntimeError("Null reference");
            }
            return *value_ptr;
        }

        /**
         * @brief 获取引用指向的值（const版本）
         * @return 值的const引用
         */
        [[nodiscard]] const Value &get() const {
            if (!value_ptr) {
                throw RuntimeError("Null reference");
            }
            return *value_ptr;
        }
    };

    /**
     * @brief 迭代器对象，支持遍历可迭代对象
     */
    struct IteratorObject {
        std::shared_ptr<Value> iterable;             ///< 被迭代的对象（使用指针避免循环依赖）
        size_t index = 0;                           ///< 当前迭代位置
        
        /**
         * @brief 构造函数
         * @param obj 要迭代的对象
         */
        explicit IteratorObject(std::shared_ptr<Value> obj) : iterable(std::move(obj)) {}
        
        /**
         * @brief 获取下一个元素
         * @return 是否还有下一个元素
         */
        bool next(Value& out);
    };

    /**
     * @brief 统一值类型，支持多种数据类型的存储和操作
     */
    class Value {
    public:
        /**
         * @brief 值的类型枚举
         */
        enum class Type {
            None,           ///< 空值
            Number,         ///< 任意精度整数
            Bool,           ///< 布尔值
            String,         ///< 字符串
            Function,       ///< 函数（内置或用户定义）
            Module,         ///< 模块
            Vector,         ///< 向量/列表
            Dictionary,     ///< 字典/映射
            Reference,      ///< 引用
            Rational,       ///< 有理数
            Iterator        ///< 迭代器
        };

    private:
        Type type;                                   ///< 当前值的类型
        std::variant<
            lang::lammp::Number,
            bool,
            std::string,
            FunctionType,
            std::shared_ptr<FunctionObject>,
            std::shared_ptr<ModuleObject>,
            std::vector<std::shared_ptr<Value>>,
            std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>,
            Ref,
            lang::lammp::Rational,
            std::shared_ptr<IteratorObject>
        > data;                                      ///< 存储实际值的变体

    public:
        /**
         * @brief 默认构造函数，创建空值
         */
        Value() : type(Type::None) {
        }

        /**
         * @brief 复制构造函数
         */
        Value(const Value& other) : type(other.type), data(other.data) {
        }

        /**
         * @brief 移动构造函数
         */
        Value(Value&& other) noexcept : type(other.type), data(std::move(other.data)) {
        }

        /**
         * @brief 从Number类型构造
         */
        explicit Value(const lang::lammp::Number &value)
            : type(Type::Number), data(value) {
        }

        /**
         * @brief 从Number类型移动构造
         */
        explicit Value(lang::lammp::Number &&value)
            : type(Type::Number), data(std::move(value)) {
        }

        /**
         * @brief 从整数类型构造
         * @tparam T 整数类型
         */
        template<IntegerType T>
        explicit Value(T value)
            : type(Type::Number), data(lang::lammp::Number(static_cast<int64_t>(value))) {
        }

        /**
         * @brief 从字符串类型构造
         * @tparam T 字符串类型
         */
        template<StringType T>
        explicit Value(T value)
            : type(Type::String), data(static_cast<std::string>(std::move(value))) {
        }

        /**
         * @brief 从std::string构造
         */
        explicit Value(const std::string& value)
            : type(Type::String), data(value) {
        }

        /**
         * @brief 从std::string移动构造
         */
        explicit Value(std::string&& value)
            : type(Type::String), data(std::move(value)) {
        }

        /**
         * @brief 从内置函数类型构造
         */
        explicit Value(FunctionType value)
            : type(Type::Function), data(value) {
        }

        /**
         * @brief 从用户定义函数对象构造
         */
        explicit Value(std::shared_ptr<FunctionObject> value)
            : type(Type::Function), data(value) {
        }

        /**
         * @brief 从布尔值构造
         */
        explicit Value(bool value)
            : type(Type::Bool), data(value) {
        }

        /**
         * @brief 从模块对象构造
         */
        explicit Value(std::shared_ptr<ModuleObject> value)
            : type(Type::Module), data(value) {
        }

        /**
         * @brief 从迭代器对象构造
         */
        explicit Value(std::shared_ptr<IteratorObject> value)
            : type(Type::Iterator), data(value) {
        }

        /**
         * @brief 从向量构造
         */
        explicit Value(std::vector<std::shared_ptr<Value>> value)
            : type(Type::Vector), data(std::move(value)) {
        }

        /**
         * @brief 从字典构造
         */
        explicit Value(std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> value)
            : type(Type::Dictionary), data(std::move(value)) {
        }

        /**
         * @brief 从初始化列表构造向量
         */
        Value(const std::initializer_list<Value> init)
            : type(Type::Vector) {
            std::vector<std::shared_ptr<Value>> vec;
            vec.reserve(init.size());
            for (const auto &val: init) {
                vec.push_back(std::make_shared<Value>(val));
            }
            data = std::move(vec);
        }

        /**
         * @brief 复制赋值运算符
         */
        Value& operator=(const Value& other) {
            if (this != &other) {
                type = other.type;
                data = other.data;
            }
            return *this;
        }

        /**
         * @brief 移动赋值运算符
         */
        Value& operator=(Value&& other) noexcept {
            if (this != &other) {
                type = other.type;
                data = std::move(other.data);
            }
            return *this;
        }

        /**
         * @brief 从引用构造
         */
        explicit Value(Ref ref)
            : type(Type::Reference), data(std::move(ref)) {
        }

        /**
         * @brief 从有理数构造
         */
        explicit Value(const lang::lammp::Rational &value)
            : type(Type::Rational), data(value) {
        }

        /**
         * @brief 从有理数移动构造
         */
        explicit Value(lang::lammp::Rational &&value)
            : type(Type::Rational), data(std::move(value)) {
        }

        /**
         * @brief 创建指向值的引用（移动语义）
         */
        static Value makeRef(Value &&val) {
            return Value(Ref(std::make_shared<Value>(std::move(val))));
        }

        /**
         * @brief 创建指向值的引用（复制语义）
         */
        static Value makeRef(const Value &val) {
            return Value(Ref(std::make_shared<Value>(val)));
        }

        /**
         * @brief 从智能指针创建引用
         */
        static Value makeRef(std::shared_ptr<Value> val_ptr) {
            return Value(Ref(std::move(val_ptr)));
        }

        /**
         * @brief 创建空引用
         */
        static Value makeEmptyRef() {
            return Value(Ref(std::make_shared<Value>()));
        }

        /**
         * @brief 函数调用操作符
         * @param vm 虚拟机实例
         * @param args 参数列表
         * @return 返回值
         */
        Value operator()(VM &vm, const std::vector<Value> &args) const {
            if (type != Type::Function) {
                throw RuntimeError("Value is not a function");
            }
            if (std::holds_alternative<FunctionType>(data)) {
                return std::get<FunctionType>(data)(vm, args);
            }
            throw RuntimeError("User-defined functions should be called via CALL instruction");
        }

        /**
         * @brief 获取值的类型（解引用后）
         * @return 值类型
         */
        [[nodiscard]] Type getType() const {
            if (type == Type::Reference) {
                return asReference().get().getType();
            }
            return type;
        }

        /**
         * @brief 获取类型名称字符串
         * @return 类型名称
         */
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
                case Type::Rational:
                    return "Rational";
                case Type::Dictionary:
                    return "Dictionary";
                case Type::Reference:
                    return "Reference";
                default:
                    return "<Unknown_Type>";
            }
        }

        /**
         * @brief 转换为字符串表示
         * @return 字符串表示
         */
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
            if (type == Type::Rational) {
                return const_cast<lang::lammp::Number &>(
                    *new lang::lammp::Number(asRational().toNumber()));
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
            if (type == Type::Rational) {
                return *new lang::lammp::Number(asRational().toNumber());
            }
            if (type != Type::Number) {
                throw RuntimeError("Value is not a number");
            }
            return std::get<lang::lammp::Number>(data);
        }

        [[nodiscard]] bool isRational() const {
            return deref().type == Type::Rational;
        }

        [[nodiscard]] const lang::lammp::Rational &asRational() const {
            if (type == Type::Reference) {
                return asReference().get().asRational();
            }
            if (type == Type::Number) {
                return const_cast<lang::lammp::Rational &>(
                    *new lang::lammp::Rational(asNumber()));
            }
            if (type != Type::Rational) {
                throw RuntimeError("Value is not a rational");
            }
            return std::get<lang::lammp::Rational>(data);
        }

        [[nodiscard]] lang::lammp::Rational &asRational() {
            if (type == Type::Reference) {
                return const_cast<lang::lammp::Rational &>(asReference().get().asRational());
            }
            if (type == Type::Number) {
                return *new lang::lammp::Rational(asNumber());
            }
            if (type != Type::Rational) {
                throw RuntimeError("Value is not a rational");
            }
            return std::get<lang::lammp::Rational>(data);
        }

        [[nodiscard]] ptrdiff_t asInt() const {
            return asNumber().toInt64();
        }

        DEFINE_AS_METHOD(Bool, Bool, bool, "a boolean")
        DEFINE_AS_METHOD(String, String, std::string, "a string")
        DEFINE_AS_METHOD(Function, Function, FunctionType, "a function")
        DEFINE_AS_METHOD(Module, Module, std::shared_ptr<ModuleObject>, "a module")
        DEFINE_AS_METHOD(Iterator, Iterator, std::shared_ptr<IteratorObject>, "an iterator")

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
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() + b.asRational());
            }
            throw RuntimeError("Unsupported + operation");
        }

        Value operator-(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() - b.asNumber());
            }
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() - b.asRational());
            }
            throw RuntimeError("Unsupported - operation");
        }

        Value operator*(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() * b.asNumber());
            }
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() * b.asRational());
            }
            throw RuntimeError("Unsupported * operation");
        }

        Value operator/(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                LOG("exec div...");
                return Value(a.asRational() / b.asRational());
            }
            throw RuntimeError("Unsupported / operation");
        }

        Value operator-() const {
            const Value &self = deref();
            if (self.type == Type::Number) {
                return Value(-self.asNumber());
            }
            if (self.type == Type::Rational) {
                return Value(-self.asRational());
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
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() < b.asRational());
            }
            throw RuntimeError("Unsupported < operation");
        }

        Value operator<=(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() <= b.asNumber());
            }
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() <= b.asRational());
            }
            throw RuntimeError("Unsupported <= operation");
        }

        Value operator>(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() > b.asNumber());
            }
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() > b.asRational());
            }
            throw RuntimeError("Unsupported > operation");
        }

        Value operator>=(const Value &other) const {
            const Value &a = deref();
            const Value &b = other.deref();
            if (a.type == Type::Number && b.type == Type::Number) {
                return Value(a.asNumber() >= b.asNumber());
            }
            if ((a.type == Type::Rational || a.type == Type::Number) && 
                (b.type == Type::Rational || b.type == Type::Number)) {
                return Value(a.asRational() >= b.asRational());
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

    class ITER_NEW {
    public:
        COMMON(ITER_NEW)
        std::vector<Value> operands;

        ITER_NEW() = default;

        void emit(VM &vm) const;
    };

    class ITER_NEXT {
    public:
        COMMON(ITER_NEXT)
        std::vector<Value> operands;

        ITER_NEXT() = default;

        void emit(VM &vm) const;
    };

    class ITER_END {
    public:
        COMMON(ITER_END)
        std::vector<Value> operands;

        ITER_END() = default;

        void emit(VM &vm) const;
    };

    inline Ref::Ref(std::shared_ptr<Value> ptr) : value_ptr(std::move(ptr)) {
        if (value_ptr->isReference()) {
            value_ptr = std::make_shared<Value>(value_ptr->deref());
        }
    }


    /**
     * @brief 符号表，用于管理变量的存储和访问
     */
    class SymbolTable {
    public:
        ArrMap<std::shared_ptr<Value>> symbols;    ///< 符号值映射
        ArrMap<bool> constants;                    ///< 常量标记映射

        /**
         * @brief 默认构造函数
         */
        SymbolTable() = default;

        /**
         * @brief 从map构造符号表
         */
        explicit SymbolTable(const std::map<size_t, std::shared_ptr<Value>> &symbols)
            : symbols(symbols) {
        }

        /**
         * @brief 从map移动构造符号表
         */
        explicit SymbolTable(std::map<size_t, std::shared_ptr<Value>> &&symbols)
            : symbols(symbols) {
        }

        /**
         * @brief 设置符号值
         * @param id 符号ID
         * @param value 值
         */
        void set(size_t id, const Value &value);

        /**
         * @brief 设置符号值（智能指针版本）
         * @param id 符号ID
         * @param value 值的智能指针
         */
        void set(size_t id, const std::shared_ptr<Value> &value);

        /**
         * @brief 设置符号是否为常量
         * @param id 符号ID
         * @param is_constant 是否为常量
         */
        void set_constant(size_t id, bool is_constant);

        /**
         * @brief 检查符号是否为常量
         * @param id 符号ID
         * @return 是否为常量
         */
        [[nodiscard]] bool is_constant(size_t id) const noexcept;

        /**
         * @brief 转换为字符串表示
         * @return 字符串表示
         */
        [[nodiscard]] std::string toString() const;

        /**
         * @brief 获取符号值
         * @param id 符号ID
         * @return 值的智能指针（如果存在）
         */
        [[nodiscard]] std::optional<std::shared_ptr<Value>> get(size_t id) const noexcept;

        /**
         * @brief 检查符号是否存在
         * @param id 符号ID
         * @return 是否存在
         */
        [[nodiscard]] bool exists(const size_t id) const {
            return symbols.contains(id);
        }

        [[nodiscard]] auto begin() const { return symbols.begin(); }
        [[nodiscard]] auto end() const { return symbols.end(); }
        [[nodiscard]] bool empty() const { return symbols.empty(); }
    };

    /**
     * @brief 栈模板类
     * @tparam Stackable 栈元素类型
     * @tparam reserve 预分配空间大小
     */
    template<typename Stackable, size_t reserve = 256>
    class Stack {
        std::vector<Stackable> data;               ///< 存储栈元素的向量

    public:
        /**
         * @brief 默认构造函数
         */
        Stack() {
            data.reserve(reserve);
        }

        /**
         * @brief 从向量构造
         */
        explicit Stack(std::vector<Stackable> data) : data(std::move(data)) {
            data.reserve(reserve);
        }

        /**
         * @brief 压入元素
         */
        void push(const Stackable &value) {
            data.push_back(value);
        }

        /**
         * @brief 移动压入元素
         */
        void push(Stackable &&value) {
            data.push_back(std::move(value));
        }

        /**
         * @brief 弹出元素（不返回）
         */
        void pop() {
            data.pop_back();
        }

        /**
         * @brief 弹出并返回元素
         * @return 弹出的元素
         */
        [[nodiscard]] Stackable popValue() {
            Stackable value = data.back();
            data.pop_back();
            return value;
        }

        /**
         * @brief 获取栈顶元素
         * @return 栈顶元素引用
         */
        [[nodiscard]] const Stackable &top() const {
            return data.back();
        }

        /**
         * @brief 获取栈大小
         * @return 元素数量
         */
        [[nodiscard]] size_t size() const {
            return data.size();
        }

        /**
         * @brief 检查栈是否为空
         * @return 是否为空
         */
        [[nodiscard]] bool empty() const {
            return data.empty();
        }

        /**
         * @brief 清空栈
         */
        void clear() {
            data.clear();
        }

        auto begin() { return data.begin(); }
        auto end() { return data.end(); }
        [[nodiscard]] auto begin() const { return data.begin(); }
        [[nodiscard]] auto end() const { return data.end(); }

        /**
         * @brief 转换为字符串表示
         * @return 字符串表示
         */
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

    /**
     * @brief 寄存器类，用于存储临时值
     */
    class Register {
    public:
        std::vector<Value> data;                   ///< 寄存器数据

        /**
         * @brief 构造函数
         * @param data 初始数据
         */
        explicit Register(std::vector<Value> const &data = std::vector<Value>())
            : data(data) {
        }

        /**
         * @brief 索引访问
         * @param index 索引
         * @return 值
         */
        Value operator[](const size_t index) {
            if (index >= data.size()) {
                std::stringstream ss;
                ss << "Register[]: index out of range, size=" << data.size() << ", try to access " << index;
                throw std::out_of_range(ss.str());
            }
            return data[index];
        }
    };

    /**
     * @brief 缓存类，用于加速变量访问
     * 使用固定大小的槽位实现，支持作用域管理
     */
    class Cache {
        static constexpr size_t SLOT_COUNT = 16;   ///< 槽位数量

        /**
         * @brief 作用域内的缓存槽
         */
        struct Scope {
            std::array<std::pair<size_t, std::shared_ptr<Value>>, SLOT_COUNT> slots;  ///< 槽数组
            std::unordered_map<size_t, size_t> id_to_index;                           ///< ID到索引的映射
            size_t next_slot = 0;                                                     ///< 下一个可用槽位

            Scope() {
                for (auto &id: slots | std::views::keys) {
                    id = 0;
                }
            }
        };

        std::vector<Scope> scopes;                 ///< 作用域栈

    public:
        /**
         * @brief 默认构造函数，创建初始作用域
         */
        Cache() {
            scopes.emplace_back();
        }

        /**
         * @brief 添加或更新缓存项
         * @param id 符号ID
         * @param val 值的智能指针
         */
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

        /**
         * @brief 获取缓存项
         * @param id 符号ID
         * @return 值的智能指针（如果存在）
         */
        [[nodiscard]] std::optional<std::shared_ptr<Value>> get(size_t id) const {
            const auto &scope = scopes.back();
            const auto it = scope.id_to_index.find(id);
            if (it != scope.id_to_index.end()) {
                return scope.slots[it->second].second;
            }
            return std::nullopt;
        }

        /**
         * @brief 检查缓存是否包含指定ID
         * @param id 符号ID
         * @return 是否包含
         */
        [[nodiscard]] bool contains(size_t id) const {
            const auto &scope = scopes.back();
            return scope.id_to_index.contains(id);
        }

        /**
         * @brief 进入新作用域
         */
        void enter_scope() {
            scopes.emplace_back();
        }

        /**
         * @brief 离开当前作用域
         */
        void leave_scope() {
            scopes.pop_back();
        }

        /**
         * @brief 清空所有缓存
         */
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

    class DEREF {
    public:
        COMMON(DEREF)
        std::vector<Value> operands;

        DEREF() = default;

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

    class NEW_VAR_OR_LOAD {
    public:
        COMMON(NEW_VAR_OR_LOAD)
        std::vector<Value> operands;

        explicit NEW_VAR_OR_LOAD(const std::string &name) {
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

    class LOAD_FAST {
    public:
        COMMON(LOAD_FAST)
        std::vector<Value> operands;

        explicit LOAD_FAST(size_t slot_index) {
            operands.emplace_back(static_cast<ptrdiff_t>(slot_index));
        }

        void emit(VM &vm) const;
    };

    class STORE_FAST {
    public:
        COMMON(STORE_FAST)
        std::vector<Value> operands;

        explicit STORE_FAST(size_t slot_index) {
            operands.emplace_back(static_cast<ptrdiff_t>(slot_index));
        }

        void emit(VM &vm) const;
    };

    /**
     * @brief 字符串池，用于字符串的唯一化存储
     */
    class StringPool {
        std::unordered_map<std::string, size_t> string_to_id;  ///< 字符串到ID的映射
        std::vector<std::string> id_to_string;                  ///< ID到字符串的映射
        size_t counter = 0;                                     ///< 下一个可用ID

    public:
        /**
         * @brief 默认构造函数
         */
        StringPool() = default;

        /**
         * @brief 添加字符串到池中
         * @param name 字符串
         * @return 字符串的ID
         */
        size_t add(const std::string &name) {
            const auto it = string_to_id.find(name);
            if (it != string_to_id.end()) {
                return it->second;
            }
            string_to_id[name] = counter;
            id_to_string.push_back(name);
            return counter++;
        }

        /**
         * @brief 检查字符串是否存在
         * @param name 字符串
         * @return 是否存在
         */
        bool exists(const std::string &name) const {
            return string_to_id.contains(name);
        }

        /**
         * @brief 获取字符串的ID
         * @param name 字符串
         * @return ID
         */
        size_t get_id(const std::string &name) const {
            const auto it = string_to_id.find(name);
            if (it == string_to_id.end()) {
                throw RuntimeError("String not found in pool: " + name);
            }
            return it->second;
        }

        /**
         * @brief 根据ID获取字符串
         * @param id ID
         * @return 字符串
         */
        const std::string &get_string(size_t id) const {
            if (id >= id_to_string.size()) {
                throw RuntimeError("String ID out of range: " + std::to_string(id));
            }
            return id_to_string[id];
        }

        /**
         * @brief 获取池大小
         * @return 字符串数量
         */
        size_t size() const {
            return id_to_string.size();
        }

        /**
         * @brief 检查池是否为空
         * @return 是否为空
         */
        bool empty() const {
            return id_to_string.empty();
        }

        /**
         * @brief 清空池
         */
        void clear() {
            string_to_id.clear();
            id_to_string.clear();
            counter = 0;
        }
    };

    inline StringPool g_string_pool{};                      ///< 全局字符串池

    /**
     * @brief 虚拟机类，执行IR指令
     */
    class VM {
    public:
        Stack<Value> op_stack{};                            ///< 操作数栈
        Stack<Value> call_stack{};                          ///< 调用栈
        std::vector<std::string> traceback{};               ///< 调用栈跟踪
        std::vector<FunctionObject> call_func_stack{};      ///< 函数调用栈
        std::vector<Opcode> code{};                         ///< IR指令序列
        std::vector<SymbolTable> symbol_stack{SymbolTable()};///< 符号表栈
        std::vector<std::vector<Value>> locals_stack;       ///< 局部变量栈
        Cache cache{};                                      ///< 变量缓存
        std::unordered_map<size_t, size_t> label_table{};   ///< 标签位置表
        size_t pc = 0;                                      ///< 程序计数器
        size_t label_counter = 0;                           ///< 标签计数器
        std::shared_ptr<ModuleObject> main_module;          ///< 主模块
        lm::compiler::ModuleManager* module_manager = nullptr; ///< 模块管理器指针

        /**
         * @brief 初始化内置函数
         */
        void init_builtins();

        /**
         * @brief 默认构造函数
         */
        VM();

        /**
         * @brief 从IR指令序列构造
         * @param c IR指令序列
         */
        explicit VM(std::vector<Opcode> c);

        /**
         * @brief 扫描标签并建立标签位置映射
         */
        void scan_labels() {
            for (size_t i = pc; i < code.size(); i++) {
                std::visit([&]<typename VT>(VT &op) -> void {
                    if constexpr (std::is_same_v<std::decay_t<VT>, LABEL>) {
                        label_table[static_cast<size_t>(op.operands[0].asInt())] = i;
                    }
                }, code[i]);
            }
        }

        /**
         * @brief 执行IR指令序列
         */
        void run();

        /**
         * @brief 获取符号值
         * @param name 符号名称
         * @return 值（如果存在）
         */
        std::optional<Value> get_symbol(const std::string &name) const;

        /**
         * @brief 设置符号值
         * @param name 符号名称
         * @param value 值
         */
        void set_symbol(const std::string &name, const Value &value);
    };

    /**
     * @brief 模块对象，存储模块的导出和子模块
     */
    class ModuleObject : public std::enable_shared_from_this<ModuleObject> {
        bool is_user;                                        ///< 是否为用户模块

    public:
        std::string name;                                    ///< 模块名称
        std::string full_name;                               ///< 完整模块路径名
        std::unordered_map<std::string, Value> exports;      ///< 导出的符号
        std::unordered_map<std::string, std::shared_ptr<ModuleObject>> submodules;  ///< 子模块
        VM *owner_vm = nullptr;                             ///< 所属虚拟机

        /**
         * @brief 从代码字符串构造模块
         * @tparam string 字符串类型
         * @param code 代码字符串
         */
        template<StringType string>
        explicit ModuleObject(string code);

        /**
         * @brief 从符号表构造模块（内置模块）
         * @param symbols 符号表
         */
        explicit(false) ModuleObject(const SymbolTable &symbols) : is_user(false) {
            for (const auto &[id, val]: symbols.symbols) {
                exports[g_string_pool.get_string(id)] = *val;
            }
            LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
            for (const auto &[atname, val]: exports) {
                LOG(atname << " : " << val);
            }
        }

        /**
         * @brief 从符号表栈构造模块（用户模块）
         * @param symbol_stack 符号表栈
         */
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

        /**
         * @brief 构造空模块
         * @param n 模块名称
         * @param vm 所属虚拟机
         */
        explicit ModuleObject(std::string n, VM *vm)
            : is_user(true), name(std::move(n)), full_name(name), owner_vm(vm) {
            LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
            for (const auto &[atname, val]: exports) {
                LOG(atname << " : " << val);
            }
        }

        /**
         * @brief 获取模块属性
         * @param attrname 属性名称
         * @return 属性值（如果存在）
         */
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

        /**
         * @brief 设置模块属性
         * @param attrname 属性名称
         * @param value 属性值
         */
        void set_attr(const std::string &attrname, const Value &value) {
            if (value.getType() == Value::Type::Module) {
                auto mod = value.asModule();
                mod->full_name = this->full_name.empty() ? attrname : this->full_name + "." + attrname;
                submodules[attrname] = mod;
            }
            exports[attrname] = value;
        }

        /**
         * @brief 导入子模块
         * @param module_name 模块名称
         * @return 模块值
         */
        Value import(const std::string &module_name);
    };
}
#undef NAME
