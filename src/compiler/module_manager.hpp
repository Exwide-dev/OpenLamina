#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <optional>
#include <functional>

namespace lm::compiler {

class ModuleObject;

/**
 * @struct ModulePath
 * @brief 模块路径结构
 */
struct ModulePath {
    std::vector<std::string> components; ///< 路径组件列表

    /**
     * @brief 默认构造函数
     */
    ModulePath() = default;
    
    /**
     * @brief 从组件列表构造
     * @param comps 路径组件列表
     */
    explicit ModulePath(std::vector<std::string> comps) : components(std::move(comps)) {}
    
    /**
     * @brief 从点分隔的名称构造
     * @param dotted_name 点分隔的模块名
     */
    explicit ModulePath(const std::string& dotted_name);

    /**
     * @brief 转换为字符串
     * @return 点分隔的模块名
     */
    [[nodiscard]] std::string toString() const;
    
    /**
     * @brief 检查是否为空
     * @return 如果为空返回true
     */
    [[nodiscard]] bool empty() const { return components.empty(); }
    
    /**
     * @brief 获取路径长度
     * @return 组件数量
     */
    [[nodiscard]] size_t size() const { return components.size(); }
    
    /**
     * @brief 获取父路径
     * @return 父模块路径
     */
    [[nodiscard]] ModulePath parent() const;
    
    /**
     * @brief 获取指定索引的组件
     * @param idx 索引
     * @return 组件引用
     */
    std::string& operator[](size_t idx) { return components[idx]; }
    
    /**
     * @brief 获取指定索引的组件（const版本）
     * @param idx 索引
     * @return 组件const引用
     */
    const std::string& operator[](size_t idx) const { return components[idx]; }
    
    /**
     * @brief 相等比较
     * @param other 另一个路径
     * @return 如果相等返回true
     */
    bool operator==(const ModulePath& other) const = default;
};

/**
 * @struct ModuleSearchPath
 * @brief 模块搜索路径结构
 */
struct ModuleSearchPath {
    std::filesystem::path path;  ///< 文件系统路径
    bool recursive;              ///< 是否递归搜索
    
    /**
     * @brief 构造函数
     * @param p 文件系统路径
     * @param rec 是否递归搜索，默认为true
     */
    ModuleSearchPath(std::filesystem::path p, bool rec = true) 
        : path(std::move(p)), recursive(rec) {}
};

/**
 * @class ModuleManager
 * @brief 模块管理器类
 */
class ModuleManager {
public:
    using ModulePtr = std::shared_ptr<ModuleObject>;       ///< 模块指针类型
    using LoaderFunc = std::function<ModulePtr(const ModulePath&)>; ///< 加载器函数类型

private:
    std::vector<ModuleSearchPath> search_paths_;   ///< 搜索路径列表
    std::unordered_map<std::string, ModulePtr> module_cache_; ///< 模块缓存
    std::vector<LoaderFunc> custom_loaders_;       ///< 自定义加载器列表
    ModulePtr root_module_;                        ///< 根模块

    /**
     * @brief 在文件系统中查找模块文件
     * @param path 模块路径
     * @return 文件路径（可选）
     */
    std::optional<std::filesystem::path> find_module_file(const ModulePath& path) const;
    
    /**
     * @brief 从文件系统加载模块
     * @param path 模块路径
     * @return 模块指针
     */
    ModulePtr load_from_filesystem(const ModulePath& path);

public:
    /**
     * @brief 默认构造函数
     */
    ModuleManager();

    /**
     * @brief 添加搜索路径
     * @param path 文件系统路径
     * @param recursive 是否递归搜索，默认为true
     */
    void add_search_path(const std::filesystem::path& path, bool recursive = true);
    
    /**
     * @brief 添加自定义加载器
     * @param loader 加载器函数
     */
    void add_custom_loader(LoaderFunc loader);
    
    /**
     * @brief 导入模块
     * @param path 模块路径
     * @param alias 别名，默认为空
     * @return 模块指针
     */
    ModulePtr import_module(const ModulePath& path, const std::string& alias = "");
    
    /**
     * @brief 获取模块
     * @param path 模块路径
     * @return 模块指针（如果存在）
     */
    ModulePtr get_module(const ModulePath& path) const;
    
    /**
     * @brief 检查模块是否存在
     * @param path 模块路径
     * @return 如果存在返回true
     */
    bool has_module(const ModulePath& path) const;
    
    /**
     * @brief 缓存模块
     * @param path 模块路径
     * @param module 模块指针
     */
    void cache_module(const ModulePath& path, ModulePtr module);
    
    /**
     * @brief 清除缓存
     */
    void clear_cache();
    
    /**
     * @brief 设置根模块
     * @param module 模块指针
     */
    void set_root_module(ModulePtr module);
    
    /**
     * @brief 获取根模块
     * @return 根模块指针
     */
    ModulePtr get_root_module() const { return root_module_; }

    /**
     * @brief 规范化模块名称
     * @param name 模块名称
     * @return 规范化后的名称
     */
    static std::string normalize_module_name(const std::string& name);
};

} // namespace lm::compiler

namespace std {
    /**
     * @brief ModulePath 的哈希函数特化
     */
    template<> struct hash<lm::compiler::ModulePath> {
        size_t operator()(const lm::compiler::ModulePath& path) const {
            return std::hash<std::string>()(path.toString());
        }
    };
}