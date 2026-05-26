#pragma once
#include "opcode.hpp"
#include "parser/ast.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

namespace lm::irgen {
    template<typename T>
    class Stack {
        std::vector<T> data;
    public:
        void push(const T& value) { data.push_back(value); }
        void push(T&& value) { data.push_back(std::move(value)); }
        void pop() { data.pop_back(); }
        T& top() { return data.back(); }
        const T& top() const { return data.back(); }
        bool empty() const { return data.empty(); }
        size_t size() const { return data.size(); }
        const std::vector<T>& get_container() const { return data; }
        T& emplace() {
            data.emplace_back();
            return data.back();
        }
    };

    struct VarLocation {
        size_t define_depth = 0;
        size_t slot = 0;
        VarLocation() = default;
        VarLocation(size_t depth, size_t s) : define_depth(depth), slot(s) {}
    };

    struct LocalScope {
        std::unordered_map<std::string, VarLocation> var_to_location;
        std::unordered_map<std::string, bool> is_const;
        std::unordered_map<std::string, lmx::Visibility> visibility;
        size_t next_slot = 0;
        size_t depth = 0;

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

        std::optional<VarLocation> get_location(const std::string& name) const {
            auto it = var_to_location.find(name);
            if (it != var_to_location.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        std::optional<size_t> get_slot(const std::string& name) const {
            auto it = var_to_location.find(name);
            if (it != var_to_location.end()) {
                return it->second.slot;
            }
            return std::nullopt;
        }

        size_t get_local_count() const {
            return next_slot;
        }
    };

    class Generator {
    public:
        lmx::ProgramASTNode* ast;
        explicit Generator(lmx::ProgramASTNode* ast) : ast(ast) {};
        ~Generator() = default;

        [[nodiscard]] std::vector<::irgen::Opcode> gen() const;

        static void replace_string(std::vector<::irgen::Opcode>& codes) {
            for (auto& code : codes) {
                std::visit([&](auto& op) {
                    const std::string op_name = op.name();
                    if (op_name == "PUSH") return;
                    for (auto& operand : op.operands) {
                        if (operand.isString()) {
                            const std::string name = operand.asString();
                            size_t id = ::irgen::g_string_pool.add(name);
                            operand = ::irgen::Value(id);
                        }
                    }
                }, code);
            }
        }
    };

    // 包装函数：接收 ProgramASTNode，生成字节码并执行
    ::irgen::Value execute(const lmx::ProgramASTNode* program);
}
