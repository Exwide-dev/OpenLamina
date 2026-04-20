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
        : TypeNode(n), subtypes(subtypes) {
        kind = ASTNodeType::CompositeType;
    }
    ~CompositeTypeNode() {
        for (auto type : subtypes) {
            delete type;
        }
    }
};

struct NumberNode : ExprNode {
    std::string value;

    explicit NumberNode(const std::string& v) : ExprNode(ASTNodeType::Number), value(v) {}
    virtual ~NumberNode() = default;
};

struct BoolNode : ExprNode {
    bool value;

    explicit BoolNode(bool v) : ExprNode(ASTNodeType::Bool), value(v) {}
    virtual ~BoolNode() = default;
};

struct StringNode : ExprNode {
    std::string value;

    explicit StringNode(const std::string& v) : ExprNode(ASTNodeType::String), value(v) {}
    virtual ~StringNode() = default;
};

struct VarRefNode : ExprNode {
    std::string name;

    explicit VarRefNode(const std::string& n) : ExprNode(ASTNodeType::VarRef), name(n) {}
    virtual ~VarRefNode() = default;
};

struct UnaryNode : ExprNode {
    std::string op;
    ExprNode* operand;

    UnaryNode(const std::string& o, ExprNode* oper)
        : ExprNode(ASTNodeType::Unary), op(o), operand(oper) {}
    ~UnaryNode() {
        delete operand;
    }
};

struct BinaryNode : ExprNode {
    std::string op;
    ExprNode* left;
    ExprNode* right;

    BinaryNode(ExprNode* l, ExprNode* r, const std::string& o)
        : ExprNode(ASTNodeType::Binary), left(l), right(r), op(o) {}
    ~BinaryNode() {
        delete left;
        delete right;
    }
};

struct FuncCallExprNode : ExprNode {
    std::string func_name;
    std::vector<ASTNode*> args;

    FuncCallExprNode(const std::string& n, std::vector<ASTNode*> a)
        : ExprNode(ASTNodeType::FuncCallExpr), func_name(n), args(a) {}
    ~FuncCallExprNode() {
        for (auto arg : args) {
            delete arg;
        }
    }
};

struct VMCallNode : ExprNode {
    std::string index;
    std::vector<ASTNode*> args;

    VMCallNode(const std::string& idx, std::vector<ASTNode*> a)
        : ExprNode(ASTNodeType::VMCall), index(idx), args(a) {}
    ~VMCallNode() {
        for (auto arg : args) {
            delete arg;
        }
    }
};

struct BlockStmtNode : ASTNode {
    std::vector<ASTNode*> stmts;

    explicit BlockStmtNode(std::vector<ASTNode*> s)
        : ASTNode(ASTNodeType::BlockStmt), stmts(s) {}
    ~BlockStmtNode() {
        for (auto stmt : stmts) {
            delete stmt;
        }
    }
};

struct VarDeclNode : ASTNode {
    std::string name;
    ExprNode* init;
    bool is_redecl;

    VarDeclNode(const std::string& n, ExprNode* i, bool redec = true)
        : ASTNode(ASTNodeType::VarDecl), name(n), init(i), is_redecl(redec) {}
    ~VarDeclNode() {
        delete init;
    }
};

struct FuncDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    BlockStmtNode* body;
    std::vector<TypeNode*> args_type;
    TypeNode* ret_type;

    FuncDeclNode(const std::string& n, std::vector<std::string> p, BlockStmtNode* b)
        : ASTNode(ASTNodeType::FuncDecl), name(n), params(p), body(b) {}
    ~FuncDeclNode() {
        delete body;
        for (auto type : args_type) {
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

    ExternFuncNode(const std::string& n, std::vector<std::string> p,
                   std::vector<TypeNode*> a, StringNode* l,
                   TypeNode* r)
        : ASTNode(ASTNodeType::ExternFunc), name(n), params(p),
          args_type(a), lib_name(l), ret_type(r) {}
    ~ExternFuncNode() {
        delete lib_name;
        for (auto type : args_type) {
            delete type;
        }
        delete ret_type;
    }
};

struct ReturnStmtNode : ASTNode {
    ExprNode* expr;

    explicit ReturnStmtNode(ExprNode* e)
        : ASTNode(ASTNodeType::ReturnStmt), expr(e) {}
    ~ReturnStmtNode() {
        delete expr;
    }
};

struct LoopNode : ASTNode {
    ExprNode* condition;
    BlockStmtNode* body;

    LoopNode(ExprNode* cond, BlockStmtNode* b)
        : ASTNode(ASTNodeType::Loop), condition(cond), body(b) {}
    ~LoopNode() {
        delete condition;
        delete body;
    }
};

struct BreakNode : ASTNode {
    BreakNode() : ASTNode(ASTNodeType::Break) {}
    virtual ~BreakNode() = default;
};

struct ContinueNode : ASTNode {
    ContinueNode() : ASTNode(ASTNodeType::Continue) {}
    virtual ~ContinueNode() = default;
};

struct IfStmtNode : ASTNode {
    ExprNode* condition;
    BlockStmtNode* then_block;
    BlockStmtNode* else_block;

    IfStmtNode(ExprNode* cond, BlockStmtNode* t,
              BlockStmtNode* e)
        : ASTNode(ASTNodeType::IfStmt), condition(cond),
          then_block(t), else_block(e) {}
    ~IfStmtNode() {
        delete condition;
        delete then_block;
        delete else_block;
    }
};

struct ModuleNode : ASTNode {
    enum class Types { ord, dyn };

    std::string name;
    Types type;
    StringNode* lib;
    std::vector<VarDeclNode*> vars;
    std::vector<FuncDeclNode*> ord_funcs;
    std::vector<ExternFuncNode*> dyn_funcs;
    std::vector<ModuleNode*> children;

    ModuleNode(const std::string& n, Types t, StringNode* l,
              std::vector<VarDeclNode*> v,
              std::vector<FuncDeclNode*> o,
              std::vector<ExternFuncNode*> d,
              std::vector<ModuleNode*> c)
        : ASTNode(ASTNodeType::Module), name(n), type(t), lib(l),
          vars(v), ord_funcs(o), dyn_funcs(d),
          children(c) {}
    ~ModuleNode() {
        delete lib;
        for (auto var : vars) {
            delete var;
        }
        for (auto func : ord_funcs) {
            delete func;
        }
        for (auto func : dyn_funcs) {
            delete func;
        }
        for (auto child : children) {
            delete child;
        }
    }
};

struct ProgramASTNode : ASTNode {
    std::vector<ASTNode*> stmts;

    explicit ProgramASTNode(std::vector<ASTNode*> s)
        : ASTNode(ASTNodeType::Program), stmts(s) {}
    ~ProgramASTNode() {
        for (auto stmt : stmts) {
            delete stmt;
        }
    }
};

} // namespace lmx
