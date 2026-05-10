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

struct ModulePath {
    std::vector<std::string> components;

    ModulePath() = default;
    explicit ModulePath(std::vector<std::string> comps) : components(std::move(comps)) {}
    explicit ModulePath(const std::string& dotted_name);

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool empty() const { return components.empty(); }
    [[nodiscard]] size_t size() const { return components.size(); }
    
    [[nodiscard]] ModulePath parent() const;
    std::string& operator[](size_t idx) { return components[idx]; }
    const std::string& operator[](size_t idx) const { return components[idx]; }
    
    bool operator==(const ModulePath& other) const = default;
};

struct ModuleSearchPath {
    std::filesystem::path path;
    bool recursive;
    
    ModuleSearchPath(std::filesystem::path p, bool rec = true) 
        : path(std::move(p)), recursive(rec) {}
};

class ModuleManager {
public:
    using ModulePtr = std::shared_ptr<ModuleObject>;
    using LoaderFunc = std::function<ModulePtr(const ModulePath&)>;

private:
    std::vector<ModuleSearchPath> search_paths_;
    std::unordered_map<std::string, ModulePtr> module_cache_;
    std::vector<LoaderFunc> custom_loaders_;
    ModulePtr root_module_;

    std::optional<std::filesystem::path> find_module_file(const ModulePath& path) const;
    ModulePtr load_from_filesystem(const ModulePath& path);

public:
    ModuleManager();

    void add_search_path(const std::filesystem::path& path, bool recursive = true);
    void add_custom_loader(LoaderFunc loader);
    
    ModulePtr import_module(const ModulePath& path, const std::string& alias = "");
    ModulePtr get_module(const ModulePath& path) const;
    bool has_module(const ModulePath& path) const;
    
    void cache_module(const ModulePath& path, ModulePtr module);
    void clear_cache();
    
    void set_root_module(ModulePtr module);
    ModulePtr get_root_module() const { return root_module_; }

    static std::string normalize_module_name(const std::string& name);
};

} // namespace lm::compiler

namespace std {
    template<> struct hash<lm::compiler::ModulePath> {
        size_t operator()(const lm::compiler::ModulePath& path) const {
            return std::hash<std::string>()(path.toString());
        }
    };
}