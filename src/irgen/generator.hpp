#pragma once
#include "opcode.hpp"
#include "parser/ast.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

namespace lm::irgen {
/**
 * @brief 栈模板类
 * @tparam T 栈元素类型
 */
template<typename T>
class Stack {
    std::vector<T> data; ///< 存储栈元素的向量
public:
    /**
     * @brief 压入元素
     * @param value 要压入的元素
     */
    void push(const T& value) { data.push_back(value); }

    /**
     * @brief 移动压入元素
     * @param value 要压入的元素（右值引用）
     */
    void push(T&& value) { data.push_back(std::move(value)); }

    /**
     * @brief 弹出元素
     */
    void pop() { data.pop_back(); }

    /**
     * @brief 获取栈顶元素
     * @return 栈顶元素的引用
     */
    T& top() { return data.back(); }

    /**
     * @brief 获取栈顶元素（const版本）
     * @return 栈顶元素的const引用
     */
    const T& top() const { return data.back(); }

    /**
     * @brief 检查栈是否为空
     * @return 如果为空返回true
     */
    [[nodiscard]] bool empty() const { return data.empty(); }

    /**
     * @brief 获取栈大小
     * @return 栈中元素数量
     */
    [[nodiscard]] size_t size() const { return data.size(); }

    /**
     * @brief 获取底层容器
     * @return 存储元素的向量
     */
    const std::vector<T>& get_container() const { return data; }

    /**
     * @brief 在栈顶构造元素
     * @return 新构造元素的引用
     */
    T& emplace() {
        data.emplace_back();
        return data.back();
    }
};

/**
 * @struct VarLocation
 * @brief 变量位置信息
 */
struct VarLocation {
    size_t define_depth = 0; ///< 变量定义时的作用域深度
    size_t slot = 0;         ///< 变量在栈帧中的槽位

    /**
     * @brief 默认构造函数
     */
    VarLocation() = default;

    /**
     * @brief 构造函数
     * @param depth 作用域深度
     * @param s 槽位索引
     */
    VarLocation(size_t depth, size_t s) : define_depth(depth), slot(s) {
    }
};

/**
 * @struct LocalScope
 * @brief 局部作用域信息
 */
struct LocalScope {
    std::unordered_map<std::string, VarLocation> var_to_location; ///< 变量名到位置的映射
    std::unordered_map<std::string, bool> is_const;               ///< 变量是否为常量
    std::unordered_map<std::string, lmx::Visibility> visibility;  ///< 变量可见性
    size_t next_slot = 0;                                         ///< 下一个可用槽位
    size_t depth = 0;                                             ///< 作用域深度

    /**
     * @brief 分配变量槽位
     * @param name 变量名
     * @param constant 是否为常量
     * @param vis 可见性
     * @return 分配的槽位索引
     */
    size_t allocate_slot(const std::string& name, bool constant, lmx::Visibility vis) {
        auto it = var_to_location.find(name);
        if (it != var_to_location.end()) {
            return it->second.slot;
        }
        size_t slot = next_slot++;
        var_to_location[name] = VarLocation(depth, slot);
        is_const[name] = constant;
        visibility[name] = vis;
        return slot;
    }

    /**
     * @brief 获取变量位置
     * @param name 变量名
     * @return 变量位置（可选）
     */
    std::optional<VarLocation> get_location(const std::string& name) const {
        auto it = var_to_location.find(name);
        if (it != var_to_location.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 获取变量槽位
     * @param name 变量名
     * @return 槽位索引（可选）
     */
    std::optional<size_t> get_slot(const std::string& name) const {
        auto it = var_to_location.find(name);
        if (it != var_to_location.end()) {
            return it->second.slot;
        }
        return std::nullopt;
    }

    /**
     * @brief 获取局部变量数量
     * @return 已分配的槽位数量
     */
    size_t get_local_count() const {
        return next_slot;
    }
};

/**
 * @class Generator
 * @brief IR生成器类，将AST转换为字节码
 */
class Generator {
public:
    lmx::ProgramASTNode* ast; ///< 程序AST根节点

    /**
     * @brief 构造函数
     * @param ast 程序AST根节点
     */
    explicit Generator(lmx::ProgramASTNode* ast) : ast(ast) {
    };

    ~Generator() = default;

    /**
     * @brief 生成字节码
     * @return 字节码向量
     */
    [[nodiscard]] std::vector<::irgen::Opcode> gen() const;

    /**
     * @brief 将字符串操作数替换为字符串池ID
     * @param codes 字节码向量
     */
    static void replace_string(std::vector<::irgen::Opcode>& codes) {}
};

/**
 * @brief 执行程序的包装函数
 * @param program 程序AST节点
 * @return 执行结果
 */
::irgen::Value execute(const lmx::ProgramASTNode* program);

/**
 *
 * @param code 字节码序列
 */
void print_code(const std::vector<::irgen::Opcode>& code);
}
