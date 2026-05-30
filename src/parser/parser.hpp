#pragma once

#include <vector>
#include <string>
#include "../lexer/lexer.hpp"
#include "ast.hpp"

namespace lmx {

/**
 * @enum ParserContext
 * @brief 解析器上下文类型
 */
enum class ParserContext {
    TopLevel,     ///< 顶层上下文
    FunctionBody, ///< 函数体上下文
    DoBody,       ///< do 表达式体上下文
    Dict,         ///< 字典上下文
    Vec           ///< 向量上下文
};

/**
 * @class Parser
 * @brief 语法分析器类，将 token 序列转换为 AST
 */
class Parser {
public:
    /**
     * @brief 构造函数
     * @param filename 源文件名，用于错误报告
     */
    explicit Parser(std::string filename = "<unknown>");
    
    /**
     * @brief 添加 token 序列
     * @param toks token 向量
     * @param source 源代码字符串
     */
    void add_tokens(std::vector<Token> toks, const std::string& source = "");
    
    /**
     * @brief 开始解析，生成程序 AST
     * @return 程序根节点
     */
    ProgramASTNode* parse();
    
    /**
     * @brief 继续解析剩余 token
     * @return AST 节点列表
     */
    std::vector<ASTNode*> parse_rest();
    
    /**
     * @brief 获取源文件名
     * @return 文件名
     */
    [[nodiscard]] const std::string& get_filename() const { return filename; }
    
    /**
     * @brief 获取源代码行列表
     * @return 源代码行向量
     */
    [[nodiscard]] const std::vector<std::string>& get_source_lines() const { return source_lines; }

private:
    std::string filename;           ///< 源文件名
    std::vector<std::string> source_lines; ///< 源代码行列表
    std::vector<Token> tokens;      ///< token 列表
    std::vector<ParserContext> context_stack; ///< 上下文栈
    size_t current_pos = 0;         ///< 当前解析位置
    
    /**
     * @brief 压入上下文
     * @param ctx 上下文类型
     */
    void push_context(const ParserContext ctx) { context_stack.push_back(ctx); }
    
    /**
     * @brief 弹出上下文
     */
    void pop_context() { if (!context_stack.empty()) context_stack.pop_back(); }
    
    /**
     * @brief 获取当前上下文
     * @return 当前上下文类型
     */
    [[nodiscard]] ParserContext current_context() const { 
        return context_stack.empty() ? ParserContext::TopLevel : context_stack.back(); 
    }
    
    /**
     * @brief 判断是否忽略换行符
     * @return 如果忽略换行返回 true
     */
    [[nodiscard]] bool ignore_newline() const {
        ParserContext ctx = current_context();
        return ctx == ParserContext::Dict || ctx == ParserContext::Vec;
    }
    
    /**
     * @brief 获取 token 类型名称
     * @param type token 类型
     * @return 类型名称字符串
     */
    static std::string getTokenTypeName(TokenType type);
    
    /**
     * @brief 抛出解析错误
     * @param message 错误信息
     */
    void throw_error(const std::string& message);
    
    /**
     * @brief 在指定 token 位置抛出解析错误
     * @param message 错误信息
     * @param token 出错的 token
     */
    void throw_error_at(const std::string& message, const Token& token);
    
    /**
     * @brief 前进到下一个 token
     */
    void advance();
    
    /**
     * @brief 消费指定类型的 token
     * @param expected 期望的 token 类型
     */
    void consume(TokenType expected);
    
    /**
     * @brief 尝试匹配指定类型的 token
     * @param type 期望的 token 类型
     * @return 如果匹配成功返回 true
     */
    bool match(TokenType type);
    
    /**
     * @brief 检查当前 token 是否为指定类型（不消费）
     * @param type 要检查的 token 类型
     * @return 如果匹配返回 true
     */
    [[nodiscard]] bool check(TokenType type) const;
    
    /**
     * @brief 检查是否到达 token 序列末尾
     * @return 如果到达末尾返回 true
     */
    [[nodiscard]] bool isAtEnd() const;
    
    /**
     * @brief 查看指定位置的 token（不消费）
     * @param n 偏移量，默认为 0（当前 token）
     * @return 指定位置的 token
     */
    [[nodiscard]] Token peek(int n = 0) const;
    
    /**
     * @brief 获取当前 token
     * @return 当前 token
     */
    [[nodiscard]] Token current_token() const;
    
    /**
     * @brief 解析语句
     * @return 语句节点
     */
    ASTNode* parseStatement();
    
    /**
     * @brief 解析语句列表
     * @return 语句节点列表
     */
    std::vector<ASTNode*> parseStatementList();
    
    /**
     * @brief 解析块语句列表
     * @return 语句节点列表
     */
    std::vector<ASTNode*> parseBlockStatementList();
    
    /**
     * @brief 解析变量声明
     * @return 变量声明节点
     */
    ASTNode* parseVarDecl();
    
    /**
     * @brief 解析函数声明
     * @return 函数声明节点
     */
    ASTNode* parseFuncDecl();
    
    /**
     * @brief 解析返回语句
     * @return 返回语句节点
     */
    ASTNode* parseReturnStmt();
    
    /**
     * @brief 解析 if 语句
     * @return if 语句节点
     */
    ASTNode* parseIfStmt();
    
    /**
     * @brief 解析 loop 循环语句
     * @return loop 语句节点
     */
    ASTNode* parseLoopStmt();
    
    /**
     * @brief 解析 while 循环语句
     * @return while 语句节点
     */
    ASTNode* parseWhileStmt();
    
    /**
     * @brief 解析 break 语句
     * @return break 语句节点
     */
    ASTNode* parseBreakStmt();
    
    /**
     * @brief 解析 continue 语句
     * @return continue 语句节点
     */
    ASTNode* parseContinueStmt();
    
    /**
     * @brief 解析块语句
     * @return 块语句节点
     */
    ASTNode* parseBlockStmt();
    
    /**
     * @brief 解析 import 语句
     * @return import 语句节点
     */
    ASTNode* parseImportStmt();
    
    /**
     * @brief 解析 use 语句
     * @return use 语句节点
     */
    ASTNode* parseUseStmt();
    
    /**
     * @brief 解析赋值语句
     * @return 赋值语句节点
     */
    ASTNode* parseAssignStmt();
    
    /**
     * @brief 解析表达式
     * @return 表达式节点
     */
    ExprNode* parseExpression();
    
    /**
     * @brief 解析比较表达式
     * @return 表达式节点
     */
    ExprNode* parseComparisonExpr();
    
    /**
     * @brief 解析加法表达式
     * @return 表达式节点
     */
    ExprNode* parseAdditiveExpr();
    
    /**
     * @brief 解析乘法表达式
     * @return 表达式节点
     */
    ExprNode* parseMultiplicativeExpr();
    
    /**
     * @brief 解析一元表达式
     * @return 表达式节点
     */
    ExprNode* parseUnaryExpr();
    
    /**
     * @brief 解析后缀表达式
     * @return 表达式节点
     */
    ExprNode* parsePostfixExpr();
    
    /**
     * @brief 解析因子
     * @return 表达式节点
     */
    ExprNode* parseFactor();
    
    /**
     * @brief 解析 do 表达式
     * @return 表达式节点
     */
    ExprNode* parseDoExpr();
    
    /**
     * @brief 解析参数列表
     * @return 参数名称列表
     */
    std::vector<std::string> parseParamList();
    
    /**
     * @brief 解析参数列表
     * @return 参数表达式列表
     */
    std::vector<ASTNode*> parseArgList();
    
    /**
     * @brief 解析向量元素列表
     * @return 元素表达式列表
     */
    std::vector<ASTNode*> parseVecElementList();
    
    /**
     * @brief 解析字典条目列表
     * @return 字典条目节点列表
     */
    std::vector<DictEntryNode*> parseDictEntryList();
    
    /**
     * @brief 解析限定名称
     * @return 名称组件列表
     */
    std::vector<std::string> parseQualifiedName();
    
    /**
     * @brief 解析 use 项列表
     * @return use 项列表
     */
    std::vector<UseItem> parseUseList();
    
    /**
     * @brief 解析单个 use 项
     * @return use 项
     */
    UseItem parseUseItem();
    
    /**
     * @brief 解析装饰器列表
     * @return 装饰器表达式列表
     */
    std::vector<ExprNode*> parseDecorators();
    
    /**
     * @brief 解析 with 装饰器列表
     * @return 装饰器表达式列表
     */
    std::vector<ExprNode*> parseWithDecoratorList();
};

} // namespace lmx