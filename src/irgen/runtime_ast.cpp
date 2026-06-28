#include "runtime_ast.hpp"

#include "struct_types.hpp"
#include "../tools/debug.hpp"

#include <format>
#include <stdexcept>

namespace irgen {
namespace {

RuntimeAstNode from_expr(const lmx::ExprNode* node);
RuntimeAstNode from_stmt(const lmx::ASTNode* node);
RuntimeAstNode from_any(const lmx::ASTNode* node);
RuntimeAstNode from_match_pattern(const lmx::MatchPatternNode* pattern);

RuntimeAstNode make_node(const lmx::ASTNode* node, const lmx::ASTNodeType kind) {
    RuntimeAstNode out;
    out.kind = kind;
    out.line = node != nullptr ? node->source_line : 0;
    return out;
}

void set_a(RuntimeAstNode& node, RuntimeAstNode value) {
    node.slot_a = std::make_unique<RuntimeAstNode>(std::move(value));
}

void set_b(RuntimeAstNode& node, RuntimeAstNode value) {
    node.slot_b = std::make_unique<RuntimeAstNode>(std::move(value));
}

void set_c(RuntimeAstNode& node, RuntimeAstNode value) {
    node.slot_c = std::make_unique<RuntimeAstNode>(std::move(value));
}

const RuntimeAstNode& get_a(const RuntimeAstNode& node) {
    static const RuntimeAstNode empty{};
    return node.slot_a != nullptr ? *node.slot_a : empty;
}

const RuntimeAstNode& get_b(const RuntimeAstNode& node) {
    static const RuntimeAstNode empty{};
    return node.slot_b != nullptr ? *node.slot_b : empty;
}

const RuntimeAstNode& get_c(const RuntimeAstNode& node) {
    static const RuntimeAstNode empty{};
    return node.slot_c != nullptr ? *node.slot_c : empty;
}

const RuntimeAstNode& get_call_arg(const RuntimeAstNode::CallArg& arg) {
    static const RuntimeAstNode empty{};
    return arg.value != nullptr ? *arg.value : empty;
}

RuntimeAstNode from_expr(const lmx::ExprNode* node) {
    if (node == nullptr) {
        return {};
    }

    switch (node->kind) {
        case lmx::ASTNodeType::Number: {
            auto out = make_node(node, node->kind);
            out.text = dynamic_cast<const lmx::NumberNode*>(node)->value;
            return out;
        }
        case lmx::ASTNodeType::String: {
            auto out = make_node(node, node->kind);
            out.text = dynamic_cast<const lmx::StringNode*>(node)->value;
            return out;
        }
        case lmx::ASTNodeType::Bool: {
            auto out = make_node(node, node->kind);
            out.bool_val = dynamic_cast<const lmx::BoolNode*>(node)->value;
            return out;
        }
        case lmx::ASTNodeType::VarRef: {
            auto out = make_node(node, node->kind);
            out.text = dynamic_cast<const lmx::VarRefNode*>(node)->name;
            return out;
        }
        case lmx::ASTNodeType::Placeholder:
            return make_node(node, node->kind);
        case lmx::ASTNodeType::Unary: {
            const auto* u = dynamic_cast<const lmx::UnaryNode*>(node);
            auto out = make_node(node, node->kind);
            out.text = u->op;
            set_a(out, from_expr(u->operand));
            return out;
        }
        case lmx::ASTNodeType::Binary: {
            const auto* b = dynamic_cast<const lmx::BinaryNode*>(node);
            auto out = make_node(node, node->kind);
            out.text = b->op;
            set_a(out, from_expr(b->left));
            set_b(out, from_expr(b->right));
            return out;
        }
        case lmx::ASTNodeType::FuncCallExpr: {
            const auto* c = dynamic_cast<const lmx::FuncCallExprNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(c->func_expr));
            for (const auto& arg : c->args) {
                RuntimeAstNode::CallArg ca;
                ca.kw_name = arg.name;
                ca.is_splat = arg.is_splat;
                ca.value = std::make_unique<RuntimeAstNode>(from_expr(arg.value));
                out.call_args.push_back(std::move(ca));
            }
            return out;
        }
        case lmx::ASTNodeType::MacroCallExpr: {
            const auto* c = dynamic_cast<const lmx::MacroCallExprNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(c->macro_expr));
            for (const auto& arg : c->args) {
                RuntimeAstNode::CallArg ca;
                ca.kw_name = arg.name;
                ca.is_splat = arg.is_splat;
                ca.value = std::make_unique<RuntimeAstNode>(from_expr(arg.value));
                out.call_args.push_back(std::move(ca));
            }
            return out;
        }
        case lmx::ASTNodeType::MemberAccess: {
            const auto* m = dynamic_cast<const lmx::MemberAccessNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(m->object));
            out.text = m->member;
            return out;
        }
        case lmx::ASTNodeType::TypeConvert: {
            const auto* t = dynamic_cast<const lmx::TypeConvertExprNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(t->type_expr));
            set_b(out, from_expr(t->value_expr));
            return out;
        }
        case lmx::ASTNodeType::IndexAccess: {
            const auto* i = dynamic_cast<const lmx::IndexAccessNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(i->object));
            set_b(out, from_expr(i->index));
            return out;
        }
        case lmx::ASTNodeType::Vector: {
            const auto* v = dynamic_cast<const lmx::VectorNode*>(node);
            auto out = make_node(node, node->kind);
            for (const auto* el : v->elements) {
                out.children.push_back(from_any(el));
            }
            return out;
        }
        case lmx::ASTNodeType::Dictionary: {
            const auto* d = dynamic_cast<const lmx::DictionaryNode*>(node);
            auto out = make_node(node, node->kind);
            for (const auto* entry : d->entries) {
                RuntimeAstNode pair_node;
                pair_node.kind = lmx::ASTNodeType::DictEntry;
                pair_node.line = entry->source_line;
                set_a(pair_node, from_expr(entry->key));
                set_b(pair_node, from_expr(entry->value));
                out.children.push_back(std::move(pair_node));
            }
            return out;
        }
        case lmx::ASTNodeType::QuoteExpr: {
            const auto* q = dynamic_cast<const lmx::QuoteExprNode*>(node);
            auto out = make_node(node, node->kind);
            out.hygienic_names = q->hygienic_names;
            for (const auto* b : q->bindings) {
                out.bindings.push_back(from_expr(b));
            }
            set_a(out, from_stmt(q->body));
            return out;
        }
        default:
            throw std::runtime_error(
                std::format("ast_from_parse: unsupported expression kind {}", static_cast<int>(node->kind))
            );
    }
}

RuntimeAstNode from_match_pattern(const lmx::MatchPatternNode* pattern) {
    if (pattern == nullptr) {
        return {};
    }
    RuntimeAstNode out = make_node(pattern, lmx::ASTNodeType::MatchPattern);
    switch (pattern->pattern_kind) {
        case lmx::MatchPatternKind::Expr:
            out.text = "Expr";
            set_a(out, from_expr(pattern->expr));
            break;
        case lmx::MatchPatternKind::Bind:
            out.text = "Bind";
            out.binding_names = {pattern->bind_name};
            break;
        case lmx::MatchPatternKind::Vector:
            out.text = "Vector";
            for (const auto* elem : pattern->elements) {
                out.children.push_back(from_match_pattern(elem));
            }
            break;
        case lmx::MatchPatternKind::Struct:
            out.text = "Struct";
            out.hygienic_names = {pattern->struct_type_name};
            out.binding_names = pattern->struct_field_binds;
            break;
        case lmx::MatchPatternKind::Or:
            out.text = "Or";
            for (const auto* alt : pattern->alternatives) {
                out.children.push_back(from_match_pattern(alt));
            }
            break;
    }
    return out;
}

RuntimeAstNode from_stmt(const lmx::ASTNode* node) {
    if (node == nullptr) {
        return {};
    }

    switch (node->kind) {
        case lmx::ASTNodeType::BlockStmt: {
            const auto* b = dynamic_cast<const lmx::BlockStmtNode*>(node);
            auto out = make_node(node, node->kind);
            for (const auto* s : b->stmts) {
                out.stmts.push_back(from_any(s));
            }
            return out;
        }
        case lmx::ASTNodeType::VarDecl: {
            const auto* v = dynamic_cast<const lmx::VarDeclNode*>(node);
            auto out = make_node(node, node->kind);
            out.text = v->name;
            out.bool_val = v->is_const;
            if (v->init) {
                set_a(out, from_expr(v->init));
            }
            return out;
        }
        case lmx::ASTNodeType::Assign: {
            const auto* a = dynamic_cast<const lmx::AssignNode*>(node);
            auto out = make_node(node, node->kind);
            if (a->var != nullptr && a->var->kind == lmx::ASTNodeType::VarRef) {
                out.text = dynamic_cast<const lmx::VarRefNode*>(a->var)->name;
            }
            set_a(out, from_expr(a->value));
            return out;
        }
        case lmx::ASTNodeType::ReturnStmt: {
            const auto* r = dynamic_cast<const lmx::ReturnStmtNode*>(node);
            auto out = make_node(node, node->kind);
            if (r->expr) {
                set_a(out, from_expr(r->expr));
            }
            return out;
        }
        case lmx::ASTNodeType::IfStmt: {
            const auto* i = dynamic_cast<const lmx::IfStmtNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(i->condition));
            set_b(out, from_stmt(i->then_block));
            if (i->else_block) {
                set_c(out, from_stmt(i->else_block));
            }
            return out;
        }
        case lmx::ASTNodeType::MatchCase: {
            const auto* match_case = dynamic_cast<const lmx::MatchCaseNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_match_pattern(match_case->pattern));
            set_b(out, from_stmt(match_case->body));
            return out;
        }
        case lmx::ASTNodeType::MatchStmt: {
            const auto* match_stmt = dynamic_cast<const lmx::MatchStmtNode*>(node);
            auto out = make_node(node, node->kind);
            set_a(out, from_expr(match_stmt->subject));
            for (const auto* match_case : match_stmt->cases) {
                out.stmts.push_back(from_stmt(match_case));
            }
            if (match_stmt->else_block != nullptr) {
                set_c(out, from_stmt(match_stmt->else_block));
            }
            return out;
        }
        default:
            if (dynamic_cast<const lmx::ExprNode*>(node) != nullptr) {
                return from_expr(dynamic_cast<const lmx::ExprNode*>(node));
            }
            throw std::runtime_error(
                std::format("ast_from_parse: unsupported statement kind {}", static_cast<int>(node->kind))
            );
    }
}

RuntimeAstNode from_any(const lmx::ASTNode* node) {
    if (node == nullptr) {
        return {};
    }
    if (dynamic_cast<const lmx::ExprNode*>(node) != nullptr &&
        node->kind != lmx::ASTNodeType::BlockStmt) {
        return from_expr(dynamic_cast<const lmx::ExprNode*>(node));
    }
    return from_stmt(node);
}

lmx::ExprNode* to_expr(TempParseTree& tree, const RuntimeAstNode& node);
lmx::ASTNode* to_stmt(TempParseTree& tree, const RuntimeAstNode& node);
lmx::MatchPatternNode* to_match_pattern(TempParseTree& tree, const RuntimeAstNode& node);

lmx::ExprNode* to_expr(TempParseTree& tree, const RuntimeAstNode& node) {
    switch (node.kind) {
        case lmx::ASTNodeType::Number: {
            auto* n = new lmx::NumberNode(node.text);
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::String: {
            auto* n = new lmx::StringNode(node.text);
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::Bool: {
            auto* n = new lmx::BoolNode(node.bool_val);
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::VarRef: {
            auto* n = new lmx::VarRefNode(node.text);
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::Unary: {
            auto* n = new lmx::UnaryNode(
                node.text,
                to_expr(tree, get_a(node))
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::Binary: {
            auto* n = new lmx::BinaryNode(
                to_expr(tree, get_a(node)),
                to_expr(tree, get_b(node)),
                node.text
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::FuncCallExpr: {
            std::vector<lmx::CallArgument> args;
            args.reserve(node.call_args.size());
            for (const auto& arg : node.call_args) {
                lmx::CallArgument ca(to_expr(tree, get_call_arg(arg)), arg.kw_name, arg.is_splat);
                args.push_back(std::move(ca));
            }
            auto* n = new lmx::FuncCallExprNode(
                to_expr(tree, get_a(node)),
                std::move(args)
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::MacroCallExpr: {
            std::vector<lmx::CallArgument> args;
            args.reserve(node.call_args.size());
            for (const auto& arg : node.call_args) {
                lmx::CallArgument ca(to_expr(tree, get_call_arg(arg)), arg.kw_name, arg.is_splat);
                args.push_back(std::move(ca));
            }
            auto* n = new lmx::MacroCallExprNode(
                to_expr(tree, get_a(node)),
                std::move(args)
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::MemberAccess: {
            auto* n = new lmx::MemberAccessNode(
                to_expr(tree, get_a(node)),
                node.text
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::TypeConvert: {
            auto* n = new lmx::TypeConvertExprNode(
                to_expr(tree, get_a(node)),
                to_expr(tree, get_b(node))
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::IndexAccess: {
            auto* n = new lmx::IndexAccessNode(
                to_expr(tree, get_a(node)),
                to_expr(tree, get_b(node))
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::Vector: {
            std::vector<lmx::ASTNode*> elements;
            for (const auto& child : node.children) {
                elements.push_back(to_expr(tree, child));
            }
            auto* n = new lmx::VectorNode(std::move(elements));
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::BlockStmt:
            return dynamic_cast<lmx::ExprNode*>(to_stmt(tree, node));
        default:
            throw std::runtime_error(
                std::format("to_parse_tree: unsupported expr kind {}", static_cast<int>(node.kind))
            );
    }
}

lmx::MatchPatternNode* to_match_pattern(TempParseTree& tree, const RuntimeAstNode& node) {
    if (node.text == "Expr") {
        auto* n = new lmx::MatchPatternNode(lmx::MatchPatternKind::Expr, to_expr(tree, get_a(node)));
        n->source_line = node.line;
        return n;
    }
    if (node.text == "Bind") {
        const std::string bind = node.binding_names.empty() ? std::string{} : node.binding_names.front();
        auto* n = new lmx::MatchPatternNode(lmx::MatchPatternKind::Bind, nullptr, bind);
        n->source_line = node.line;
        return n;
    }
    if (node.text == "Vector") {
        std::vector<lmx::MatchPatternNode*> elements;
        elements.reserve(node.children.size());
        for (const auto& child : node.children) {
            elements.push_back(to_match_pattern(tree, child));
        }
        auto* n = new lmx::MatchPatternNode(
            lmx::MatchPatternKind::Vector,
            nullptr,
            std::string{},
            std::move(elements)
        );
        n->source_line = node.line;
        return n;
    }
    if (node.text == "Struct") {
        const std::string type_name = node.hygienic_names.empty() ? std::string{} : node.hygienic_names.front();
        auto* n = new lmx::MatchPatternNode(
            lmx::MatchPatternKind::Struct,
            nullptr,
            std::string{},
            {},
            {},
            type_name,
            node.binding_names
        );
        n->source_line = node.line;
        return n;
    }
    if (node.text == "Or") {
        std::vector<lmx::MatchPatternNode*> alternatives;
        alternatives.reserve(node.children.size());
        for (const auto& child : node.children) {
            alternatives.push_back(to_match_pattern(tree, child));
        }
        auto* n = new lmx::MatchPatternNode(
            lmx::MatchPatternKind::Or,
            nullptr,
            std::string{},
            {},
            std::move(alternatives)
        );
        n->source_line = node.line;
        return n;
    }
    throw std::runtime_error("to_match_pattern: unknown match pattern tag");
}

lmx::ASTNode* to_stmt(TempParseTree& tree, const RuntimeAstNode& node) {
    switch (node.kind) {
        case lmx::ASTNodeType::BlockStmt: {
            std::vector<lmx::ASTNode*> stmts;
            for (const auto& s : node.stmts) {
                stmts.push_back(to_stmt(tree, s));
            }
            auto* n = new lmx::BlockStmtNode(std::move(stmts));
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::VarDecl: {
            auto* n = new lmx::VarDeclNode(
                node.text,
                node.slot_a != nullptr ? to_expr(tree, *node.slot_a) : nullptr,
                node.bool_val,
                lmx::Visibility::Internal
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::Assign: {
            auto* var = new lmx::VarRefNode(node.text);
            auto* n = new lmx::AssignNode(
                var,
                to_expr(tree, get_a(node))
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::ReturnStmt: {
            auto* n = new lmx::ReturnStmtNode(
                node.slot_a != nullptr ? to_expr(tree, *node.slot_a) : nullptr
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::IfStmt: {
            auto* n = new lmx::IfStmtNode(
                to_expr(tree, get_a(node)),
                dynamic_cast<lmx::BlockStmtNode*>(to_stmt(tree, get_b(node))),
                {},
                node.slot_c != nullptr
                    ? dynamic_cast<lmx::BlockStmtNode*>(to_stmt(tree, *node.slot_c))
                    : nullptr
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::MatchCase: {
            auto* n = new lmx::MatchCaseNode(
                to_match_pattern(tree, get_a(node)),
                dynamic_cast<lmx::BlockStmtNode*>(to_stmt(tree, get_b(node)))
            );
            n->source_line = node.line;
            return n;
        }
        case lmx::ASTNodeType::MatchStmt: {
            std::vector<lmx::MatchCaseNode*> cases;
            cases.reserve(node.stmts.size());
            for (const auto& match_case : node.stmts) {
                cases.push_back(dynamic_cast<lmx::MatchCaseNode*>(to_stmt(tree, match_case)));
            }
            auto* n = new lmx::MatchStmtNode(
                to_expr(tree, get_a(node)),
                std::move(cases),
                node.slot_c != nullptr
                    ? dynamic_cast<lmx::BlockStmtNode*>(to_stmt(tree, *node.slot_c))
                    : nullptr
            );
            n->source_line = node.line;
            return n;
        }
        default:
            return to_expr(tree, node);
    }
}

std::string expr_to_source(const RuntimeAstNode& node);
std::string stmt_to_source(const RuntimeAstNode& node);

std::string expr_to_source(const RuntimeAstNode& node) {
    switch (node.kind) {
        case lmx::ASTNodeType::Number:
            return node.text;
        case lmx::ASTNodeType::String:
            return std::format("\"{}\"", node.text);
        case lmx::ASTNodeType::Bool:
            return node.bool_val ? "true" : "false";
        case lmx::ASTNodeType::VarRef:
            return node.text;
        case lmx::ASTNodeType::Unary:
            return node.text + expr_to_source(get_a(node));
        case lmx::ASTNodeType::Binary: {
            const std::string left = expr_to_source(get_a(node));
            const std::string right = expr_to_source(get_b(node));
            return std::format("{} {} {}", left, node.text, right);
        }
        case lmx::ASTNodeType::FuncCallExpr: {
            std::string callee = expr_to_source(get_a(node));
            std::string args;
            for (size_t i = 0; i < node.call_args.size(); ++i) {
                if (i > 0) {
                    args += ", ";
                }
                if (node.call_args[i].is_splat) {
                    args += "*";
                }
                if (!node.call_args[i].kw_name.empty()) {
                    args += node.call_args[i].kw_name + "=";
                }
                args += expr_to_source(get_call_arg(node.call_args[i]));
            }
            return std::format("{}({})", callee, args);
        }
        case lmx::ASTNodeType::MacroCallExpr: {
            std::string callee = expr_to_source(get_a(node));
            std::string args;
            for (size_t i = 0; i < node.call_args.size(); ++i) {
                if (i > 0) {
                    args += ", ";
                }
                if (node.call_args[i].is_splat) {
                    args += "*";
                }
                args += expr_to_source(get_call_arg(node.call_args[i]));
            }
            return callee + "{" + args + "}";
        }
        case lmx::ASTNodeType::MemberAccess:
            return expr_to_source(get_a(node)) + "." + node.text;
        case lmx::ASTNodeType::TypeConvert:
            return expr_to_source(get_a(node)) +
                   ".(" + expr_to_source(get_b(node)) + ")";
        case lmx::ASTNodeType::IndexAccess:
            return expr_to_source(get_a(node)) + "[" +
                   expr_to_source(get_b(node)) + "]";
        case lmx::ASTNodeType::Vector: {
            std::string inner;
            for (size_t i = 0; i < node.children.size(); ++i) {
                if (i > 0) {
                    inner += ", ";
                }
                inner += expr_to_source(node.children[i]);
            }
            return std::format("[{}]", inner);
        }
        case lmx::ASTNodeType::BlockStmt:
            return stmt_to_source(node);
        default:
            return "<ast>";
    }
}

std::string stmt_to_source(const RuntimeAstNode& node) {
    switch (node.kind) {
        case lmx::ASTNodeType::BlockStmt: {
            std::string body;
            for (const auto& s : node.stmts) {
                body += stmt_to_source(s) + "\n";
            }
            return "{\n" + body + "}";
        }
        case lmx::ASTNodeType::VarDecl: {
            std::string init;
            if (node.slot_a != nullptr) {
                init = " = " + expr_to_source(*node.slot_a);
            }
            return (node.bool_val ? "const " : "var ") + node.text + init;
        }
        case lmx::ASTNodeType::Assign:
            return node.text + " = " + expr_to_source(get_a(node));
        case lmx::ASTNodeType::ReturnStmt:
            if (node.slot_a != nullptr) {
                return "return " + expr_to_source(*node.slot_a);
            }
            return "return";
        case lmx::ASTNodeType::IfStmt: {
            std::string out = "if (" + expr_to_source(get_a(node)) + ") " +
                              stmt_to_source(get_b(node));
            if (node.slot_c != nullptr) {
                out += " else " + stmt_to_source(*node.slot_c);
            }
            return out;
        }
        default:
            return expr_to_source(node);
    }
}

} // namespace

RuntimeAstNode RuntimeAstNode::clone() const {
    RuntimeAstNode out;
    out.kind = kind;
    out.line = line;
    out.text = text;
    out.bool_val = bool_val;
    out.hygienic_names = hygienic_names;
    out.binding_names = binding_names;
    out.stmts.reserve(stmts.size());
    for (const auto& s : stmts) {
        out.stmts.push_back(s.clone());
    }
    out.children.clear();
    out.children.reserve(children.size());
    for (const auto& c : children) {
        out.children.push_back(c.clone());
    }
    out.call_args.clear();
    out.call_args.reserve(call_args.size());
    for (const auto& a : call_args) {
        RuntimeAstNode::CallArg ca;
        ca.kw_name = a.kw_name;
        ca.is_splat = a.is_splat;
        if (a.value != nullptr) {
            ca.value = std::make_unique<RuntimeAstNode>(a.value->clone());
        }
        out.call_args.push_back(std::move(ca));
    }
    if (slot_a != nullptr) {
        out.slot_a = std::make_unique<RuntimeAstNode>(slot_a->clone());
    }
    if (slot_b != nullptr) {
        out.slot_b = std::make_unique<RuntimeAstNode>(slot_b->clone());
    }
    if (slot_c != nullptr) {
        out.slot_c = std::make_unique<RuntimeAstNode>(slot_c->clone());
    }
    out.bindings.clear();
    out.bindings.reserve(bindings.size());
    for (const auto& b : bindings) {
        out.bindings.push_back(b.clone());
    }
    return out;
}

RuntimeAstNode ast_from_parse(const lmx::ASTNode* node) {
    return from_any(node);
}

TempParseTree::~TempParseTree() = default;

lmx::ASTNode* TempParseTree::to_parse_tree(const RuntimeAstNode& node) {
    owned_.clear();
    lmx::ASTNode* root = to_stmt(*this, node);
    owned_.push_back(std::unique_ptr<lmx::ASTNode>(root));
    return root;
}

std::string ast_to_source(const RuntimeAstNode& node) {
    if (node.kind == lmx::ASTNodeType::BlockStmt ||
        node.kind == lmx::ASTNodeType::VarDecl ||
        node.kind == lmx::ASTNodeType::Assign ||
        node.kind == lmx::ASTNodeType::ReturnStmt ||
        node.kind == lmx::ASTNodeType::IfStmt) {
        return stmt_to_source(node);
    }
    return expr_to_source(node);
}

Value make_ast_value(RuntimeAstNode node) {
    return Value(std::make_shared<RuntimeAstNode>(std::move(node)));
}

bool value_is_ast(const Value& v) {
    return v.deref().getType() == Value::Type::RuntimeAst;
}

RuntimeAstNode value_as_ast(const Value& v) {
    return v.deref().asRuntimeAst().clone();
}

Value pack_ast_vec(std::vector<RuntimeAstNode> nodes, VM& vm) {
    std::vector<std::shared_ptr<Value>> items;
    items.reserve(nodes.size());
    for (auto& n : nodes) {
        items.push_back(vm.cell_pool.allocateValue(make_ast_value(std::move(n))));
    }
    return Value(std::move(items));
}

void register_ast_type_converters() {
    // AST → text 由 coerce_primitive 在 type_call / convert 路径处理
}

namespace {

void register_ast_struct(
    const std::string& name,
    const std::string& base,
    const std::vector<std::pair<std::string, std::string>>& fields
) {
    if (is_type_name(name)) {
        return;
    }
    StructTypeDef def;
    def.name = name;
    def.base_name = base;
    def.kind = TypeKind::User;
    def.typed = true;
    for (const auto& [field_name, field_type] : fields) {
        StructFieldDef fd;
        fd.name = field_name;
        fd.type_name = field_type;
        fd.has_type_annotation = true;
        fd.mutable_field = false;
        def.fields.push_back(std::move(fd));
    }
    (void)register_type_def(std::move(def));
}

Value ast_field_value(const RuntimeAstNode* slot) {
    if (slot == nullptr) {
        return make_ast_value(RuntimeAstNode{});
    }
    return make_ast_value(slot->clone());
}

Value pack_call_arg_nodes(const std::vector<RuntimeAstNode::CallArg>& call_args, VM& vm) {
    std::vector<RuntimeAstNode> nodes;
    nodes.reserve(call_args.size());
    for (const auto& arg : call_args) {
        if (arg.is_splat) {
            throw RuntimeError("internal error: splat in materialized call arg list");
        }
        if (arg.value != nullptr) {
            nodes.push_back(arg.value->clone());
        }
    }
    return pack_ast_vec(std::move(nodes), vm);
}

Value pack_child_nodes(const std::vector<RuntimeAstNode>& children, VM& vm) {
    std::vector<RuntimeAstNode> nodes;
    nodes.reserve(children.size());
    for (const auto& child : children) {
        nodes.push_back(child.clone());
    }
    return pack_ast_vec(std::move(nodes), vm);
}

Value pack_binding_nodes(const std::vector<RuntimeAstNode>& bindings, VM& vm) {
    std::vector<RuntimeAstNode> nodes;
    nodes.reserve(bindings.size());
    for (const auto& binding : bindings) {
        nodes.push_back(binding.clone());
    }
    return pack_ast_vec(std::move(nodes), vm);
}

} // namespace

Value ast_vec_push(const Value& vec_value, const Value& ast_value, VM& vm) {
    const Value& vec = vec_value.deref();
    if (vec.getType() != Value::Type::Vector) {
        throw RuntimeError("__ast_vec_push__ expects a vector");
    }
    if (!value_is_ast(ast_value)) {
        throw RuntimeError("__ast_vec_push__ expects an AST element");
    }
    std::vector<std::shared_ptr<Value>> out = vec.asVector();
    out.push_back(vm.cell_pool.allocateCopy(ast_value));
    return Value(std::move(out));
}

Value ast_vec_extend(const Value& vec_value, const Value& more_vec, VM& vm) {
    const Value& base = vec_value.deref();
    const Value& extra = more_vec.deref();
    if (base.getType() != Value::Type::Vector || extra.getType() != Value::Type::Vector) {
        throw RuntimeError("__ast_vec_extend__ expects two vectors");
    }
    std::vector<std::shared_ptr<Value>> out = base.asVector();
    for (const auto& elem : extra.asVector()) {
        if (elem == nullptr || !value_is_ast(*elem)) {
            throw RuntimeError("__ast_vec_extend__ expects AST elements");
        }
        out.push_back(vm.cell_pool.allocateCopy(*elem));
    }
    return Value(std::move(out));
}

Value runtime_ast_to_struct(VM& vm, const RuntimeAstNode& node) {
    switch (node.kind) {
        case lmx::ASTNodeType::Number:
            return make_struct_instance(vm, "AstNumber", {Value(node.text)});
        case lmx::ASTNodeType::String:
            return make_struct_instance(vm, "AstString", {Value(node.text)});
        case lmx::ASTNodeType::Bool:
            return make_struct_instance(vm, "AstBool", {Value(node.bool_val)});
        case lmx::ASTNodeType::VarRef:
            return make_struct_instance(vm, "AstVarRef", {Value(node.text)});
        case lmx::ASTNodeType::Unary:
            return make_struct_instance(
                vm,
                "AstUnary",
                {Value(node.text), ast_field_value(node.slot_a.get())}
            );
        case lmx::ASTNodeType::Binary:
            return make_struct_instance(
                vm,
                "AstBinary",
                {
                    Value(node.text),
                    ast_field_value(node.slot_a.get()),
                    ast_field_value(node.slot_b.get()),
                }
            );
        case lmx::ASTNodeType::FuncCallExpr:
            return make_struct_instance(
                vm,
                "AstFuncCall",
                {ast_field_value(node.slot_a.get()), pack_call_arg_nodes(node.call_args, vm)}
            );
        case lmx::ASTNodeType::MacroCallExpr:
            return make_struct_instance(
                vm,
                "AstMacroCall",
                {ast_field_value(node.slot_a.get()), pack_call_arg_nodes(node.call_args, vm)}
            );
        case lmx::ASTNodeType::MemberAccess:
            return make_struct_instance(
                vm,
                "AstMemberAccess",
                {ast_field_value(node.slot_a.get()), Value(node.text)}
            );
        case lmx::ASTNodeType::TypeConvert:
            return make_struct_instance(
                vm,
                "AstTypeConvert",
                {ast_field_value(node.slot_a.get()), ast_field_value(node.slot_b.get())}
            );
        case lmx::ASTNodeType::IndexAccess:
            return make_struct_instance(
                vm,
                "AstIndexAccess",
                {ast_field_value(node.slot_a.get()), ast_field_value(node.slot_b.get())}
            );
        case lmx::ASTNodeType::Vector:
            return make_struct_instance(
                vm,
                "AstVector",
                {pack_child_nodes(node.children, vm)}
            );
        case lmx::ASTNodeType::QuoteExpr: {
            std::vector<Value> positional = {
                node.slot_a != nullptr ? make_ast_value(node.slot_a->clone()) : make_ast_value(RuntimeAstNode{}),
                pack_binding_nodes(node.bindings, vm),
            };
            return make_struct_instance(vm, "AstQuote", std::move(positional));
        }
        default:
            throw RuntimeError(
                std::format("cannot convert AST kind {} to struct", static_cast<int>(node.kind))
            );
    }
}

void register_ast_struct_types() {
    register_ast_struct("AstNode", "", {});

    register_ast_struct("AstNumber", "AstNode", {{"value", "text"}});
    register_ast_struct("AstString", "AstNode", {{"value", "text"}});
    register_ast_struct("AstBool", "AstNode", {{"value", "bool"}});
    register_ast_struct("AstVarRef", "AstNode", {{"name", "text"}});
    register_ast_struct("AstUnary", "AstNode", {{"op", "text"}, {"operand", "AST"}});
    register_ast_struct("AstBinary", "AstNode", {{"op", "text"}, {"left", "AST"}, {"right", "AST"}});
    register_ast_struct("AstFuncCall", "AstNode", {{"callee", "AST"}, {"args", "vector"}});
    register_ast_struct("AstMacroCall", "AstNode", {{"callee", "AST"}, {"args", "vector"}});
    register_ast_struct("AstMemberAccess", "AstNode", {{"object", "AST"}, {"member", "text"}});
    register_ast_struct("AstTypeConvert", "AstNode", {{"type_expr", "AST"}, {"value", "AST"}});
    register_ast_struct("AstIndexAccess", "AstNode", {{"object", "AST"}, {"index", "AST"}});
    register_ast_struct("AstVector", "AstNode", {{"elements", "vector"}});
    register_ast_struct("AstQuote", "AstNode", {{"body", "AST"}, {"bindings", "vector"}});
}

} // namespace irgen
