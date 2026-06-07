#pragma once

#include <utility>
#include <vector>
#include <string>

namespace lmx {
/**
 * @struct UseItem
 * @brief use 语句中的导入项，包含名称和别名
 */
struct UseItem {
    std::string name;  ///< 原始名称
    std::string alias; ///< 别名

    /**
     * @brief 构造函数
     * @param n 名称
     * @param a 别名
     */
    UseItem(std::string n, std::string a) : name(std::move(n)), alias(std::move(a)) {
    }
};

/**
 * @enum ValueCategory
 * @brief 值类别，区分左值和右值
 */
enum class ValueCategory {
    LVALUE, ///< 左值，可以出现在赋值语句左侧
    RVALUE  ///< 右值，只能出现在赋值语句右侧
};

/**
 * @enum Visibility
 * @brief 符号可见性
 */
enum class Visibility {
    Exported, ///< 导出的，可被外部模块访问
    Internal  ///< 内部的，仅模块内部可见
};

/**
 * @enum ASTNodeType
 * @brief AST 节点类型枚举
 */
enum class ASTNodeType {
    Unknown,       ///< 未知类型
    BlockStmt,     ///< 块语句
    Expr,          ///< 表达式
    VarDecl,       ///< 变量声明
    Assign,        ///< 赋值语句
    FuncDecl,      ///< 函数声明
    DoFuncDecl,    ///< do 表达式形式的函数
    DecoratedFunc, ///< 带装饰器的函数
    ExternFunc,    ///< 外部函数声明
    ReturnStmt,    ///< 返回语句
    Loop,          ///< loop 循环
    WhileStmt,     ///< while 循环
    Break,         ///< break 语句
    Continue,      ///< continue 语句
    ForLoop,       ///< for 循环
    IfStmt,        ///< if 语句
    Module,        ///< 模块
    Binary,        ///< 二元表达式
    Unary,         ///< 一元表达式
    Number,        ///< 数字字面量
    Bool,          ///< 布尔字面量
    String,        ///< 字符串字面量
    VarRef,        ///< 变量引用
    FuncCallExpr,  ///< 函数调用表达式
    MemberAccess,  ///< 成员访问
    Type,          ///< 类型节点
    CompositeType, ///< 复合类型
    Program,       ///< 程序根节点
    VMCall,        ///< VM 调用
    Import,        ///< import 语句
    Use,           ///< use 语句
    Vector,        ///< 向量字面量
    IndexAccess,   ///< 索引访问
    Dictionary,    ///< 字典字面量
    DictEntry,     ///< 字典条目
    StructDecl     ///< struct 类型声明
};

struct ASTNode;
struct ExprNode;
struct TypeNode;

/**
 * @struct ASTNode
 * @brief AST 节点基类
 */
struct ASTNode {
    ASTNodeType kind;               ///< 节点类型
    std::vector<ASTNode*> children; ///< 子节点列表

    /**
     * @brief 构造函数
     * @param t 节点类型
     */
    explicit ASTNode(const ASTNodeType t) : kind(t) {
    }

    /**
     * @brief 默认构造函数
     */
    ASTNode() = default;

    /**
     * @brief 析构函数，递归删除所有子节点
     */
    virtual ~ASTNode() {
        for (const auto child : children) {
            delete child;
        }
    }
};

/**
 * @struct ExprNode
 * @brief 表达式节点基类
 */
struct ExprNode : ASTNode {
    /**
     * @brief 构造函数
     * @param t 节点类型，默认为 Expr
     */
    explicit ExprNode(const ASTNodeType t = ASTNodeType::Expr) : ASTNode(t) {
    }

    /**
     * @brief 析构函数
     */
    ~ExprNode() override = default;

    /**
     * @brief 获取值类别
     * @return 值类别
     */
    [[nodiscard]] virtual ValueCategory getValueCategory() const {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct TypeNode
 * @brief 类型节点
 */
struct TypeNode : ASTNode {
    std::string name; ///< 类型名称

    /**
     * @brief 构造函数
     * @param n 类型名称
     */
    explicit TypeNode(std::string n) : ASTNode(ASTNodeType::Type), name(std::move(n)) {
    }

    /**
     * @brief 析构函数
     */
    ~TypeNode() override = default;
};

/**
 * @struct CompositeTypeNode
 * @brief 复合类型节点（如泛型类型）
 */
struct CompositeTypeNode final : TypeNode {
    std::vector<TypeNode*> subtypes; ///< 子类型列表

    /**
     * @brief 构造函数
     * @param n 类型名称
     * @param subtypes 子类型列表
     */
    CompositeTypeNode(const std::string& n, std::vector<TypeNode*> subtypes)
        : TypeNode(n), subtypes(std::move(subtypes)) {
        kind = ASTNodeType::CompositeType;
    }

    /**
     * @brief 析构函数
     */
    ~CompositeTypeNode() override {
        for (const auto type : subtypes) {
            delete type;
        }
    }
};

/**
 * @struct NumberNode
 * @brief 数字字面量节点
 */
struct NumberNode final : ExprNode {
    std::string value; ///< 数字字符串表示

    /**
     * @brief 构造函数
     * @param v 数字字符串
     */
    explicit NumberNode(std::string v) : ExprNode(ASTNodeType::Number), value(std::move(v)) {
    }

    /**
     * @brief 析构函数
     */
    ~NumberNode() override = default;

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct BoolNode
 * @brief 布尔字面量节点
 */
struct BoolNode final : ExprNode {
    bool value; ///< 布尔值

    /**
     * @brief 构造函数
     * @param v 布尔值
     */
    explicit BoolNode(const bool v) : ExprNode(ASTNodeType::Bool), value(v) {
    }

    /**
     * @brief 析构函数
     */
    ~BoolNode() override = default;

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct StringNode
 * @brief 字符串字面量节点
 */
struct StringNode final : ExprNode {
    std::string value; ///< 字符串值

    /**
     * @brief 构造函数
     * @param v 字符串值
     */
    explicit StringNode(std::string v) : ExprNode(ASTNodeType::String), value(std::move(v)) {
    }

    /**
     * @brief 析构函数
     */
    ~StringNode() override = default;

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct VectorNode
 * @brief 向量字面量节点
 */
struct VectorNode final : ExprNode {
    std::vector<ASTNode*> elements; ///< 元素列表

    /**
     * @brief 构造函数
     * @param e 元素列表
     */
    explicit VectorNode(std::vector<ASTNode*> e)
        : ExprNode(ASTNodeType::Vector), elements(std::move(e)) {
    }

    /**
     * @brief 析构函数
     */
    ~VectorNode() override {
        for (const auto elem : elements) {
            delete elem;
        }
    }

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct DictEntryNode
 * @brief 字典条目节点
 */
struct DictEntryNode final : ASTNode {
    ExprNode* key;   ///< 键表达式
    ExprNode* value; ///< 值表达式

    /**
     * @brief 构造函数
     * @param k 键表达式
     * @param v 值表达式
     */
    DictEntryNode(ExprNode* k, ExprNode* v) : ASTNode(ASTNodeType::DictEntry), key(k), value(v) {
    }

    /**
     * @brief 析构函数
     */
    ~DictEntryNode() override {
        delete key;
        delete value;
    }
};

/**
 * @struct DictionaryNode
 * @brief 字典字面量节点
 */
struct DictionaryNode final : ExprNode {
    std::vector<DictEntryNode*> entries; ///< 条目列表

    /**
     * @brief 构造函数
     * @param e 条目列表
     */
    explicit DictionaryNode(std::vector<DictEntryNode*> e)
        : ExprNode(ASTNodeType::Dictionary), entries(std::move(e)) {
    }

    /**
     * @brief 析构函数
     */
    ~DictionaryNode() override {
        for (const auto entry : entries) {
            delete entry;
        }
    }

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct VarRefNode
 * @brief 变量引用节点
 */
struct VarRefNode : ExprNode {
    std::string name; ///< 变量名称

    /**
     * @brief 构造函数
     * @param n 变量名称
     */
    explicit VarRefNode(std::string n) : ExprNode(ASTNodeType::VarRef), name(std::move(n)) {
    }

    /**
     * @brief 析构函数
     */
    ~VarRefNode() override = default;

    /**
     * @brief 获取值类别
     * @return LVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::LVALUE;
    }
};

/**
 * @struct UnaryNode
 * @brief 一元表达式节点
 */
struct UnaryNode final : ExprNode {
    std::string op;    ///< 运算符
    ExprNode* operand; ///< 操作数

    /**
     * @brief 构造函数
     * @param o 运算符
     * @param oper 操作数
     */
    UnaryNode(std::string o, ExprNode* oper)
        : ExprNode(ASTNodeType::Unary), op(std::move(o)), operand(oper) {
    }

    /**
     * @brief 析构函数
     */
    ~UnaryNode() override {
        delete operand;
    }

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct BinaryNode
 * @brief 二元表达式节点
 */
struct BinaryNode : ExprNode {
    std::string op;  ///< 运算符
    ExprNode* left;  ///< 左操作数
    ExprNode* right; ///< 右操作数

    /**
     * @brief 构造函数
     * @param l 左操作数
     * @param r 右操作数
     * @param o 运算符
     */
    BinaryNode(ExprNode* l, ExprNode* r, std::string o)
        : ExprNode(ASTNodeType::Binary), op(std::move(o)), left(l), right(r) {
    }

    /**
     * @brief 析构函数
     */
    ~BinaryNode() override {
        delete left;
        delete right;
    }

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct FuncCallExprNode
 * @brief 函数调用表达式节点
 */
struct FuncCallExprNode final : ExprNode {
    ExprNode* func_expr;        ///< 函数表达式
    std::vector<ASTNode*> args; ///< 参数列表

    /**
     * @brief 构造函数
     * @param e 函数表达式
     * @param a 参数列表
     */
    FuncCallExprNode(ExprNode* e, std::vector<ASTNode*> a)
        : ExprNode(ASTNodeType::FuncCallExpr), func_expr(e), args(std::move(a)) {
    }

    /**
     * @brief 析构函数
     */
    ~FuncCallExprNode() override {
        delete func_expr;
        for (const auto arg : args) {
            delete arg;
        }
    }

    /**
     * @brief 获取值类别
     * @return RVALUE
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return ValueCategory::RVALUE;
    }
};

/**
 * @struct MemberAccessNode
 * @brief 成员访问表达式节点
 */
struct MemberAccessNode final : ExprNode {
    ExprNode* object;   ///< 对象表达式
    std::string member; ///< 成员名称

    /**
     * @brief 构造函数
     * @param obj 对象表达式
     * @param mem 成员名称
     */
    MemberAccessNode(ExprNode* obj, std::string mem)
        : ExprNode(ASTNodeType::MemberAccess), object(obj), member(std::move(mem)) {
    }

    /**
     * @brief 析构函数
     */
    ~MemberAccessNode() override {
        delete object;
    }

    /**
     * @brief 获取值类别
     * @return 对象的值类别
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return object->getValueCategory();
    }
};

/**
 * @struct IndexAccessNode
 * @brief 索引访问表达式节点
 */
struct IndexAccessNode final : ExprNode {
    ExprNode* object; ///< 对象表达式
    ExprNode* index;  ///< 索引表达式

    /**
     * @brief 构造函数
     * @param obj 对象表达式
     * @param idx 索引表达式
     */
    IndexAccessNode(ExprNode* obj, ExprNode* idx)
        : ExprNode(ASTNodeType::IndexAccess), object(obj), index(idx) {
    }

    /**
     * @brief 析构函数
     */
    ~IndexAccessNode() override {
        delete object;
        delete index;
    }

    /**
     * @brief 获取值类别
     * @return 对象的值类别
     */
    [[nodiscard]] ValueCategory getValueCategory() const override {
        return object->getValueCategory();
    }
};

/**
 * @struct VMCallNode
 * @brief VM 内部调用节点
 */
struct VMCallNode final : ExprNode {
    std::string index;          ///< 调用索引/名称
    std::vector<ASTNode*> args; ///< 参数列表

    /**
     * @brief 构造函数
     * @param idx 调用索引
     * @param a 参数列表
     */
    VMCallNode(std::string idx, std::vector<ASTNode*> a)
        : ExprNode(ASTNodeType::VMCall), index(std::move(idx)), args(std::move(a)) {
    }

    /**
     * @brief 析构函数
     */
    ~VMCallNode() override {
        for (const auto arg : args) {
            delete arg;
        }
    }
};

/**
 * @struct BlockStmtNode
 * @brief 块语句节点
 */
struct BlockStmtNode final : ASTNode {
    std::vector<ASTNode*> stmts; ///< 语句列表

    /**
     * @brief 构造函数
     * @param s 语句列表
     */
    explicit BlockStmtNode(std::vector<ASTNode*> s)
        : ASTNode(ASTNodeType::BlockStmt), stmts(std::move(s)) {
    }

    /**
     * @brief 析构函数
     */
    ~BlockStmtNode() override {
        for (const auto stmt : stmts) {
            delete stmt;
        }
    }
};

/**
 * @struct VarDeclNode
 * @brief 变量声明节点
 */
struct VarDeclNode final : ASTNode {
    std::string name;      ///< 变量名称
    ExprNode* init;        ///< 初始化表达式
    bool is_const;         ///< 是否为常量
    Visibility visibility; ///< 可见性

    /**
     * @brief 构造函数
     * @param n 变量名称
     * @param i 初始化表达式
     * @param is_const 是否为常量
     * @param vis 可见性
     */
    VarDeclNode(
        std::string n,
        ExprNode* i,
        bool is_const = false,
        Visibility vis = Visibility::Exported
    )
        : ASTNode(ASTNodeType::VarDecl),
          name(std::move(n)),
          init(i),
          is_const(is_const),
          visibility(vis) {
    }

    /**
     * @brief 析构函数
     */
    ~VarDeclNode() override {
        delete init;
    }
};

/**
 * @struct AssignNode
 * @brief 赋值语句节点
 */
struct AssignNode final : ASTNode {
    ExprNode* var;   ///< 目标变量表达式
    ExprNode* value; ///< 值表达式

    /**
     * @brief 构造函数
     * @param n 目标变量
     * @param v 值表达式
     */
    AssignNode(ExprNode* n, ExprNode* v)
        : ASTNode(ASTNodeType::Assign), var(n), value(v) {
    }

    /**
     * @brief 析构函数
     */
    ~AssignNode() override {
        delete value;
    }
};

/**
 * @struct DecoratedFuncNode
 * @brief 带装饰器的函数节点
 */
struct DecoratedFuncNode final : ASTNode {
    std::vector<ExprNode*> decorators; ///< 装饰器列表
    ASTNode* target;                   ///< 目标函数

    /**
     * @brief 构造函数
     * @param decs 装饰器列表
     * @param t 目标函数
     */
    DecoratedFuncNode(std::vector<ExprNode*> decs, ASTNode* t)
        : ASTNode(ASTNodeType::DecoratedFunc), decorators(std::move(decs)), target(t) {
    }

    /**
     * @brief 析构函数
     */
    ~DecoratedFuncNode() override {
        for (const auto dec : decorators) {
            delete dec;
        }
        delete target;
    }
};

/**
 * @struct FuncDeclNode
 * @brief 函数声明节点
 */
struct FuncDeclNode final : ASTNode {
    std::string name;                             ///< 函数名称
    std::vector<std::string> params;              ///< 参数列表
    BlockStmtNode* body;                          ///< 函数体
    std::vector<TypeNode*> args_type;             ///< 参数类型列表
    TypeNode* ret_type{};                         ///< 返回类型
    std::vector<ExprNode*> decos;                 ///< 装饰器列表
    Visibility visibility = Visibility::Exported; ///< 可见性

    /**
     * @brief 构造函数
     * @param n 函数名称
     * @param p 参数列表
     * @param b 函数体
     * @param v 可见性
     * @param decos 装饰器列表
     */
    FuncDeclNode(
        std::string n,
        std::vector<std::string> p,
        BlockStmtNode* b,
        Visibility v = Visibility::Exported,
        const std::vector<ExprNode*>& decos = {}
    ) : ASTNode(ASTNodeType::FuncDecl),
        name(std::move(n)),
        params(std::move(p)),
        body(b),
        decos(decos),
        visibility(v) {
    }

    /**
     * @brief 析构函数
     */
    ~FuncDeclNode() override {
        delete body;
        for (const auto type : args_type) {
            delete type;
        }
        delete ret_type;
    }
};

/**
 * @struct DoFuncDeclNode
 * @brief do 表达式形式的函数声明节点
 */
struct DoFuncDeclNode : ExprNode {
    std::vector<std::string> params;  ///< 参数列表
    BlockStmtNode* body;              ///< 函数体
    std::vector<TypeNode*> args_type; ///< 参数类型列表
    TypeNode* ret_type{};             ///< 返回类型
    std::vector<ExprNode*> decos;     ///< 装饰器列表

    /**
     * @brief 构造函数
     * @param p 参数列表
     * @param b 函数体
     * @param decos 装饰器列表
     */
    DoFuncDeclNode(
        std::vector<std::string> p,
        BlockStmtNode* b,
        const std::vector<ExprNode*>& decos = {}
    )
        : ExprNode(ASTNodeType::DoFuncDecl), params(std::move(p)), body(b), decos(decos) {
    }

    /**
     * @brief 析构函数
     */
    ~DoFuncDeclNode() override {
        delete body;
        for (const auto type : args_type) {
            delete type;
        }
        delete ret_type;
    }
};

/**
 * @struct ExternFuncNode
 * @brief 外部函数声明节点（用于调用动态库函数）
 */
struct ExternFuncNode : ASTNode {
    std::string name;                 ///< 函数名称
    std::vector<std::string> params;  ///< 参数列表
    std::vector<TypeNode*> args_type; ///< 参数类型列表
    StringNode* lib_name;             ///< 库名称
    TypeNode* ret_type;               ///< 返回类型
    std::vector<ExprNode*> decos;     ///< 装饰器列表

    /**
     * @brief 构造函数
     * @param n 函数名称
     * @param p 参数列表
     * @param a 参数类型列表
     * @param l 库名称
     * @param r 返回类型
     * @param decos 装饰器列表
     */
    ExternFuncNode(
        std::string n,
        std::vector<std::string> p,
        std::vector<TypeNode*> a,
        StringNode* l,
        TypeNode* r,
        const std::vector<ExprNode*>& decos = {}
    )
        : ASTNode(ASTNodeType::ExternFunc),
          name(std::move(n)),
          params(std::move(p)),
          args_type(std::move(a)),
          lib_name(l),
          ret_type(r),
          decos(decos) {
    }

    /**
     * @brief 析构函数
     */
    ~ExternFuncNode() override {
        delete lib_name;
        for (auto type : args_type) {
            delete type;
        }
        delete ret_type;
    }
};

/**
 * @struct ReturnStmtNode
 * @brief 返回语句节点
 */
struct ReturnStmtNode final : ASTNode {
    ExprNode* expr; ///< 返回表达式

    /**
     * @brief 构造函数
     * @param e 返回表达式
     */
    explicit ReturnStmtNode(ExprNode* e)
        : ASTNode(ASTNodeType::ReturnStmt), expr(e) {
    }

    /**
     * @brief 析构函数
     */
    ~ReturnStmtNode() override {
        delete expr;
    }
};

/**
 * @struct LoopNode
 * @brief loop 循环节点
 */
struct LoopNode final : ASTNode {
    ExprNode* condition; ///< 循环条件
    BlockStmtNode* body; ///< 循环体

    /**
     * @brief 构造函数
     * @param cond 循环条件
     * @param b 循环体
     */
    LoopNode(ExprNode* cond, BlockStmtNode* b)
        : ASTNode(ASTNodeType::Loop), condition(cond), body(b) {
    }

    /**
     * @brief 析构函数
     */
    ~LoopNode() override {
        delete condition;
        delete body;
    }
};

/**
 * @struct ForLoopNode
 * @brief for 循环节点，支持多变量迭代
 */
struct ForLoopNode final : ASTNode {
    struct IterationItem {
        std::string var_name; ///< 迭代变量名
        ExprNode* iterable;   ///< 可迭代对象表达式

        IterationItem(std::string name, ExprNode* iter)
            : var_name(std::move(name)), iterable(iter) {
        }

        ~IterationItem() { delete iterable; }
    };

    std::vector<IterationItem*> items; ///< 迭代项列表
    BlockStmtNode* body;               ///< 循环体

    /**
     * @brief 构造函数
     * @param its 迭代项列表
     * @param b 循环体
     */
    ForLoopNode(std::vector<IterationItem*> its, BlockStmtNode* b)
        : ASTNode(ASTNodeType::ForLoop), items(std::move(its)), body(b) {
    }

    /**
     * @brief 析构函数
     */
    ~ForLoopNode() override {
        for (auto item : items) {
            delete item;
        }
        delete body;
    }
};

/**
 * @struct WhileStmtNode
 * @brief while 循环节点
 */
struct WhileStmtNode final : ASTNode {
    ExprNode* condition; ///< 循环条件
    BlockStmtNode* body; ///< 循环体

    /**
     * @brief 构造函数
     * @param cond 循环条件
     * @param b 循环体
     */
    WhileStmtNode(ExprNode* cond, BlockStmtNode* b)
        : ASTNode(ASTNodeType::WhileStmt), condition(cond), body(b) {
    }

    /**
     * @brief 析构函数
     */
    ~WhileStmtNode() override {
        delete condition;
        delete body;
    }
};

/**
 * @struct BreakNode
 * @brief break 语句节点
 */
struct BreakNode final : ASTNode {
    /**
     * @brief 构造函数
     */
    BreakNode() : ASTNode(ASTNodeType::Break) {
    }

    /**
     * @brief 析构函数
     */
    ~BreakNode() override = default;
};

/**
 * @struct ContinueNode
 * @brief continue 语句节点
 */
struct ContinueNode final : ASTNode {
    /**
     * @brief 构造函数
     */
    ContinueNode() : ASTNode(ASTNodeType::Continue) {
    }

    /**
     * @brief 析构函数
     */
    ~ContinueNode() override = default;
};

/**
 * @struct IfStmtNode
 * @brief if 语句节点
 */
struct IfStmtNode final : ASTNode {
    ExprNode* condition;       ///< 条件表达式
    BlockStmtNode* then_block; ///< then 块
    BlockStmtNode* else_block; ///< else 块

    /**
     * @brief 构造函数
     * @param cond 条件表达式
     * @param t then 块
     * @param e else 块
     */
    IfStmtNode(
        ExprNode* cond,
        BlockStmtNode* t,
        BlockStmtNode* e
    )
        : ASTNode(ASTNodeType::IfStmt),
          condition(cond),
          then_block(t),
          else_block(e) {
    }

    /**
     * @brief 析构函数
     */
    ~IfStmtNode() override {
        delete condition;
        delete then_block;
        delete else_block;
    }
};

/**
 * @struct ModuleNode
 * @brief 模块节点
 */
struct ModuleNode final : ASTNode {
    /**
     * @enum Types
     * @brief 模块类型
     */
    enum class Types {
        ord, ///< 普通模块
        dyn  ///< 动态模块
    };

    std::string name;                       ///< 模块名称
    Types type;                             ///< 模块类型
    StringNode* lib;                        ///< 库名称（动态模块）
    std::vector<VarDeclNode*> vars;         ///< 变量声明列表
    std::vector<FuncDeclNode*> ord_funcs;   ///< 普通函数列表
    std::vector<ExternFuncNode*> dyn_funcs; ///< 动态函数列表
    std::vector<ModuleNode*> children;      ///< 子模块列表

    /**
     * @brief 构造函数
     * @param n 模块名称
     * @param t 模块类型
     * @param l 库名称
     * @param v 变量列表
     * @param o 普通函数列表
     * @param d 动态函数列表
     * @param c 子模块列表
     */
    ModuleNode(
        std::string n,
        const Types t,
        StringNode* l,
        std::vector<VarDeclNode*> v,
        std::vector<FuncDeclNode*> o,
        std::vector<ExternFuncNode*> d,
        std::vector<ModuleNode*> c
    )
        : ASTNode(ASTNodeType::Module),
          name(std::move(n)),
          type(t),
          lib(l),
          vars(std::move(v)),
          ord_funcs(std::move(o)),
          dyn_funcs(std::move(d)),
          children(std::move(c)) {
    }

    /**
     * @brief 析构函数
     */
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

/**
 * @struct ProgramASTNode
 * @brief 程序根节点
 */
struct ProgramASTNode final : ASTNode {
    std::vector<ASTNode*> stmts; ///< 顶层语句列表

    /**
     * @brief 构造函数
     * @param s 顶层语句列表
     */
    explicit ProgramASTNode(std::vector<ASTNode*> s)
        : ASTNode(ASTNodeType::Program), stmts(std::move(s)) {
    }

    /**
     * @brief 析构函数
     */
    ~ProgramASTNode() override {
        for (const auto stmt : stmts) {
            delete stmt;
        }
    }
};

/**
 * @struct ImportNode
 * @brief import 语句节点
 */
struct ImportNode final : ASTNode {
    std::vector<std::string> module_name; ///< 模块名称路径
    std::string alias;                    ///< 别名

    /**
     * @brief 构造函数
     * @param n 模块名称路径
     * @param a 别名
     */
    ImportNode(std::vector<std::string> n, std::string a)
        : ASTNode(ASTNodeType::Import), module_name(std::move(n)), alias(std::move(a)) {
    }

    /**
     * @brief 析构函数
     */
    ~ImportNode() override = default;
};

/**
 * @struct UseNode
 * @brief use 语句节点
 */
struct UseNode final : ASTNode {
    std::vector<std::string> module_name; ///< 模块名称路径
    std::vector<UseItem> items;           ///< 导入项列表

    /**
     * @brief 构造函数
     * @param n 模块名称路径
     * @param i 导入项列表
     */
    UseNode(std::vector<std::string> n, std::vector<UseItem> i)
        : ASTNode(ASTNodeType::Use), module_name(std::move(n)), items(std::move(i)) {
    }

    /**
     * @brief 析构函数
     */
    ~UseNode() override = default;
};

/**
 * @struct StructField
 * @brief struct 中的一个字段
 */
struct StructField {
    std::string name;
    std::string type_name;
    bool has_type_annotation = false;
    bool is_var = false;
    ExprNode* default_init = nullptr;
};

/**
 * @struct StructDeclNode
 * @brief struct 声明
 */
struct StructDeclNode final : ASTNode {
    std::string name;
    bool typed = false;
    std::vector<StructField> fields;

    StructDeclNode(std::string n, bool t, std::vector<StructField> f)
        : ASTNode(ASTNodeType::StructDecl), name(std::move(n)), typed(t), fields(std::move(f)) {
    }

    ~StructDeclNode() override {
        for (auto& field : fields) {
            delete field.default_init;
        }
    }
};
} // namespace lmx
