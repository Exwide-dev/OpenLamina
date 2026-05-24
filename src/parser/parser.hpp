#pragma once

#include <vector>
#include <string>
#include "../lexer/lexer.hpp"
#include "ast.hpp"

namespace lmx {

class Parser {
public:
    explicit Parser(Lexer& lexer);
    
    ProgramASTNode* parse();
    
private:
    Lexer& lexer;
    Token current_token;
    Token lookahead;
    
    void advance();
    void consume(TokenType expected);
    bool match(TokenType type);
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool isAtEnd() const;
    [[nodiscard]] Token peek(int n) const;
    
    ASTNode* parseProgram();
    std::vector<ASTNode*> parseStatementList();
    ASTNode* parseStatement();
    
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
    ExprNode* parseExpressionNoFunc();
    
    ExprNode* parseComparison();
    ExprNode* parseComparisonNoFunc();
    
    ExprNode* parseAdditive();
    ExprNode* parseAdditiveNoFunc();
    
    ExprNode* parseMultiplicative();
    ExprNode* parseMultiplicativeNoFunc();
    
    ExprNode* parseUnary();
    ExprNode* parseUnaryNoFunc();
    
    ExprNode* parsePostfix();
    ExprNode* parsePostfixNoFunc();
    
    ExprNode* parsePrimary();
    ExprNode* parseDoExpr();
    
    std::vector<std::string> parseParamList();
    std::vector<ASTNode*> parseArgList();
    std::vector<ASTNode*> parseBlockStatementList();
    std::vector<ASTNode*> parseVecElementList();
    std::vector<DictEntryNode*> parseDictEntryList();
    std::vector<std::string> parseQualifiedName();
    std::vector<UseItem> parseUseList();
    UseItem parseUseItem();
    ExprNode* parseDecoratorExpr();
    
    [[nodiscard]] int getOperatorPrecedence(TokenType type) const;
};

}
