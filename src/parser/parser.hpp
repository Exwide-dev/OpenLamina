#pragma once

#include <vector>
#include <string>
#include "../lexer/lexer.hpp"
#include "ast.hpp"

namespace lmx {

enum class ParserContext {
    TopLevel,
    FunctionBody,
    DoBody,
    Dict,
    Vec
};

class Parser {
public:
    explicit Parser(std::string filename = "<unknown>");
    
    void add_tokens(std::vector<Token> toks, const std::string& source = "");
    ProgramASTNode* parse();
    std::vector<ASTNode*> parse_rest();
    
    [[nodiscard]] const std::string& get_filename() const { return filename; }
    [[nodiscard]] const std::vector<std::string>& get_source_lines() const { return source_lines; }

private:
    std::string filename;
    std::vector<std::string> source_lines;
    std::vector<Token> tokens;
    std::vector<ParserContext> context_stack;
    size_t current_pos = 0;
    
    void push_context(const ParserContext ctx) { context_stack.push_back(ctx); }
    void pop_context() { if (!context_stack.empty()) context_stack.pop_back(); }
    [[nodiscard]] ParserContext current_context() const { 
        return context_stack.empty() ? ParserContext::TopLevel : context_stack.back(); 
    }
    [[nodiscard]] bool ignore_newline() const {
        ParserContext ctx = current_context();
        return ctx == ParserContext::Dict || ctx == ParserContext::Vec;
    }
    
    static std::string getTokenTypeName(TokenType type);
    
    void throw_error(const std::string& message);
    void throw_error_at(const std::string& message, const Token& token);
    
    void advance();
    void consume(TokenType expected);
    bool match(TokenType type);
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool isAtEnd() const;
    [[nodiscard]] Token peek(int n = 0) const;
    [[nodiscard]] Token current_token() const;
    
    ASTNode* parseStatement();
    std::vector<ASTNode*> parseStatementList();
    std::vector<ASTNode*> parseBlockStatementList();
    
    ASTNode* parseVarDecl();
    ASTNode* parseFuncDecl();
    ASTNode* parseReturnStmt();
    ASTNode* parseIfStmt();
    ASTNode* parseLoopStmt();
    ASTNode* parseWhileStmt();
    ASTNode* parseBreakStmt();
    ASTNode* parseContinueStmt();
    ASTNode* parseBlockStmt();
    ASTNode* parseImportStmt();
    ASTNode* parseUseStmt();
    ASTNode* parseAssignStmt();
    
    ExprNode* parseExpression();
    ExprNode* parseComparisonExpr();
    ExprNode* parseAdditiveExpr();
    ExprNode* parseMultiplicativeExpr();
    ExprNode* parseUnaryExpr();
    ExprNode* parsePostfixExpr();
    ExprNode* parseFactor();
    ExprNode* parseDoExpr();
    
    std::vector<std::string> parseParamList();
    std::vector<ASTNode*> parseArgList();
    std::vector<ASTNode*> parseVecElementList();
    std::vector<DictEntryNode*> parseDictEntryList();
    std::vector<std::string> parseQualifiedName();
    std::vector<UseItem> parseUseList();
    UseItem parseUseItem();
    std::vector<ExprNode*> parseDecorators();
    std::vector<ExprNode*> parseWithDecoratorList();
};

} // namespace lmx