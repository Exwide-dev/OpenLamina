#pragma once

#include <utility>
#include <vector>
#include <string>

namespace lmx {

// AST节点类型枚举
enum class ASTNodeType {
    Unknown,
    BlockStmt,
    Expr,
    VarDecl,
    FuncDecl,
    ExternFunc,
    ReturnStmt,
    Loop,
    Break,
    Continue,
    IfStmt,
    Module,
    Binary,
    Unary,
    Number,
    Bool,
    String,
    VarRef,
    FuncCallExpr,
    Type,
    CompositeType,
    Program,
    VMCall
};

// 前向声明
struct ASTNode;
struct ExprNode;
struct TypeNode;

// AST节点基类
struct ASTNode {
    ASTNodeType kind;
    std::vector<ASTNode*> children;

    explicit ASTNode(const ASTNodeType t) : kind(t) {}
    virtual ~ASTNode() {
        for (const auto child : children) {
            delete child;
        }
    }
};

struct ExprNode : ASTNode {
    explicit ExprNode(ASTNodeType t = ASTNodeType::Expr) : ASTNode(t) {}
    ~ExprNode() override = default;
};

struct TypeNode : ASTNode {
    std::string name;

    explicit TypeNode(std::string  n) : ASTNode(ASTNodeType::Type), name(std::move(n)) {}
    ~TypeNode() override = default;
};

struct CompositeTypeNode final : TypeNode {
    std::vector<TypeNode*> subtypes;

    CompositeTypeNode(const std::string& n, std::vector<TypeNode*> subtypes)
        : TypeNode(n), subtypes(std::move(subtypes)) {
        kind = ASTNodeType::CompositeType;
    }
    ~CompositeTypeNode() override {
        for (const auto type : subtypes) {
            delete type;
        }
    }
};

struct NumberNode final : ExprNode {
    std::string value;

    explicit NumberNode(std::string  v) : ExprNode(ASTNodeType::Number), value(std::move(v)) {}
    ~NumberNode() override = default;
};

struct BoolNode final : ExprNode {
    bool value;

    explicit BoolNode(const bool v) : ExprNode(ASTNodeType::Bool), value(v) {}
    ~BoolNode() override = default;
};

struct StringNode final : ExprNode {
    std::string value;

    explicit StringNode(std::string  v) : ExprNode(ASTNodeType::String), value(std::move(v)) {}
    ~StringNode() override = default;
};

struct VarRefNode : ExprNode {
    std::string name;

    explicit VarRefNode(std::string  n) : ExprNode(ASTNodeType::VarRef), name(std::move(n)) {}
    ~VarRefNode() override = default;
};

struct UnaryNode final : ExprNode {
    std::string op;
    ExprNode* operand;

    UnaryNode(std::string  o, ExprNode* oper)
        : ExprNode(ASTNodeType::Unary), op(std::move(o)), operand(oper) {}
    ~UnaryNode() override {
        delete operand;
    }
};

struct BinaryNode : ExprNode {
    std::string op;
    ExprNode* left;
    ExprNode* right;

    BinaryNode(ExprNode* l, ExprNode* r, std::string  o)
        : ExprNode(ASTNodeType::Binary), op(std::move(o)), left(l), right(r) {}
    ~BinaryNode() override {
        delete left;
        delete right;
    }
};

struct FuncCallExprNode final : ExprNode {
    std::string func_name;
    std::vector<ASTNode*> args;

    FuncCallExprNode(std::string  n, std::vector<ASTNode*> a)
        : ExprNode(ASTNodeType::FuncCallExpr), func_name(std::move(n)), args(std::move(a)) {}
    ~FuncCallExprNode() override {
        for (const auto arg : args) {
            delete arg;
        }
    }
};

struct VMCallNode final : ExprNode {
    std::string index;
    std::vector<ASTNode*> args;

    VMCallNode(std::string  idx, std::vector<ASTNode*> a)
        : ExprNode(ASTNodeType::VMCall), index(std::move(idx)), args(std::move(a)) {}
    ~VMCallNode() override {
        for (const auto arg : args) {
            delete arg;
        }
    }
};

struct BlockStmtNode final : ASTNode {
    std::vector<ASTNode*> stmts;

    explicit BlockStmtNode(std::vector<ASTNode*> s)
        : ASTNode(ASTNodeType::BlockStmt), stmts(std::move(s)) {}
    ~BlockStmtNode() override {
        for (const auto stmt : stmts) {
            delete stmt;
        }
    }
};

struct VarDeclNode final : ASTNode {
    std::string name;
    ExprNode* init;
    bool is_redecl;

    VarDeclNode(std::string  n, ExprNode* i, bool redec = true)
        : ASTNode(ASTNodeType::VarDecl), name(std::move(n)), init(i), is_redecl(redec) {}
    ~VarDeclNode() override {
        delete init;
    }
};

struct FuncDeclNode final : ASTNode {
    std::string name;
    std::vector<std::string> params;
    BlockStmtNode* body;
    std::vector<TypeNode*> args_type;
    TypeNode* ret_type{};

    FuncDeclNode(std::string  n, std::vector<std::string> p, BlockStmtNode* b)
        : ASTNode(ASTNodeType::FuncDecl), name(std::move(n)), params(std::move(p)), body(b) {}
    ~FuncDeclNode() override {
        delete body;
        for (const auto type : args_type) {
            delete type;
        }
        delete ret_type;
    }
};

struct ExternFuncNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<TypeNode*> args_type;
    StringNode* lib_name;
    TypeNode* ret_type;

    ExternFuncNode(std::string  n, std::vector<std::string> p,
                   std::vector<TypeNode*> a, StringNode* l,
                   TypeNode* r)
        : ASTNode(ASTNodeType::ExternFunc), name(std::move(n)), params(std::move(p)),
          args_type(std::move(a)), lib_name(l), ret_type(r) {}
    ~ExternFuncNode() override {
        delete lib_name;
        for (auto type : args_type) {
            delete type;
        }
        delete ret_type;
    }
};

struct ReturnStmtNode final : ASTNode {
    ExprNode* expr;

    explicit ReturnStmtNode(ExprNode* e)
        : ASTNode(ASTNodeType::ReturnStmt), expr(e) {}
    ~ReturnStmtNode() override {
        delete expr;
    }
};

struct LoopNode final : ASTNode {
    ExprNode* condition;
    BlockStmtNode* body;

    LoopNode(ExprNode* cond, BlockStmtNode* b)
        : ASTNode(ASTNodeType::Loop), condition(cond), body(b) {}
    ~LoopNode() override {
        delete condition;
        delete body;
    }
};

struct BreakNode final : ASTNode {
    BreakNode() : ASTNode(ASTNodeType::Break) {}
    ~BreakNode() override = default;
};

struct ContinueNode final : ASTNode {
    ContinueNode() : ASTNode(ASTNodeType::Continue) {}
    ~ContinueNode() override = default;
};

struct IfStmtNode final : ASTNode {
    ExprNode* condition;
    BlockStmtNode* then_block;
    BlockStmtNode* else_block;

    IfStmtNode(ExprNode* cond, BlockStmtNode* t,
              BlockStmtNode* e)
        : ASTNode(ASTNodeType::IfStmt), condition(cond),
          then_block(t), else_block(e) {}
    ~IfStmtNode() override {
        delete condition;
        delete then_block;
        delete else_block;
    }
};

struct ModuleNode final : ASTNode {
    enum class Types { ord, dyn };

    std::string name;
    Types type;
    StringNode* lib;
    std::vector<VarDeclNode*> vars;
    std::vector<FuncDeclNode*> ord_funcs;
    std::vector<ExternFuncNode*> dyn_funcs;
    std::vector<ModuleNode*> children;

    ModuleNode(std::string  n, const Types t, StringNode* l,
              std::vector<VarDeclNode*> v,
              std::vector<FuncDeclNode*> o,
              std::vector<ExternFuncNode*> d,
              std::vector<ModuleNode*> c)
        : ASTNode(ASTNodeType::Module), name(std::move(n)), type(t), lib(l),
          vars(std::move(v)), ord_funcs(std::move(o)), dyn_funcs(std::move(d)),
          children(std::move(c)) {}
    ~ModuleNode() override {
        delete lib;
        for (const auto var : vars) {
            delete var;
        }
        for (const auto func : ord_funcs) {
            delete func;
        }
        for (const auto func : dyn_funcs) {
            delete func;
        }
        for (const auto child : children) {
            delete child;
        }
    }
};

struct ProgramASTNode final : ASTNode {
    std::vector<ASTNode*> stmts;

    explicit ProgramASTNode(std::vector<ASTNode*> s)
        : ASTNode(ASTNodeType::Program), stmts(std::move(s)) {}
    ~ProgramASTNode() override {
        for (const auto stmt : stmts) {
            delete stmt;
        }
    }
};

} // namespace lmx
