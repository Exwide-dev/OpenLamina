#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <set>

#include "../irgen/opcode.hpp"

namespace lm::compiler {

struct ModuleObject;
using ModulePtr = std::shared_ptr<ModuleObject>;

struct ModuleObject : public std::enable_shared_from_this<ModuleObject> {
    std::string name;
    std::string full_name;
    ModulePtr parent;
    std::unordered_map<std::string, ModulePtr> children;
    std::unordered_map<std::string, irgen::Value> variables;
    std::unordered_map<std::string, std::shared_ptr<irgen::FunctionObject>> functions;
    std::vector<irgen::Opcode> init_code;
    std::set<std::string> exported_symbols;
    bool is_loaded = false;
    bool is_std_module = false;

    ModuleObject() = default;
    explicit ModuleObject(std::string n) : name(std::move(n)) {}

    void add_child(const std::string& name, ModulePtr module);
    ModulePtr get_child(const std::string& name) const;
    bool has_child(const std::string& name) const;

    void export_symbol(const std::string& symbol);
    void export_all();
    bool is_exported(const std::string& symbol) const;

    ModulePtr resolve_path(const std::vector<std::string>& path, size_t index = 0) const;
};

class ModuleCompiler {
public:
    std::shared_ptr<ModuleObject> load_module(const std::string& module_name, const std::string& file_path);
    std::shared_ptr<ModuleObject> load_module_from_string(const std::string& module_name, const std::string& source);
    
    void set_module_manager(class ModuleManager* manager);
private:
    ModuleManager* manager_ = nullptr;
};

} // namespace lm::compiler