#include "module_manager.hpp"
#include "module_compiler.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lm::compiler {

ModulePath::ModulePath(const std::string& dotted_name) {
    std::istringstream iss(dotted_name);
    std::string part;
    while (std::getline(iss, part, '.')) {
        if (!part.empty()) {
            components.push_back(part);
        }
    }
}

std::string ModulePath::toString() const {
    std::string result;
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) result += ".";
        result += components[i];
    }
    return result;
}

ModulePath ModulePath::parent() const {
    if (components.empty()) {
        return {};
    }
    std::vector<std::string> parent_comps(components.begin(), components.end() - 1);
    return ModulePath(std::move(parent_comps));
}

ModuleManager::ModuleManager() {
    search_paths_.emplace_back(std::filesystem::current_path(), true);
    search_paths_.emplace_back(std::filesystem::path("."), true);
}

void ModuleManager::add_search_path(const std::filesystem::path& path, bool recursive) {
    search_paths_.emplace_back(path, recursive);
}

void ModuleManager::add_custom_loader(LoaderFunc loader) {
    custom_loaders_.push_back(std::move(loader));
}

std::optional<std::filesystem::path> ModuleManager::find_module_file(const ModulePath& path) const {
    std::filesystem::path module_path;
    for (const auto& part : path.components) {
        module_path /= part;
    }

    for (const auto& search : search_paths_) {
        std::filesystem::path candidate = search.path / module_path;
        
        std::filesystem::path lm_path = candidate;
            lm_path.replace_extension(".lm");
            if (std::filesystem::exists(lm_path)) {
                return lm_path;
            }
        
        if (std::filesystem::exists(candidate / "main.lm")) {
            return candidate / "main.lm";
        }
        
        if (search.recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(search.path)) {
                if (entry.is_regular_file() && entry.path().stem() == module_path.filename()) {
                    if (entry.path().extension() == ".lm") {
                        return entry.path();
                    }
                }
            }
        }
    }
    
    return std::nullopt;
}

ModulePtr ModuleManager::load_from_filesystem(const ModulePath& path) {
    auto file_path = find_module_file(path);
    if (!file_path) {
        throw std::runtime_error("Module not found: " + path.toString());
    }
    
    ModuleCompiler compiler;
    return compiler.load_module(path.toString(), file_path.value().string());
}

ModulePtr ModuleManager::import_module(const ModulePath& path, const std::string& alias) {
    std::string module_key = alias.empty() ? path.toString() : alias;
    
    if (module_cache_.contains(module_key)) {
        return module_cache_[module_key];
    }
    
    ModulePtr module;
    
    for (const auto& loader : custom_loaders_) {
        try {
            module = loader(path);
            if (module) {
                break;
            }
        } catch (...) {
            continue;
        }
    }
    
    if (!module) {
        module = load_from_filesystem(path);
    }
    
    module_cache_[module_key] = module;
    
    return module;
}

ModulePtr ModuleManager::get_module(const ModulePath& path) const {
    auto it = module_cache_.find(path.toString());
    if (it != module_cache_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ModuleManager::has_module(const ModulePath& path) const {
    return module_cache_.contains(path.toString());
}

void ModuleManager::cache_module(const ModulePath& path, ModulePtr module) {
    module_cache_[path.toString()] = module;
}

void ModuleManager::clear_cache() {
    module_cache_.clear();
}

void ModuleManager::set_root_module(ModulePtr module) {
    root_module_ = std::move(module);
}

std::string ModuleManager::normalize_module_name(const std::string& name) {
    std::string result;
    bool first = true;
    for (char c : name) {
        if (c == '.' || c == '/' || c == '\\') {
            if (!first) {
                result += '.';
            }
            first = true;
        } else {
            result += c;
            first = false;
        }
    }
    return result;
}

} // namespace lm::compiler