#pragma once

#include "../parser/ast.hpp"

#include <memory>
#include <string>
#include <vector>

namespace irgen {

class Value;
class VM;

/** @brief 语言内 AST 节点（值语义；递归子槽用 unique_ptr 实现） */
struct RuntimeAstNode {
    lmx::ASTNodeType kind = lmx::ASTNodeType::Unknown;
    int line = 0;

    std::string text;
    bool bool_val = false;

    std::vector<RuntimeAstNode> stmts;
    std::vector<RuntimeAstNode> children;

    std::unique_ptr<RuntimeAstNode> slot_a;
    std::unique_ptr<RuntimeAstNode> slot_b;
    std::unique_ptr<RuntimeAstNode> slot_c;

    std::vector<std::string> hygienic_names;
    /// quote `with (...)` 绑定名（与 bindings 平行）
    std::vector<std::string> binding_names;
    std::vector<RuntimeAstNode> bindings;

    struct CallArg {
        std::string kw_name;
        bool is_splat = false;
        std::unique_ptr<RuntimeAstNode> value;
    };

    std::vector<CallArg> call_args;

    [[nodiscard]] RuntimeAstNode clone() const;

    [[nodiscard]] bool empty() const {
        return kind == lmx::ASTNodeType::Unknown;
    }
};

class TempParseTree {
    std::vector<std::unique_ptr<lmx::ASTNode>> owned_;

public:
    TempParseTree() = default;
    ~TempParseTree();

    void hold(std::unique_ptr<lmx::ASTNode> node) { owned_.push_back(std::move(node)); }

    [[nodiscard]] lmx::ASTNode* to_parse_tree(const RuntimeAstNode& node);

    void clear() { owned_.clear(); }
};

[[nodiscard]] RuntimeAstNode ast_from_parse(const lmx::ASTNode* node);

[[nodiscard]] std::string ast_to_source(const RuntimeAstNode& node);

[[nodiscard]] Value make_ast_value(RuntimeAstNode node);

[[nodiscard]] bool value_is_ast(const Value& v);

[[nodiscard]] RuntimeAstNode value_as_ast(const Value& v);

[[nodiscard]] Value pack_ast_vec(std::vector<RuntimeAstNode> nodes, VM& vm);

[[nodiscard]] Value ast_vec_push(const Value& vec_value, const Value& ast_value, VM& vm);

[[nodiscard]] Value ast_vec_extend(const Value& vec_value, const Value& more_vec, VM& vm);

[[nodiscard]] Value runtime_ast_to_struct(VM& vm, const RuntimeAstNode& node);

void register_ast_type_converters();

void register_ast_struct_types();

} // namespace irgen
