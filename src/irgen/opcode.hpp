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
#include <optional>
#include <unordered_set>
#include <initializer_list>

#include "../tools/debug.hpp"
#include "../tools/error.hpp"
#include "../tools/lang/number.hpp"
#include "../tools/lang/rational.hpp"
#include "cell_pool.hpp"
#include "runtime_ast.hpp"

#define OPCODE_META(ClassName) \
    [[nodiscard]] std::string name() const { return #ClassName; } \
    std::string line; \
    int line_no = 0;

#define OPCODE_ARGS0() \
    [[nodiscard]] std::string stringArgs() const { return {}; } \
    [[nodiscard]] std::string toString() const { return name(); }

#define OPCODE_ARGS1(field) \
    [[nodiscard]] std::string stringArgs() const { return std::format("{}", field); } \
    [[nodiscard]] std::string toString() const { return std::format("{} {}", name(), field); }

#define OPCODE_ARGS1V(field) \
    [[nodiscard]] std::string stringArgs() const { return field.toString(); } \
    [[nodiscard]] std::string toString() const { return std::format("{} {}", name(), field.toString()); }

#define OPCODE_ARGS2(a, b) \
    [[nodiscard]] std::string stringArgs() const { return std::format("{} {}", a, b); } \
    [[nodiscard]] std::string toString() const { return std::format("{} {} {}", name(), a, b); }

#define OPCODE_ARGS3(a, b, c) \
    [[nodiscard]] std::string stringArgs() const { return std::format("{} {} {}", a, b, c); } \
    [[nodiscard]] std::string toString() const { return std::format("{} {} {} {}", name(), a, b, c); }

namespace irgen {
struct StructObject;
struct StructTypeDef;
struct TypeDescriptor;

[[nodiscard]] bool type_handles_equal(
    const std::shared_ptr<TypeDescriptor>& a,
    const std::shared_ptr<TypeDescriptor>& b
);

struct FriendFunctionObject;
class ModuleObject;
class Value;
class VM;
struct IteratorObject;

// IR 指令前向声明（各指令的作用、用法与意义见下方对应 class 前的块注释）
class PUSH;
class ADD;
class MUL;
class SUB;
class DIV;
class NEG;
class DEREF;
class ADDR_OF;
class DEREF_PTR;
class PTR_TO_REF;
class NOT;
class TRUTHY_NOT;
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
class GOTOIFNOT;
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
class BIND_FAST;
class ITER_NEW;
class ITER_NEXT;
class ITER_END;
class THROW;
class ENTER_TRY;
class END_TRY;
class POP_TRY;
class PUSH_EXC;
class EXC_MATCH;
class IS_INSTANCE;
class RETHROW;
class STRUCT_NEW;
class SET_FIELD;
class IS_VECTOR;
class VEC_LEN;
class MATCH_EQ;
class POP;

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
        const std::vector<T>* data_ptr;
        const std::vector<bool>* has_ptr;
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
        using pointer = const value_type*;
        using reference = const value_type;

        Iterator(const std::vector<T>* d, const std::vector<bool>* h, size_t p)
            : data_ptr(d), has_ptr(h), pos(p) {
            next_valid();
        }

        Iterator& operator++() {
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

        bool operator==(const Iterator& other) const {
            return pos == other.pos;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    ArrMap() : has(false), default_value(T{}) {
    }

    explicit ArrMap(const T& def) : has(false), default_value(def) {
    }

    ArrMap(const ArrMap& other)
        : data(other.data),
          has(other.has),
          default_value(other.default_value),
          element_count(other.element_count) {
    }

    ArrMap(ArrMap&& other) noexcept
        : data(std::move(other.data)),
          has(std::move(other.has)),
          default_value(std::move(other.default_value)),
          element_count(other.element_count) {
        other.element_count = 0;
    }

    template<typename MapType>
    explicit ArrMap(const MapType& map, T def = T{}) : has(false), default_value(std::move(def)) {
        for (const auto& [key, value] : map) {
            set(key, value);
        }
    }

    ArrMap(std::initializer_list<std::pair<size_t, T>> init, const T& def = T{})
        : default_value(def) {
        for (const auto& kv : init) {
            set(kv.first, kv.second);
        }
    }

    ArrMap& operator=(const ArrMap& other) {
        if (this != &other) {
            data = other.data;
            has = other.has;
            default_value = other.default_value;
            element_count = other.element_count;
        }
        return *this;
    }

    ArrMap& operator=(ArrMap&& other) noexcept {
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

    void set(size_t key, const T& value) {
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
    void emplace(size_t key, Args&&... args) {
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

    T& operator[](size_t key) {
        if (key >= data.size() || !has[key]) {
            throw std::out_of_range("ArrMap::operator[]: key " + std::to_string(key) + " not found");
        }
        return data[key];
    }

    const T& operator[](size_t key) const {
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

/** 所有可执行 IR 指令的联合体；VM::code 的元素类型。 */
using Opcode = std::variant<
    PUSH,
    ADD,
    MUL,
    SUB,
    DIV,
    NEG,
    DEREF,
    ADDR_OF,
    DEREF_PTR,
    PTR_TO_REF,
    NOT,
    TRUTHY_NOT,
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
    BIND_FAST,
    LABEL,
    GOTO,
    GOTOIF,
    GOTOIFNOT,
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
    ITER_END,
    THROW,
    ENTER_TRY,
    END_TRY,
    POP_TRY,
    PUSH_EXC,
    EXC_MATCH,
    IS_INSTANCE,
    RETHROW,
    STRUCT_NEW,
    SET_FIELD,
    IS_VECTOR,
    VEC_LEN,
    MATCH_EQ,
    POP
>;

class SymbolTable;

/**
 * @brief 函数对象，存储用户定义函数的信息
 */
struct FunctionObject {
    std::vector<std::string> params;                   ///< 函数参数列表
    std::vector<std::optional<std::shared_ptr<TypeDescriptor>>> param_types; ///< 参数类型对象（可选）
    std::vector<std::vector<Opcode>> param_default_ir; ///< 各参数默认值 IR（空表示无默认值）
    std::vector<Opcode> body;                          ///< 函数体的IR指令序列
    size_t location;                                   ///< 函数在源码中的位置
    size_t entry_pc = static_cast<size_t>(-1);       ///< 函数 LABEL 在 bytecode 中的下标（CALL 直达）
    std::string name = "<anonymous>";                  ///< 函数名称
    VM* owner_vm = nullptr;                            ///< 所属虚拟机
    std::vector<SymbolTable> closure;                  ///< 闭包捕获的变量
    bool needs_closure = false;                        ///< 是否需要闭包
    bool needs_symbol_bind = false;                    ///< 是否需将 fast 局部绑定到符号表（供嵌套闭包 LOAD）
    bool is_macro = false;                             ///< 是否为 macro（运行时 AST 展开）
    std::optional<size_t> variadic_param_index;        ///< *param 在 params 中的下标
    size_t fast_local_slot_count = 0;                  ///< fast 局部槽位数（预分配 locals）
    std::optional<size_t> self_local_slot;               ///< 递归自引用 fast 槽（ENTER 时填充）
    mutable std::unordered_map<std::string, size_t> param_name_to_index; ///< kw 参数名 → 下标

    ~FunctionObject();

    [[nodiscard]] bool uses_light_frame() const {
        return !needs_closure && !needs_symbol_bind;
    }

    /**
     * @brief 调用函数
     * @param caller_vm 调用者虚拟机
     * @param args 参数列表
     * @return 返回值
     */
    Value call(VM& caller_vm, const std::vector<Value>& args);
};

[[nodiscard]] Value eval_param_default(VM& vm, const std::vector<Opcode>& ir);

/** @brief 执行一段 IR，返回相对执行前栈顶新增的值（无则 none） */
[[nodiscard]] Value run_ir_snippet(VM& vm, const std::vector<Opcode>& ir);

[[nodiscard]] std::vector<Value> resolve_user_function_args(
    VM& vm,
    const FunctionObject& func_obj,
    const std::vector<Value>& positional,
    const Value& kwargs_value
);

/** @brief 无 kwargs 时共用空字典，避免每次 CALL 分配 */
[[nodiscard]] const Value& empty_kwargs_value();

void invoke_user_function_with_args(
    VM& vm,
    const std::shared_ptr<FunctionObject>& func_obj,
    std::vector<Value> args
);

/** Run a user function to completion on the current VM (same interpreter loop as CALL). */
Value call_user_function_sync(
    VM& vm,
    const std::shared_ptr<FunctionObject>& func_obj,
    std::vector<Value> args
);

/**
 * @brief 函数类型，内置函数的签名
 */
using FunctionType = std::function<Value(VM&, const std::vector<Value>&)>;

class SymbolTable;

/**
 * @brief 槽位引用：变量绑定（透明）或地址引用（& 产生，不透明）
 */
struct Ref {
    std::shared_ptr<Value> value_ptr; ///< 指向池内槽位
    bool opaque = false;              ///< true = 地址视图（print 为 pointer，* 穿透一层）

    explicit Ref(std::shared_ptr<Value> ptr, bool is_opaque = false, CellPool* pool = nullptr);

    [[nodiscard]] bool isAddress() const { return opaque; }

    Value& get() {
        if (!value_ptr) {
            throw RuntimeError("Null reference");
        }
        return *value_ptr;
    }

    [[nodiscard]] const Value& get() const {
        if (!value_ptr) {
            throw RuntimeError("Null reference");
        }
        return *value_ptr;
    }
};

/**
 * @brief 迭代器对象，支持遍历可迭代对象
 */
enum class IteratorKind {
    Sequence,  ///< 内置 vec/text/dict
    UserNext   ///< 用户 __next__ 方法
};

struct IteratorObject {
    IteratorKind kind = IteratorKind::Sequence;
    std::shared_ptr<Value> iterable;      ///< Sequence：被迭代对象
    std::shared_ptr<Value> user_self;     ///< UserNext：绑定 self
    FunctionType user_next;               ///< UserNext：__next__ 调用体
    CellPool* cell_pool = nullptr;
    size_t index = 0;

    explicit IteratorObject() = default;

    explicit IteratorObject(std::shared_ptr<Value> obj) : iterable(std::move(obj)) {
    }

    [[nodiscard]] bool next(VM& vm, Value& out);
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
        None,        ///< 空值
        Number,      ///< 任意精度整数
        Bool,        ///< 布尔值
        String,      ///< 字符串
        Function,    ///< 函数（内置或用户定义）
        Module,      ///< 模块
        Vector,      ///< 向量/列表
        Dictionary,  ///< 字典/映射
        Reference,   ///< 槽位引用（绑定或地址）
        Rational,    ///< 有理数
        Iterator,    ///< 迭代器
        StructObject, ///< struct 实例
        FriendFunction, ///< friend func（__dispatch__ 多分派）
        TypeHandle,    ///< 类型句柄（指向 StructTypeDef，可调用、含 __convert__）
        RuntimeAst     ///< 运行时 AST 值（宏 / eval / quote）
    };

private:
    Type type; ///< 当前值的类型
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
        std::shared_ptr<IteratorObject>,
        std::shared_ptr<StructObject>,
        std::shared_ptr<FriendFunctionObject>,
        std::shared_ptr<TypeDescriptor>,
        std::shared_ptr<RuntimeAstNode>
    > data; ///< 存储实际值的变体

public:
    /**
     * @brief 默认构造函数，创建空值
     */
    Value() : type(Type::None) {
    }

    /**
     * @brief 复制构造函数
     */
    Value(const Value& other) = default;

    /**
     * @brief 移动构造函数
     */
    Value(Value&& other) noexcept : type(other.type), data(std::move(other.data)) {
    }

    /**
     * @brief 从Number类型构造
     */
    explicit Value(const lang::lammp::Number& value)
        : type(Type::Number), data(value) {
    }

    /**
     * @brief 从Number类型移动构造
     */
    explicit Value(lang::lammp::Number&& value)
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

    explicit Value(std::shared_ptr<StructObject> value)
        : type(Type::StructObject), data(std::move(value)) {
    }

    explicit Value(std::shared_ptr<FriendFunctionObject> value)
        : type(Type::FriendFunction), data(std::move(value)) {
    }

    explicit Value(std::shared_ptr<TypeDescriptor> type_desc);

    explicit Value(std::shared_ptr<StructTypeDef> type_def);

    explicit Value(std::shared_ptr<RuntimeAstNode> ast)
        : type(Type::RuntimeAst), data(std::move(ast)) {
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
    explicit Value(const lang::lammp::Rational& value)
        : type(Type::Rational), data(value) {
    }

    /**
     * @brief 从有理数移动构造
     */
    explicit Value(lang::lammp::Rational&& value)
        : type(Type::Rational), data(std::move(value)) {
    }

    /**
     * @brief 创建指向值的引用（移动语义）
     */
    static Value makeRef(Value&& val, CellPool& pool);

    static Value makeRef(const Value& val, CellPool& pool);

    static Value makeRef(std::shared_ptr<Value> val_ptr, CellPool& pool);

    static Value makeEmptyRef(CellPool& pool);

    /** @brief 创建地址槽引用（&expr 的结果，指向已有槽位） */
    static Value makeAddressRef(std::shared_ptr<Value> slot) {
        return Value(Ref(std::move(slot), true));
    }

    /**
     * @brief 函数调用操作符
     * @param vm 虚拟机实例
     * @param args 参数列表
     * @return 返回值
     */
    Value operator()(VM& vm, const std::vector<Value>& args) const {
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
            const Ref& ref = asReference();
            if (ref.opaque) {
                return Type::Reference;
            }
            return ref.get().getType();
        }
        return type;
    }

    /**
     * @brief 获取类型名称字符串
     * @return 类型名称
     */
    [[nodiscard]] std::string type_name() const;

    /**
     * @brief 控制台显示用字符串（如 REPL 求值结果；字符串带引号）
     * @return 字符串表示
     */
    [[nodiscard]] std::string displayString() const;

    /**
     * @brief print/str 等输出用字符串（字符串为原始内容，不带引号）
     * @return 字符串表示
     */
    [[nodiscard]] std::string printString() const;

    /**
     * @brief 转换为字符串表示（同 displayString，供调试与错误信息使用）
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

    [[nodiscard]] const lang::lammp::Number& asNumber() const {
        if (type == Type::Reference) {
            return asReference().get().asNumber();
        }
        if (type == Type::Rational) {
            return const_cast<lang::lammp::Number&>(
                *new lang::lammp::Number(asRational().toNumber()));
        }
        if (type != Type::Number) {
            throw RuntimeError("Value is not a number");
        }
        return std::get<lang::lammp::Number>(data);
    }

    [[nodiscard]] lang::lammp::Number& asNumber() {
        if (type == Type::Reference) {
            return const_cast<lang::lammp::Number&>(asReference().get().asNumber());
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

    [[nodiscard]] const lang::lammp::Rational& asRational() const {
        if (type == Type::Reference) {
            return asReference().get().asRational();
        }
        if (type == Type::Number) {
            return const_cast<lang::lammp::Rational&>(
                *new lang::lammp::Rational(asNumber()));
        }
        if (type != Type::Rational) {
            throw RuntimeError("Value is not a rational");
        }
        return std::get<lang::lammp::Rational>(data);
    }

    [[nodiscard]] lang::lammp::Rational& asRational() {
        if (type == Type::Reference) {
            return const_cast<lang::lammp::Rational&>(asReference().get().asRational());
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

    [[nodiscard]] const std::vector<std::shared_ptr<Value>>& asVector() const {
        const Value& self = deref();
        if (self.type != Type::Vector) {
            throw RuntimeError("Value is not a vector");
        }
        return std::get<std::vector<std::shared_ptr<Value>>>(self.data);
    }

    [[nodiscard]] std::vector<std::shared_ptr<Value>>& asVector() {
        Value& self = deref();
        if (self.type != Type::Vector) {
            throw RuntimeError("Value is not a vector");
        }
        return std::get<std::vector<std::shared_ptr<Value>>>(self.data);
    }

    [[nodiscard]] const std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>& asDictionary() const {
        const Value& self = deref();
        if (self.type != Type::Dictionary) {
            throw RuntimeError("Value is not a dictionary");
        }
        return std::get<std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>>(self.data);
    }

    [[nodiscard]] std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>& asDictionary() {
        Value& self = deref();
        if (self.type != Type::Dictionary) {
            throw RuntimeError("Value is not a dictionary");
        }
        return std::get<std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>>>(self.data);
    }

    [[nodiscard]] Ref& asReference() {
        if (type != Type::Reference) {
            throw RuntimeError("Value is not a reference");
        }
        return std::get<Ref>(data);
    }

    [[nodiscard]] const Ref& asReference() const {
        if (type != Type::Reference) {
            throw RuntimeError("Value is not a reference");
        }
        return std::get<Ref>(data);
    }

    /** @brief 是否为地址槽引用（& 产生） */
    [[nodiscard]] bool isAddressRef() const {
        return type == Type::Reference && asReference().opaque;
    }

    std::shared_ptr<Value> getRefValuePtr() {
        if (type != Type::Reference) {
            throw RuntimeError("Value is not a reference");
        }
        return std::get<Ref>(data).value_ptr;
    }

    Value operator+(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
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

    Value operator-(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() - b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() - b.asRational());
        }
        throw RuntimeError("Unsupported - operation");
    }

    Value operator*(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() * b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() * b.asRational());
        }
        throw RuntimeError("Unsupported * operation");
    }

    Value operator/(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            LOG("exec div...");
            return Value(a.asRational() / b.asRational());
        }
        throw RuntimeError("Unsupported / operation");
    }

    Value operator-() const {
        const Value& self = deref();
        if (self.type == Type::Number) {
            return Value(-self.asNumber());
        }
        if (self.type == Type::Rational) {
            return Value(-self.asRational());
        }
        throw RuntimeError("Unsupported unary - operation");
    }

    Value operator!() const {
        const Value& self = deref();
        if (self.type == Type::Bool) {
            return Value(!self.asBool());
        }
        throw RuntimeError("Unsupported ! operation");
    }

    Value operator&&(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Bool && b.type == Type::Bool) {
            return Value(a.asBool() && b.asBool());
        }
        throw RuntimeError("Unsupported && operation");
    }

    Value operator||(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Bool && b.type == Type::Bool) {
            return Value(a.asBool() || b.asBool());
        }
        throw RuntimeError("Unsupported || operation");
    }

    Value operator<(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() < b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() < b.asRational());
        }
        throw RuntimeError("Unsupported < operation");
    }

    Value operator<=(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() <= b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() <= b.asRational());
        }
        throw RuntimeError("Unsupported <= operation");
    }

    Value operator>(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() > b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() > b.asRational());
        }
        throw RuntimeError("Unsupported > operation");
    }

    Value operator>=(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() >= b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() >= b.asRational());
        }
        throw RuntimeError("Unsupported >= operation");
    }

    bool operator==(const Value& other) {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Reference && a.asReference().opaque &&
            b.type == Type::Reference && b.asReference().opaque) {
            return a.asReference().value_ptr.get() == b.asReference().value_ptr.get();
        }
        if (a.type == Type::Reference && a.asReference().opaque) {
            return false;
        }
        if (b.type == Type::Reference && b.asReference().opaque) {
            return false;
        }
        if (a.type == Type::Number && b.type == Type::Number) {
            return a.asNumber() == b.asNumber();
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return a.asRational() == b.asRational();
        }
        if (a.type == Type::Bool && b.type == Type::Bool) {
            return a.asBool() == b.asBool();
        }
        if (a.type == Type::String && b.type == Type::String) {
            return a.asString() == b.asString();
        }
        if (a.type == Type::TypeHandle && b.type == Type::TypeHandle) {
            return type_handles_equal(a.asTypeDesc(), b.asTypeDesc());
        }
        throw RuntimeError(
            std::format("Unsupported == operation, left = {}, right = {}", a.type_name(), b.type_name())
        );
    }

    Value operator!=(const Value& other) const {
        const Value& a = deref();
        const Value& b = other.deref();
        if (a.type == Type::Reference && a.asReference().opaque &&
            b.type == Type::Reference && b.asReference().opaque) {
            return Value(a.asReference().value_ptr.get() != b.asReference().value_ptr.get());
        }
        if (a.type == Type::Reference && a.asReference().opaque) {
            return Value(true);
        }
        if (b.type == Type::Reference && b.asReference().opaque) {
            return Value(true);
        }
        if (a.type == Type::Number && b.type == Type::Number) {
            return Value(a.asNumber() != b.asNumber());
        }
        if ((a.type == Type::Rational || a.type == Type::Number) &&
            (b.type == Type::Rational || b.type == Type::Number)) {
            return Value(a.asRational() != b.asRational());
        }
        if (a.type == Type::Bool && b.type == Type::Bool) {
            return Value(a.asBool() != b.asBool());
        }
        if (a.type == Type::String && b.type == Type::String) {
            return Value(a.asString() != b.asString());
        }
        if (a.type == Type::TypeHandle && b.type == Type::TypeHandle) {
            return Value(!type_handles_equal(a.asTypeDesc(), b.asTypeDesc()));
        }
        throw RuntimeError("Unsupported != operation");
    }

    [[nodiscard]] std::shared_ptr<FunctionObject> asFunctionObject() const {
        const Value& self = deref();
        if (self.type != Type::Function) {
            throw RuntimeError("Value is not a function");
        }
        if (self.data.index() == 4) {
            return std::get<std::shared_ptr<FunctionObject>>(self.data);
        }
        throw RuntimeError("Value is not a user-defined function");
    }

    [[nodiscard]] bool isUserFunction() const {
        const Value& self = deref();
        return self.type == Type::Function &&
               self.data.index() == 4;
    }

    [[nodiscard]] bool isBuiltinFunction() const {
        const Value& self = deref();
        return self.type == Type::Function &&
               self.data.index() == 3;
    }

    [[nodiscard]] bool isRuntimeAst() const {
        return deref().type == Type::RuntimeAst;
    }

    [[nodiscard]] const RuntimeAstNode& asRuntimeAst() const {
        const Value& self = deref();
        if (self.type != Type::RuntimeAst) {
            throw RuntimeError("Value is not AST");
        }
        const auto& ptr = std::get<std::shared_ptr<RuntimeAstNode>>(self.data);
        if (!ptr) {
            throw RuntimeError("AST value is null");
        }
        return *ptr;
    }

    [[nodiscard]] RuntimeAstNode& asRuntimeAst() {
        Value& self = deref();
        if (self.type != Type::RuntimeAst) {
            throw RuntimeError("Value is not AST");
        }
        auto& ptr = std::get<std::shared_ptr<RuntimeAstNode>>(self.data);
        if (!ptr) {
            throw RuntimeError("AST value is null");
        }
        return *ptr;
    }

#undef DEFINE_AS_METHOD

    [[nodiscard]] bool isNone() const {
        return deref().type == Type::None;
    }

    [[nodiscard]] bool isTruthy() const {
        const Value& self = deref();
        switch (self.type) {
            case Type::None:
                return false;
            case Type::Bool:
                return self.asBool();
            case Type::Number:
                return !self.asNumber().isZero();
            case Type::Rational:
                return self.asRational() != lang::lammp::Rational();
            case Type::String:
                return !self.asString().empty();
            case Type::Vector:
                return !self.asVector().empty();
            case Type::Dictionary:
                return !self.asDictionary().empty();
            default:
                return true;
        }
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

    [[nodiscard]] bool isStruct() const {
        return deref().type == Type::StructObject;
    }

    [[nodiscard]] bool isIterator() const {
        return deref().type == Type::Iterator;
    }

    [[nodiscard]] bool isTypeHandle() const {
        return deref().type == Type::TypeHandle;
    }

    [[nodiscard]] bool isFriendFunction() const {
        return deref().type == Type::FriendFunction;
    }

    [[nodiscard]] bool isCallable() const {
        return isFunction() || isTypeHandle() || isFriendFunction();
    }

    [[nodiscard]] const std::shared_ptr<FriendFunctionObject>& asFriendFunction() const;

    [[nodiscard]] std::shared_ptr<FriendFunctionObject>& asFriendFunction();

    [[nodiscard]] const std::shared_ptr<TypeDescriptor>& asTypeDesc() const;

    [[nodiscard]] std::shared_ptr<TypeDescriptor>& asTypeDesc();

    [[nodiscard]] const std::shared_ptr<StructTypeDef>& asTypeDef() const;

    [[nodiscard]] std::shared_ptr<StructTypeDef>& asTypeDef();

    [[nodiscard]] const std::shared_ptr<StructObject>& asStruct() const;

    [[nodiscard]] std::shared_ptr<StructObject>& asStruct();

    [[nodiscard]] bool isReference() const { return type == Type::Reference; }

    void set(const Value& value) const {
        if (type != Type::Reference) {
            throw RuntimeError("Cannot set a non-reference value");
        }
        if (value.type == Type::Reference && value.asReference().opaque) {
            *std::get<Ref>(data).value_ptr = value;
        } else {
            *std::get<Ref>(data).value_ptr = value.deref();
        }
    }

    [[nodiscard]] const Value& deref() const {
        if (type != Type::Reference) {
            return *this;
        }
        const Ref& ref = std::get<Ref>(data);
        if (ref.opaque) {
            return *this;
        }
        if (!ref.value_ptr) {
            throw RuntimeError("Null reference");
        }
        const Value* current = ref.value_ptr.get();
        std::unordered_set<const Value*> visited;
        while (current->type == Type::Reference) {
            if (visited.contains(current)) {
                throw RuntimeError("Circular reference detected");
            }
            visited.insert(current);
            const Ref& inner = std::get<Ref>(current->data);
            if (inner.opaque) {
                return *current;
            }
            current = inner.value_ptr.get();
        }
        return *current;
    }

    Value& deref() {
        if (type != Type::Reference) {
            return *this;
        }
        Ref& ref = std::get<Ref>(data);
        if (ref.opaque) {
            return *this;
        }
        if (!ref.value_ptr) {
            throw RuntimeError("Null reference");
        }
        Value* current = ref.value_ptr.get();
        std::unordered_set<Value*> visited;
        while (current->type == Type::Reference) {
            if (visited.contains(current)) {
                throw RuntimeError("Circular reference detected");
            }
            visited.insert(current);
            Ref& inner = std::get<Ref>(current->data);
            if (inner.opaque) {
                return *current;
            }
            current = inner.value_ptr.get();
        }
        return *current;
    }

    Value& safe_deref() {
        std::unordered_set<Value*> visited; // 用裸指针，避免 shared_ptr 拷贝
        auto current = this;

        while (current->type == Type::Reference) {
            if (visited.contains(current)) {
                // 先检查当前节点
                throw RuntimeError("Circular reference detected");
            }
            visited.insert(current);
            current = std::get<Ref>(current->data).value_ptr.get();
        }

        return *current;
    }

    friend std::ostream& operator<<(std::ostream& os, const Value& value) {
        os << value.displayString();
        return os;
    }

    bool isNumber() const {
        return deref().type == Type::Number;
    };
};

struct StructFieldDef {
    std::string name;
    std::string type_name; ///< 仅用于错误信息等展示；匹配用 type_desc
    std::shared_ptr<TypeDescriptor> type_desc;
    bool has_type_annotation = false;
    bool mutable_field = false;
    bool has_default = false;
    Value default_value;
};

enum class TypeKind {
    User,       ///< 用户 struct（有字段、可实例化）
    Primitive   ///< 内置类型名（text/num/…，主要作转换目标）
};

enum class VarianceMode;

struct StructTypeParamDef {
    std::string name;
    std::shared_ptr<TypeDescriptor> bound;
    VarianceMode variance;
};

struct StructTypeDef {
    std::string name;
    std::string base_name;
    TypeKind kind = TypeKind::User;
    bool typed = false;
    bool is_generic = false;
    std::vector<StructTypeParamDef> type_params;
    std::vector<StructFieldDef> fields;
    std::unordered_map<std::string, std::shared_ptr<FunctionObject>> methods;
    /// 类型转换 friend func（`__convert__.__dispatch__`）
    std::shared_ptr<FriendFunctionObject> convert_func;

    void ensure_convert_func();

    [[nodiscard]] size_t required_field_count() const {
        size_t n = 0;
        for (const auto& f : fields) {
            if (!f.has_default) {
                ++n;
            }
        }
        return n;
    }
};

struct StructObject {
    std::shared_ptr<StructTypeDef> type;
    std::vector<Value> slots;
};

/**
 * ITER_NEW — 为 for-in 创建迭代器。
 *
 * 作用：弹出栈顶可迭代对象，构造 IteratorObject 并压回（保留原对象语义见 emit）。
 * 何时使用：for 循环进入前、对每个绑定序列的迭代驱动。
 * 意义：将 for-in 降维为「迭代器 + 条件跳转」模式，与 ITER_NEXT/ITER_END 成套。
 */
class ITER_NEW {
public:
    OPCODE_META(ITER_NEW)
    OPCODE_ARGS0()

    ITER_NEW() = default;

    void emit(VM& vm) const;
};

/**
 * ITER_NEXT — 推进迭代并压入 (has_next, value)。
 *
 * 作用：弹出迭代器，若还有下一项则压入 true 与当前值，否则压入 false；
 *       迭代器对象留在栈上供下一轮使用（具体栈序见 generator/emit）。
 * 何时使用：循环体每次迭代开头、配合 GOTOIF 判断退出。
 * 意义：统一字符串/向量等类型的遍历协议。
 */
class ITER_NEXT {
public:
    OPCODE_META(ITER_NEXT)
    OPCODE_ARGS0()

    ITER_NEXT() = default;

    void emit(VM& vm) const;
};

/**
 * ITER_END — 结束迭代，清理迭代器。
 *
 * 作用：弹出栈顶迭代器对象，释放/for 循环收尾。
 * 何时使用：for 循环正常结束或 break 汇合后的清理点。
 * 意义：与 ITER_NEW 配对，避免迭代状态泄漏到外层栈帧。
 */
class ITER_END {
public:
    OPCODE_META(ITER_END)
    OPCODE_ARGS0()

    ITER_END() = default;

    void emit(VM& vm) const;
};

/**
 * THROW — 抛出用户异常对象（可被 try/catch 捕获）。
 */
class THROW {
public:
    OPCODE_META(THROW)
    OPCODE_ARGS0()

    THROW() = default;

    void emit(VM& vm) const;
};

/** try 块入口：登记 catch/else/end 标签与当前栈深度。 */
class ENTER_TRY {
public:
    OPCODE_META(ENTER_TRY)
    size_t catch_label = 0;
    size_t else_label = 0;
    size_t end_label = 0;
    OPCODE_ARGS3(catch_label, else_label, end_label)

    ENTER_TRY(size_t catch_l, size_t else_l, size_t end_l)
        : catch_label(catch_l), else_label(else_l), end_label(end_l) {
    }

    void emit(VM& vm) const;
};

/** try 体正常结束：跳转到 else（若有）或 end。 */
class END_TRY {
public:
    OPCODE_META(END_TRY)
    OPCODE_ARGS0()

    END_TRY() = default;

    void emit(VM& vm) const;
};

/** try/catch/else 结束：弹出 try 帧并清除 active_exception。 */
class POP_TRY {
public:
    OPCODE_META(POP_TRY)
    OPCODE_ARGS0()

    POP_TRY() = default;

    void emit(VM& vm) const;
};

/** 将当前 active_exception 压栈（供 catch 绑定）。 */
class PUSH_EXC {
public:
    OPCODE_META(PUSH_EXC)
    OPCODE_ARGS0()

    PUSH_EXC() = default;

    void emit(VM& vm) const;
};

/** 判断 active_exception 是否为指定类型（含子类）。 */
class EXC_MATCH {
public:
    OPCODE_META(EXC_MATCH)
    size_t type_name_id = 0;
    OPCODE_ARGS1(type_name_id)

    explicit EXC_MATCH(std::string type_name);

    void emit(VM& vm) const;
};

/** 判断栈顶值是否为指定 struct 类型（含子类）。 */
class IS_INSTANCE {
public:
    OPCODE_META(IS_INSTANCE)
    size_t type_name_id = 0;
    OPCODE_ARGS1(type_name_id)

    explicit IS_INSTANCE(std::string type_name);

    void emit(VM& vm) const;
};

/** 重新抛出 active_exception，交给外层 try。 */
class RETHROW {
public:
    OPCODE_META(RETHROW)
    OPCODE_ARGS0()

    RETHROW() = default;

    void emit(VM& vm) const;
};

/**
 * @brief 符号表，用于管理变量的存储和访问
 */
class SymbolTable {
public:
    ArrMap<std::shared_ptr<Value>> symbols; ///< 符号值映射
    ArrMap<bool> constants;                 ///< 常量标记映射

    /**
     * @brief 默认构造函数
     */
    SymbolTable() = default;

    /**
     * @brief 从map构造符号表
     */
    explicit SymbolTable(const std::map<size_t, std::shared_ptr<Value>>& symbols)
        : symbols(symbols) {
    }

    /**
     * @brief 从map移动构造符号表
     */
    explicit SymbolTable(std::map<size_t, std::shared_ptr<Value>>&& symbols)
        : symbols(symbols) {
    }

    /**
     * @brief 设置符号值
     * @param id 符号ID
     * @param value 值
     * @param pool 对象池
     */
    void set(size_t id, const Value& value, CellPool& pool);

    /**
     * @brief 设置符号值（智能指针版本）
     * @param id 符号ID
     * @param value 值的智能指针
     */
    void set(size_t id, const std::shared_ptr<Value>& value);

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
    std::vector<Stackable> data; ///< 存储栈元素的向量

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
    void push(const Stackable& value) {
        data.push_back(value);
    }

    /**
     * @brief 移动压入元素
     */
    void push(Stackable&& value) {
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
        Stackable value = std::move(data.back());
        data.pop_back();
        return value;
    }

    /**
     * @brief 获取栈顶元素
     * @return 栈顶元素引用
     */
    [[nodiscard]] const Stackable& top() const {
        return data.back();
    }

    [[nodiscard]] Stackable& top() {
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

    /** @brief 只读访问底层元素（供 GC 根扫描等） */
    [[nodiscard]] const std::vector<Stackable>& items() const {
        return data;
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
                   data.rbegin(),
                   data.rend(),
                   std::string{},
                   [i = data.size() - 1](std::string acc, const auto& elem) mutable {
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
    std::vector<Value> data; ///< 寄存器数据

    /**
     * @brief 构造函数
     * @param data 初始数据
     */
    explicit Register(std::vector<Value> const& data = std::vector<Value>())
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
    static constexpr size_t SLOT_COUNT = 16; ///< 槽位数量

    /**
     * @brief 作用域内的缓存槽
     */
    struct Scope {
        std::array<std::pair<size_t, std::shared_ptr<Value>>, SLOT_COUNT> slots; ///< 槽数组
        std::unordered_map<size_t, size_t> id_to_index;                          ///< ID到索引的映射
        size_t next_slot = 0;                                                    ///< 下一个可用槽位

        Scope() {
            for (auto& id : slots | std::views::keys) {
                id = 0;
            }
        }
    };

    std::vector<Scope> scopes; ///< 作用域栈

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
    void add(size_t id, const std::shared_ptr<Value>& val) {
        auto& scope = scopes.back();
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
        const auto& scope = scopes.back();
        const auto it = scope.id_to_index.find(id);
        if (it == scope.id_to_index.end()) {
            return std::nullopt;
        }
        return scope.slots[it->second].second;
    }

    /**
     * @brief 检查缓存是否包含指定ID
     * @param id 符号ID
     * @return 是否包含
     */
    [[nodiscard]] bool contains(size_t id) const {
        const auto& scope = scopes.back();
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
        if (scopes.size() <= 1) {
            return;
        }
        scopes.pop_back();
    }

    /** @brief 当前作用域栈深度（含全局层） */
    [[nodiscard]] size_t scope_depth() const {
        return scopes.size();
    }

    /**
     * @brief 清空所有缓存
     */
    void clear() {
        scopes.clear();
        scopes.emplace_back();
    }

    /** @brief 收集所有作用域中缓存的槽位指针（供 GC 根扫描） */
    [[nodiscard]] std::vector<std::pair<size_t, std::shared_ptr<Value>>> allEntries() const {
        std::vector<std::pair<size_t, std::shared_ptr<Value>>> out;
        for (const Scope& scope : scopes) {
            for (const auto& [slot_id, val] : scope.slots) {
                if (slot_id != 0 && val) {
                    out.emplace_back(slot_id, val);
                }
            }
        }
        return out;
    }
};

// -------------------------------------------------------------------------
// IR 指令类：栈式字节码，由 codegen 生成、VM::run 解释执行。
// 约定：二元运算与比较从栈顶弹出右操作数再左操作数；CALL 栈顶为可调用对象。
// -------------------------------------------------------------------------

/**
 * PUSH — 将常量或函数对象压入操作数栈。
 *
 * 作用：operands[0] 为待压入的 Value（字面量、标签占位、FunctionObject 等）。
 * 何时使用：字面量、函数体定义末尾把 FunctionObject 压栈以便装饰器 CALL、
 *           或任何需要把已知值送入后续指令的场景。
 * 意义：栈机的基础数据来源；对 FunctionObject 还会在首次 PUSH 时捕获闭包环境。
 */
class PUSH {
public:
    OPCODE_META(PUSH)
    Value val;
    OPCODE_ARGS1V(val)

    explicit PUSH(Value v) : val(std::move(v)) {
    }

    void emit(VM& vm) const;
};

/**
 * ADD — 二元加法。
 *
 * 作用：弹出栈顶 b、次顶 a，将 a + b 压回（支持 Number 拼接与 String 连接）。
 * 何时使用：加法表达式、字符串拼接的 IR  lowering。
 * 意义：对应语言中的 + 运算符；类型规则在 Value::operator+ 中实现。
 */
class ADD {
public:
    OPCODE_META(ADD)
    OPCODE_ARGS0()

    ADD() = default;

    void emit(VM& vm);
};

/**
 * MUL — 二元乘法.
 *
 * 作用：弹出 b、a，压入 a * b（大整数经 lammp::Number）。
 * 何时使用：乘法表达式、部分代数化简路径。
 * 意义：算术核心指令之一，与 SUB/DIV 成组。
 */
class MUL {
public:
    OPCODE_META(MUL)
    OPCODE_ARGS0()

    MUL() = default;

    void emit(VM& vm);
};

/**
 * SUB — 二元减法.
 *
 * 作用：弹出 b、a，压入 a - b。
 * 何时使用：减法表达式、循环/索引中的递减。
 * 意义：算术指令；注意栈顺序为「次顶减栈顶」。
 */
class SUB {
public:
    OPCODE_META(SUB)
    OPCODE_ARGS0()

    SUB() = default;

    void emit(VM& vm);
};

/**
 * DIV — 二元除法.
 *
 * 作用：弹出 b、a，压入 a / b（整数除法或 Number 语义由 Value 定义）。
 * 何时使用：除法表达式。
 * 意义：完成四则运算集；除零等行为在运行时 Value 层处理。
 */
class DIV {
public:
    OPCODE_META(DIV)
    OPCODE_ARGS0()

    DIV() = default;

    void emit(VM& vm);
};

/**
 * NEG — 一元取负.
 *
 * 作用：弹出栈顶，压入其相反数。
 * 何时使用：前缀 - 表达式。
 * 意义：一元算术，与 NOT 等一元逻辑指令对称。
 */
class NEG {
public:
    OPCODE_META(NEG)
    OPCODE_ARGS0()

    NEG() = default;

    void emit(VM& vm);
};

/**
 * DEREF — 解引用.
 *
 * 作用：若栈顶为 Reference，则替换为所指向的实际 Value；否则保持不变。
 * 何时使用：读取变量值（LOAD / LOAD_FAST 之后）、需要 RVALUE 的表达式位置。
 * 意义：区分「名字/槽位里的引用」与「参与运算的值」；赋值走 STORE 不经 DEREF。
 */
class DEREF {
public:
    OPCODE_META(DEREF)
    OPCODE_ARGS0()

    DEREF() = default;

    void emit(VM& vm);
};

/**
 * ADDR_OF — 取左值地址，生成地址槽引用.
 */
class ADDR_OF {
public:
    OPCODE_META(ADDR_OF)
    OPCODE_ARGS0()

    ADDR_OF() = default;

    void emit(VM& vm) const;
};

/**
 * DEREF_PTR — 读取指针指向的值（RVALUE）.
 */
class DEREF_PTR {
public:
    OPCODE_META(DEREF_PTR)
    OPCODE_ARGS0()

    DEREF_PTR() = default;

    void emit(VM& vm) const;
};

/**
 * PTR_TO_REF — 将地址槽引用转为可 STORE 的绑定槽.
 */
class PTR_TO_REF {
public:
    OPCODE_META(PTR_TO_REF)
    OPCODE_ARGS0()

    PTR_TO_REF() = default;

    void emit(VM& vm) const;
};

/**
 * NOT — 逻辑非.
 *
 * 作用：弹出栈顶，压入布尔取反。
 * 何时使用：前缀 ! 、条件取反。
 * 意义：一元逻辑；与 AND/OR 配合构成短路逻辑的底层（若未在 AST 层短路）。
 */
class NOT {
public:
    OPCODE_META(NOT)
    OPCODE_ARGS0()

    NOT() = default;

    void emit(VM& vm);
};

/**
 * TRUTHY_NOT — Python 风格 not，按 truthiness 取反并压入 Bool。
 */
class TRUTHY_NOT {
public:
    OPCODE_META(TRUTHY_NOT)
    OPCODE_ARGS0()

    TRUTHY_NOT() = default;

    void emit(VM& vm);
};

/**
 * AND — 逻辑与.
 *
 * 作用：弹出 b、a，压入 a && b 的布尔结果。
 * 何时使用：&& 表达式（若未完全在 codegen 层短路）。
 * 意义：布尔组合；实际短路可能由 GOTOIF 实现，本指令为直译版本。
 */
class AND {
public:
    OPCODE_META(AND)
    OPCODE_ARGS0()

    AND() = default;

    void emit(VM& vm);
};

/**
 * OR — 逻辑或.
 *
 * 作用：弹出 b、a，压入 a || b 的布尔结果。
 * 何时使用：|| 表达式。
 * 意义：与 AND 对称；控制流中的「或」也常配合 GOTOIF。
 */
class OR {
public:
    OPCODE_META(OR)
    OPCODE_ARGS0()

    OR() = default;

    void emit(VM& vm);
};

/**
 * EQ — 相等比较.
 *
 * 作用：弹出 b、a，压入 (a == b) 的布尔值。
 * 何时使用：== 表达式、条件判断。
 * 意义：比较指令族；类型相同时按 Value 相等语义比较。
 */
class EQ {
public:
    OPCODE_META(EQ)
    OPCODE_ARGS0()

    EQ() = default;

    void emit(VM& vm);
};

/**
 * NEQ — 不等比较.
 *
 * 作用：弹出 b、a，压入 (a != b)。
 * 何时使用：!= 表达式。
 * 意义：与 EQ 互补，常用于循环与分支条件。
 */
class NEQ {
public:
    OPCODE_META(NEQ)
    OPCODE_ARGS0()

    NEQ() = default;

    void emit(VM& vm);
};

/**
 * LT — 小于比较.
 *
 * 作用：弹出 b、a，压入 (a < b)。
 * 何时使用：< 表达式、排序与循环边界。
 * 意义：有序类型（Number 等）上的关系运算。
 */
class LT {
public:
    OPCODE_META(LT)
    OPCODE_ARGS0()

    LT() = default;

    void emit(VM& vm);
};

/**
 * LTE — 小于等于比较.
 *
 * 作用：弹出 b、a，压入 (a <= b)。
 * 何时使用：<= 表达式（如 fib、while 条件）。
 * 意义：与 LT/GT/GTE 共同支持完整比较运算符集。
 */
class LTE {
public:
    OPCODE_META(LTE)
    OPCODE_ARGS0()

    LTE() = default;

    void emit(VM& vm);
};

/**
 * GT — 大于比较.
 *
 * 作用：弹出 b、a，压入 (a > b)。
 * 何时使用：> 表达式。
 * 意义：关系运算；栈顺序与数学写法 a > b 一致（a 在次顶）。
 */
class GT {
public:
    OPCODE_META(GT)
    OPCODE_ARGS0()

    GT() = default;

    void emit(VM& vm);
};

/**
 * GTE — 大于等于比较.
 *
 * 作用：弹出 b、a，压入 (a >= b)。
 * 何时使用：>= 表达式。
 * 意义：比较族最后一个；常用于循环与范围判断。
 */
class GTE {
public:
    OPCODE_META(GTE)
    OPCODE_ARGS0()

    GTE() = default;

    void emit(VM& vm);
};

/**
 * STORE — 通过引用槽写入变量.
 *
 * 作用：栈顶为 ref_slot，次顶为 data；将 data 写入 ref 指向的单元（遵守 const 约束）。
 * 何时使用：模块级 let 赋值、NEW_VAR 后的首次绑定、BIND_FAST 前的中间步骤。
 * 意义：实现「名字指向可变单元」；与 LOAD+DEREF 读路径成对。
 */
class STORE {
public:
    OPCODE_META(STORE)
    OPCODE_ARGS0()

    STORE() = default;

    void emit(VM& vm) const;
};

/**
 * LOAD — 按名字加载变量引用.
 *
 * 作用：var_id 为变量名（编译后变为 string pool id）；在 cache、
 *       symbol_stack（含闭包捕获层）、main_module 中查找，压入 Reference。
 * 何时使用：读取模块/闭包变量、外层捕获的 fast 参数（经 BIND_FAST 注册后）。
 * 意义：名字解析的运行时入口；读值需再跟 DEREF。
 */
class LOAD {
public:
    OPCODE_META(LOAD)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit LOAD(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * LABEL — 跳转锚点（无运行时效果）。
 *
 * 作用：在 label_table 中登记当前 PC，供 GOTO/GOTOIF 解析目标。
 * 何时使用：函数体入口、循环头/尾、if 分支汇合点、跳过函数体定义。
 * 意义：将控制流与线性指令序列分离；scan_labels 在 run 前填充表。
 */
class LABEL {
public:
    OPCODE_META(LABEL)
    size_t label_id;
    OPCODE_ARGS1(label_id)

    explicit LABEL(size_t label_id) : label_id(label_id) {
    }

    void emit(VM& vm);

    void set_label(VM& vm, std::optional<size_t> on = std::nullopt) const;
};

/**
 * GOTO — 无条件跳转.
 *
 * 作用：将 PC 设为 label_table[label_id]。
 * 何时使用：跳过函数体、循环回边、if 结束汇合、程序初始化跳转到 main 段。
 * 意义：结构化控制流编译后的基本块连接手段。
 */
class GOTO {
public:
    OPCODE_META(GOTO)
    size_t label_id;
    OPCODE_ARGS1(label_id)

    explicit GOTO(size_t label_id) : label_id(label_id) {
    }

    void emit(VM& vm) const;
};

/**
 * GOTOIF — 条件为真时跳转.
 *
 * 作用：弹出栈顶条件；若为真则跳到 label_id，否则 fall-through。
 * 何时使用：if/while/for 分支、短路逻辑、循环退出判断。
 * 意义：控制流核心；将布尔结果转为 PC 变更。
 */
class GOTOIF {
public:
    OPCODE_META(GOTOIF)
    size_t label_id;
    OPCODE_ARGS1(label_id)

    explicit GOTOIF(size_t label_id) : label_id(label_id) {
    }

    void emit(VM& vm) const;
};

/**
 * GOTOIFNOT — 条件为假时跳转。
 */
class GOTOIFNOT {
public:
    OPCODE_META(GOTOIFNOT)
    size_t label_id;
    OPCODE_ARGS1(label_id)

    explicit GOTOIFNOT(size_t label_id) : label_id(label_id) {}

    void emit(VM& vm) const;
};

/**
 * ENTER_SCOPE — 进入词法作用域.
 *
 * 作用：在 symbol_stack 与 locals_stack 各压入一层新表，cache 进入子作用域。
 * 何时使用：函数体、块语句、需要隔离局部绑定的区域。
 * 意义：支撑嵌套作用域与 fast 局部槽；与 LEAVE_SCOPE 成对。
 */
class ENTER_SCOPE {
public:
    OPCODE_META(ENTER_SCOPE)
    OPCODE_ARGS0()

    ENTER_SCOPE() = default;

    void emit(VM& vm);
};

/**
 * LEAVE_SCOPE — 离开词法作用域.
 *
 * 作用：弹出 symbol_stack 与 locals_stack 栈顶一层，cache 离开子作用域。
 * 何时使用：块结束、函数 RET 之后（或 RET_THEN_LEAVE_SCOPE 内）。
 * 意义：释放本层绑定；闭包已捕获的环境不受此弹出影响（在 closure 向量中）。
 */
class LEAVE_SCOPE {
public:
    OPCODE_META(LEAVE_SCOPE)
    OPCODE_ARGS0()

    LEAVE_SCOPE() = default;

    void emit(VM& vm);
};

/**
 * CALL — 调用函数.
 *
 * 作用：栈顶为 callable；弹出实参（个数由 arg_count 指定）后执行。
 *       用户函数：压入闭包 scope、跳转至 FunctionObject::location；
 *       内建函数：直接 C++ 回调并压回返回值。
 * 何时使用：调用表达式、装饰器包装（decos.log(func)）、方法调用链末端。
 * 意义：执行模型中心；连接 codegen 与运行时 builtins/UserFunction。
 */
class CALL {
public:
    OPCODE_META(CALL)
    size_t arg_count;
    bool has_kwargs = false;
    uint64_t splat_mask = 0;
    OPCODE_ARGS2(arg_count, has_kwargs)

    CALL(size_t count, const bool kwargs = false, const uint64_t splats = 0)
        : arg_count(count), has_kwargs(kwargs), splat_mask(splats) {}

    void emit(VM& vm) const;
};

/**
 * RET — 从用户函数返回.
 *
 * 作用：栈顶（或约定位置）为返回值；恢复 call_stack 中的 PC，弹出闭包 scope。
 * 何时使用：return 语句、do 表达式返回内部函数对象（无 LEAVE 的 RET）。
 * 意义：与用户函数 CALL 配对；内建函数通常不经过 RET。
 */
class RET {
public:
    OPCODE_META(RET)
    OPCODE_ARGS0()

    RET() = default;

    void emit(VM& vm);
};

/**
 * FINDMOD — 按名导入/查找子模块.
 *
 * 作用：module_id 为模块名字符串；通过 main_module->import 加载 .lm 并压入 ModuleObject。
 * 何时使用：import 语句、访问 std 等子模块前的模块解析（若 codegen 生成）。
 * 意义：模块系统的运行时链接点；与 GETATTR 组合实现 std.decos.log 等路径。
 */
class FINDMOD {
public:
    OPCODE_META(FINDMOD)
    size_t module_id;
    OPCODE_ARGS1(module_id)

    explicit FINDMOD(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * GETATTR — 读取成员：模块/struct 字段，或内置类型方法绑定.
 *
 * 栈顶为接收者：绑定 Reference（左值槽位）或 rvalue 对象。
 */
class GETATTR {
public:
    OPCODE_META(GETATTR)
    size_t name_id;
    OPCODE_ARGS1(name_id)

    explicit GETATTR(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * VEC_NEW — 构造向量字面量.
 *
 * 作用：从栈上弹出 n 个元素（操作数指定个数），组装 Vector 并压回。
 * 何时使用：向量字面量 [a, b, c] 的 codegen。
 * 意义：复合字面量构造；元素顺序与栈弹出顺序相反需注意。
 */
class VEC_NEW {
public:
    OPCODE_META(VEC_NEW)
    size_t count;
    OPCODE_ARGS1(count)

    explicit VEC_NEW(size_t count) : count(count) {
    }

    void emit(VM& vm) const;
};

/**
 * DICT_NEW — 构造字典字面量.
 *
 * 作用：从栈上弹出成对的 key/value（个数由操作数指定），构建 Dictionary。
 * 何时使用：字典字面量 { k: v, ... }。
 * 意义：与 VEC_NEW 类似，为聚合类型提供统一构造指令。
 */
class DICT_NEW {
public:
    OPCODE_META(DICT_NEW)
    size_t count;
    OPCODE_ARGS1(count)

    explicit DICT_NEW(size_t count) : count(count) {
    }

    void emit(VM& vm) const;
};

/**
 * INDEX — 下标/索引访问.
 *
 * 作用：弹出 index、container，将 container[index] 压栈（向量、字符串等）。
 * 何时使用：a[i]、字符串字符访问（若语言支持）。
 * 意义：随机访问原语；赋值到索引若支持则需另配 STORE 变体或专用指令。
 */
class INDEX {
public:
    OPCODE_META(INDEX)
    OPCODE_ARGS0()

    INDEX() = default;

    void emit(VM& vm) const;
};

/**
 * STORE_ARG — 向当前作用域写入命名参数/变量（直接值）。
 *
 * 作用：弹出栈顶值，按变量 id 写入 symbol_stack 顶层（非引用槽模式）。
 * 何时使用：特定参数绑定路径、与模块加载相关的符号初始化（较少见）。
 * 意义：区别于 STORE（经 ref）的另一种绑定方式；具体语义见 emit 实现。
 */
class STORE_ARG {
public:
    OPCODE_META(STORE_ARG)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit STORE_ARG(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * NEW_VAR — 在当前作用域创建可变变量引用槽.
 *
 * 作用：分配空 Reference，注册到 symbol_stack，并将 ref 压栈供后续 STORE。
 * 何时使用：模块级 let、export 函数名绑定（func 声明后的 STORE）。
 * 意义：名字与存储分离；赋值必须先 NEW_VAR（或 OR_LOAD）再 STORE。
 */
class NEW_VAR {
public:
    OPCODE_META(NEW_VAR)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit NEW_VAR(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * NEW_CONST — 在当前作用域创建常量引用槽.
 *
 * 作用：与 NEW_VAR 类似，但标记为 constant，后续 STORE 若已有值可拒绝修改。
 * 何时使用：const 声明、不可重新绑定的模块级名字。
 * 意义：在运行时保留「只赋一次」的约束，配合 STORE 的 const 检查。
 */
class NEW_CONST {
public:
    OPCODE_META(NEW_CONST)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit NEW_CONST(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * NEW_INTERN_VAR — 在函数/块内创建内部可变绑定.
 *
 * 作用：在当前 ENTER_SCOPE 层注册 intern 变量 ref，并压栈 ref。
 * 何时使用：函数内 let、for 循环迭代器槽位名、需在 symbol 表可见的 intern 名。
 * 意义：桥接 fast 局部与名字查找；常与 LOAD/STORE 或循环 IR 配合。
 */
class NEW_INTERN_VAR {
public:
    OPCODE_META(NEW_INTERN_VAR)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit NEW_INTERN_VAR(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * NEW_INTERN_CONST — 在函数/块内创建内部常量绑定.
 *
 * 作用：同 NEW_INTERN_VAR，但 constants 标记为 true。
 * 何时使用：块内 const、循环中不可改的迭代名（若语言区分）。
 * 意义：作用域内常量语义与模块级 NEW_CONST 一致，层级更浅。
 */
class NEW_INTERN_CONST {
public:
    OPCODE_META(NEW_INTERN_CONST)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit NEW_INTERN_CONST(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * NEW_VAR_OR_LOAD — 若已存在则加载引用，否则创建新变量.
 *
 * 作用：在 cache/symbol 中查找名字；找到则压入已有 ref，未找到则创建并注册。
 * 何时使用：赋值语句左侧（可能首次赋值）、需要「读-改-写」同一名字的场景。
 * 意义：统一首次定义与后续赋值的路径，简化 AssignNode 的 codegen。
 */
class NEW_VAR_OR_LOAD {
public:
    OPCODE_META(NEW_VAR_OR_LOAD)
    size_t var_id;
    OPCODE_ARGS1(var_id)

    explicit NEW_VAR_OR_LOAD(const std::string& name);

    void emit(VM& vm) const;
};

/**
 * RET_THEN_LEAVE_SCOPE — 返回并弹出当前作用域.
 *
 * 作用：依次执行 RET 与 LEAVE_SCOPE（函数 return 的标准尾声）。
 * 何时使用：带返回值的 func 声明体结束、需要同时归还调用方并销毁函数帧。
 * 意义：避免忘记 LEAVE_SCOPE 导致 symbol/locals 泄漏；do 返回用裸 RET 即可。
 */
class RET_THEN_LEAVE_SCOPE {
public:
    OPCODE_META(RET_THEN_LEAVE_SCOPE)
    OPCODE_ARGS0()

    RET_THEN_LEAVE_SCOPE() = default;

    void emit(VM& vm) const;
};

/**
 * LOAD_FAST — 按槽位读取当前帧局部变量.
 *
 * 作用：从 locals_stack 顶层按 slot 取值，以 Reference 形式压栈（非 ref 则包装）。
 * 何时使用：函数参数、块内 let、当前帧内的 VarRef（非闭包捕获的外层名）。
 * 意义：O(1) 局部访问；闭包捕获的外层变量应走 LOAD(name)+DEREF 而非本指令。
 */
class LOAD_FAST {
public:
    OPCODE_META(LOAD_FAST)
    size_t slot;
    OPCODE_ARGS1(slot)

    explicit LOAD_FAST(size_t slot) : slot(slot) {
    }

    void emit(VM& vm) const;
};

/**
 * STORE_FAST — 按槽位写入当前帧局部变量.
 *
 * 作用：弹出栈顶值写入 locals_stack 顶层的 slot。
 * 何时使用：参数接收（CALL 后）、let 初始化、for 循环迭代变量更新。
 * 意义：函数帧的主力存储；参数绑定后常跟 BIND_FAST 以支持闭包与 LOAD 按名查找。
 */
class STORE_FAST {
public:
    OPCODE_META(STORE_FAST)
    size_t slot;
    OPCODE_ARGS1(slot)

    explicit STORE_FAST(size_t slot) : slot(slot) {
    }

    void emit(VM& vm) const;
};

/**
 * STRUCT_NEW — 按已注册的 struct 类型构造实例.
 *
 * 作用：弹出 n 个 positional 实参，与 struct 字段顺序对齐；缺省字段用默认值。
 * 何时使用：A(1, 2) 且 A 为 struct 名（由 codegen 根据全局 struct 注册表生成）。
 */
class STRUCT_NEW {
public:
    OPCODE_META(STRUCT_NEW)
    size_t struct_id;
    size_t arg_count;
    OPCODE_ARGS2(struct_id, arg_count)

    STRUCT_NEW(const std::string& struct_name, size_t arg_count);

    void emit(VM& vm) const;
};

/**
 * SET_FIELD — 写入 struct 的可变字段（var）。
 *
 * 作用：栈顶为值、次栈顶为 struct；typed struct 时做类型检查。
 * 何时使用：a.b = expr 且 a 为 struct 实例。
 */
class SET_FIELD {
public:
    OPCODE_META(SET_FIELD)
    size_t name_id;
    OPCODE_ARGS1(name_id)

    explicit SET_FIELD(const std::string& field_name);

    void emit(VM& vm) const;
};

/**
 * IS_VECTOR — 判断栈顶值是否为向量.
 */
class IS_VECTOR {
public:
    OPCODE_META(IS_VECTOR)
    OPCODE_ARGS0()

    IS_VECTOR() = default;

    void emit(VM& vm) const;
};

/**
 * VEC_LEN — 取向量长度（栈顶须为向量）.
 */
class VEC_LEN {
public:
    OPCODE_META(VEC_LEN)
    OPCODE_ARGS0()

    VEC_LEN() = default;

    void emit(VM& vm) const;
};

/**
 * MATCH_EQ — 模式匹配用的相等比较，类型不兼容时结果为 false.
 */
class MATCH_EQ {
public:
    OPCODE_META(MATCH_EQ)
    OPCODE_ARGS0()

    MATCH_EQ() = default;

    void emit(VM& vm) const;
};

/**
 * POP — 弹出并丢弃栈顶值.
 */
class POP {
public:
    OPCODE_META(POP)
    OPCODE_ARGS0()

    POP() = default;

    void emit(VM& vm) const;
};

/**
 * BIND_FAST — 将 fast 槽与符号表中的名字别名绑定.
 *
 * 作用：把 locals_stack 某槽与 symbol_stack 顶层的 var_id 指向同一 ref 单元
 *       （非 ref 槽会先包装为 ref 再注册）。
 * 何时使用：函数参数 STORE_FAST 之后、需要被闭包捕获或 LOAD(name) 的外层局部。
 * 意义：弥合「帧槽」与「名字+闭包」两套模型；无此指令则闭包只能捕获模块级符号。
 */
class BIND_FAST {
public:
    OPCODE_META(BIND_FAST)
    size_t slot;
    size_t var_id;
    OPCODE_ARGS2(slot, var_id)

    BIND_FAST(size_t slot, const std::string& name);

    void emit(VM& vm) const;
};

inline void set_opcode_line(Opcode& op, const std::string& line) {
    if (line.empty()) {
        return;
    }
    std::visit([&](auto& o) { o.line = line; }, op);
}

inline std::string get_opcode_line(const Opcode& op) {
    return std::visit([](const auto& o) { return o.line; }, op);
}

inline int get_opcode_line_no(const Opcode& op) {
    return std::visit([](const auto& o) { return o.line_no; }, op);
}

/**
 * @brief 字符串池，用于字符串的唯一化存储
 */
class StringPool {
    std::unordered_map<std::string, size_t> string_to_id; ///< 字符串到ID的映射
    std::vector<std::string> id_to_string;                ///< ID到字符串的映射
    size_t counter = 0;                                   ///< 下一个可用ID

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
    size_t add(const std::string& name) {
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
    bool exists(const std::string& name) const {
        return string_to_id.contains(name);
    }

    /**
     * @brief 获取字符串的ID
     * @param name 字符串
     * @return ID
     */
    size_t get_id(const std::string& name) const {
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
    const std::string& get_string(size_t id) const {
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

    /** @brief 导出池中全部字符串（按下标顺序） */
    [[nodiscard]] const std::vector<std::string>& export_strings() const {
        return id_to_string;
    }

    /** @brief 用给定字符串表重建池（下标即 ID） */
    void rebuild(const std::vector<std::string>& strings) {
        string_to_id.clear();
        id_to_string = strings;
        counter = strings.size();
        for (size_t i = 0; i < strings.size(); ++i) {
            string_to_id[strings[i]] = i;
        }
    }

    /** @brief 复制当前池状态（用于 RAII 恢复） */
    [[nodiscard]] StringPool capture() const {
        StringPool copy;
        copy.string_to_id = string_to_id;
        copy.id_to_string = id_to_string;
        copy.counter = counter;
        return copy;
    }

    /** @brief 从 capture() 的快照恢复 */
    void restore(const StringPool& state) {
        string_to_id = state.string_to_id;
        id_to_string = state.id_to_string;
        counter = state.counter;
    }
};

inline StringPool g_string_pool{}; ///< 全局字符串池

/** @brief 临时切换 g_string_pool，析构时恢复 */
class StringPoolGuard {
    StringPool saved_;

public:
    /** @brief 清空全局池，析构时恢复（隔离单次编译） */
    StringPoolGuard() : saved_(g_string_pool.capture()) { g_string_pool.clear(); }

    /** @brief 安装 module 池，析构时恢复 */
    explicit StringPoolGuard(const std::vector<std::string>& module_pool)
        : saved_(g_string_pool.capture()) {
        g_string_pool.rebuild(module_pool);
    }

    ~StringPoolGuard() { g_string_pool.restore(saved_); }

    StringPoolGuard(const StringPoolGuard&) = delete;
    StringPoolGuard& operator=(const StringPoolGuard&) = delete;
};

/** @brief try/catch 栈帧：异常时沿 call_func_stack 回退到此深度 */
struct TryHandlerFrame {
    size_t catch_label = 0;
    size_t else_label = 0;
    size_t end_label = 0;
    size_t call_stack_sz = 0;
    size_t call_func_stack_sz = 0;
    size_t symbol_stack_sz = 0;
    size_t locals_stack_sz = 0;
};

/**
 * @brief 虚拟机类，执行IR指令
 */
class VM {
public:
    CellPool cell_pool;                                             ///< 槽位对象池
    size_t gc_suppress_depth = 0;                                   ///< >0 时跳过扩容/周期 GC
    size_t iter_next_guard_depth = 0;                               ///< >0 时 for-in 的 ITER_NEXT 吞掉 StopIteration
    Stack<Value> op_stack{};                                        ///< 操作数栈
    std::vector<size_t> call_stack{};                               ///< 返回地址栈
    std::string source_filename = "<unknown>";                      ///< 当前执行的源文件名
    std::vector<std::shared_ptr<FunctionObject>> call_func_stack{}; ///< 函数调用栈
    std::vector<TryHandlerFrame> try_stack{};                       ///< try/catch 处理器栈
    std::optional<size_t> deferred_try_end_label{};                 ///< struct 方法嵌套 run 已处理异常时，外层 END_TRY 跳转目标
    size_t nested_function_run_depth = 0;                           ///< FunctionObject::call 嵌套 run 深度
    std::shared_ptr<Value> active_exception{};                      ///< 当前待匹配/绑定的异常对象
    std::vector<Opcode> code{};                                     ///< IR指令序列
    size_t label_scan_end = 0;                                      ///< 已扫描标签的 code 上界
    std::vector<SymbolTable> symbol_stack{SymbolTable()};           ///< 符号表栈
    std::vector<std::vector<Value>> locals_stack;                   ///< 局部变量栈
    Cache cache{};                                                  ///< 变量缓存
    std::unordered_map<size_t, size_t> label_table{};               ///< 标签位置表
    size_t pc = 0;                                                  ///< 程序计数器
    size_t label_counter = 0;                                       ///< 标签计数器
    std::shared_ptr<ModuleObject> main_module;                      ///< 主模块（全局命名空间与 import 根）
    std::vector<std::vector<SymbolTable>> macro_eval_scope_stack;   ///< eval(ast) 使用的调用方环境

    /**
     * @brief 初始化内置函数
     */
    void init_builtins();

    VM();

    ~VM();

    VM(VM&& other) noexcept;

    VM& operator=(VM&& other) noexcept;

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
            std::visit(
                [&]<typename VT>(VT& op) -> void {
                    if constexpr (std::is_same_v<std::decay_t<VT>, LABEL>) {
                        label_table[op.label_id] = i;
                    }
                },
                code[i]
            );
        }
    }

    /**
     * @brief 执行IR指令序列
     */
    void run();

    /** @brief 在 VM 析构前按安全顺序释放运行时状态（供 execute 等短生命周期 VM 使用） */
    void shutdown();

    /** @brief 触发标记-清除 GC，回收不可达槽位 */
    void collectGarbage();

    /**
     * @brief 获取符号值
     * @param name 符号名称
     * @return 值（如果存在）
     */
    std::optional<Value> get_symbol(const std::string& name) const;

    /**
     * @brief 设置符号值
     * @param name 符号名称
     * @param value 值
     */
    void set_symbol(const std::string& name, const Value& value);

private:
    /** @brief 移动后把 cell_pool / owner_vm / 迭代器等指针绑到新地址 */
    void rebindAfterMove(VM& source) noexcept;
};

/** @brief 批量分配期间抑制扩容 GC，避免未挂根槽位被 sweep */
struct VmGcSuppress {
    VM& vm;

    explicit VmGcSuppress(VM& vm_) : vm(vm_) {
        ++vm.gc_suppress_depth;
    }

    VmGcSuppress(const VmGcSuppress&) = delete;

    VmGcSuppress& operator=(const VmGcSuppress&) = delete;

    ~VmGcSuppress() {
        --vm.gc_suppress_depth;
    }
};

/**
 * @brief 模块对象，存储模块的导出和子模块
 */
class ModuleObject : public std::enable_shared_from_this<ModuleObject> {
    bool is_user; ///< 是否为用户模块
    std::unique_ptr<VM> owned_vm_; ///< import 时独占的 VM（析构自动释放）

public:
    std::string name;                                                          ///< 模块名称
    std::string full_name;                                                     ///< 完整模块路径名
    std::unordered_map<std::string, Value> exports;                            ///< 导出的符号
    std::unordered_map<std::string, std::shared_ptr<ModuleObject>> submodules; ///< 子模块
    VM* owner_vm = nullptr;                                                    ///< 所属虚拟机

    /**
     * @brief 从代码字符串构造模块
     * @tparam string 字符串类型
     * @param code 代码字符串
     * @param package_name 模块名
     */
    template<StringType string>
    explicit ModuleObject(string code, string package_name = "__main__");

    /**
     * @brief 从符号表构造模块（内置模块）
     * @param symbols 符号表
     */
    explicit(false) ModuleObject(const SymbolTable& symbols) : is_user(false) {
        for (const auto& [id, val] : symbols.symbols) {
            exports[g_string_pool.get_string(id)] = *val;
        }
        LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
        for (const auto& [atname, val] : exports) {
            LOG(atname << " : " << val);
        }
    }

    /**
     * @brief 从符号表栈构造模块（用户模块）
     * @param symbol_stack 符号表栈
     */
    explicit ModuleObject(const std::vector<SymbolTable>& symbol_stack) : is_user(true) {
        for (const auto& table : symbol_stack) {
            for (const auto& [id, val] : table.symbols) {
                exports[g_string_pool.get_string(id)] = *val;
            }
        }
        LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
        for (const auto& [atname, val] : exports) {
            LOG(atname << " : " << val);
        }
    }

    /**
     * @brief 构造空模块
     * @param n 模块名称
     * @param vm 所属虚拟机
     */
    explicit ModuleObject(std::string n, VM* vm)
        : is_user(true), name(std::move(n)), full_name(name), owner_vm(vm) {
        LOG("Done exports of " << name << " : " << full_name << "(" << is_user << ")");
        for (const auto& [atname, val] : exports) {
            LOG(atname << " : " << val);
        }
    }

    ModuleObject(const ModuleObject& other)
        : is_user(other.is_user),
          name(other.name),
          full_name(other.full_name),
          exports(other.exports),
          submodules(other.submodules),
          owner_vm(other.owner_vm) {}

    ModuleObject(ModuleObject&& other) noexcept
        : is_user(other.is_user),
          name(std::move(other.name)),
          full_name(std::move(other.full_name)),
          exports(std::move(other.exports)),
          submodules(std::move(other.submodules)),
          owner_vm(other.owner_vm),
          owned_vm_(std::move(other.owned_vm_)) {
        other.owner_vm = nullptr;
    }

    ModuleObject& operator=(const ModuleObject& other) {
        if (this == &other) {
            return *this;
        }
        is_user = other.is_user;
        name = other.name;
        full_name = other.full_name;
        exports = other.exports;
        submodules = other.submodules;
        owner_vm = other.owner_vm;
        owned_vm_.reset();
        return *this;
    }

    ModuleObject& operator=(ModuleObject&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        is_user = other.is_user;
        name = std::move(other.name);
        full_name = std::move(other.full_name);
        exports = std::move(other.exports);
        submodules = std::move(other.submodules);
        owner_vm = other.owner_vm;
        owned_vm_ = std::move(other.owned_vm_);
        other.owner_vm = nullptr;
        return *this;
    }

    /**
     * @brief 获取模块属性
     * @param attrname 属性名称
     * @return 属性值（如果存在）
     */
    std::optional<Value> get_attr(const std::string& attrname) const {
        LOG("Finding " + attrname);

        LOG("exports: ");
        for (const auto& [atname, val] : exports) {
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
    void set_attr(const std::string& attrname, const Value& value) {
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
    Value import(const std::string& module_name);
};
}
#undef OPCODE_ARGS0
#undef OPCODE_ARGS1
#undef OPCODE_ARGS2
#undef OPCODE_ARGS1V
#undef OPCODE_META
