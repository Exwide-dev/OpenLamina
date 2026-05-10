#include "module_compiler.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include <fstream>
#include <filesystem>

namespace lm::compiler {

void ModuleObject::add_child(const std::string& name, ModulePtr module) {
    children[name] = module;
    module->parent = shared_from_this();
    module->full_name = full_name.empty() ? name : full_name + "." + name;
}

ModulePtr ModuleObject::get_child(const std::string& name) const {
    auto it = children.find(name);
    return it != children.end() ? it->second : nullptr;
}

bool ModuleObject::has_child(const std::string& name) const {
    return children.contains(name);
}

void ModuleObject::export_symbol(const std::string& symbol) {
    exported_symbols.insert(symbol);
}

void ModuleObject::export_all() {
    for (const auto& [name, _] : variables) {
        exported_symbols.insert(name);
    }
    for (const auto& [name, _] : functions) {
        exported_symbols.insert(name);
    }
    for (const auto& [name, _] : children) {
        exported_symbols.insert(name);
    }
}

bool ModuleObject::is_exported(const std::string& symbol) const {
    if (exported_symbols.empty()) {
        return true;
    }
    return exported_symbols.contains(symbol);
}

ModulePtr ModuleObject::resolve_path(const std::vector<std::string>& path, size_t index) const {
    if (index >= path.size()) {
        return const_cast<ModuleObject*>(this)->shared_from_this();
    }
    
    const std::string& current = path[index];
    auto child = get_child(current);
    if (!child) {
        return nullptr;
    }
    
    return child->resolve_path(path, index + 1);
}

std::shared_ptr<ModuleObject> ModuleCompiler::load_module(const std::string& module_name, const std::string& file_path) {
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("Module file not found: " + file_path);
    }
    
    std::ifstream file(file_path);
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    return load_module_from_string(module_name, source);
}

std::shared_ptr<ModuleObject> ModuleCompiler::load_module_from_string(const std::string& module_name, const std::string& source) {
    auto module = std::make_shared<ModuleObject>();
    module->name = module_name;
    module->full_name = module_name;
    
    auto program = parse(source);
    
    if (!program) {
        throw std::runtime_error("Failed to parse module: " + module_name);
    }
    
    auto generator = std::make_unique<lm::irgen::Generator>(program);
    module->init_code = generator->gen();
    
    module->is_loaded = true;
    module->export_all();
    
    return module;
}

void ModuleCompiler::set_module_manager(ModuleManager* manager) {
    manager_ = manager;
}

} // namespace lm::compiler