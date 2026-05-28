#pragma once

#include <vector>
#include <string>
#include <utility>
#include "lexer.hpp"
#include "ast.hpp"

namespace lmx {

class ParserError;

struct ParseError {
    std::string message;
    std::string filename;
    int line;
    int column;
    std::string context_line;
    size_t error_pos;
    
    ParseError(std::string msg, std::string fname, int ln, int col, 
               std::string ctx, size_t pos)
        : message(std::move(msg)), filename(std::move(fname)), 
          line(ln), column(col), context_line(std::move(ctx)), error_pos(pos) {}
};

class Parser {
public:
    explicit Parser(std::string fname = "");
    
    void add_tokens(std::vector<Token> tokens);
    ProgramASTNode* parse();
    
    [[nodiscard]] const std::string& get_filename() const { return filename; }
    [[nodiscard]] const std::vector<std::string>& get_source_lines() const { return source_lines; }
    [[nodiscard]] bool has_error() const { return error_occurred; }
    [[nodiscard]] const ParseError& get_error() const { return last_error; }

private:
    std::string filename;
    std::vector<std::string> source_lines;
    std::vector<Token> tokens;
    size_t current_pos = 0;
    bool error_occurred = false;
    ParseError last_error;
    
    static std::string getTokenTypeName(TokenType type);
    
    void add_error(const std::string& message);
    void add_error_at(const std::string& message, const Token& token);
    [[nodiscard]] std::string format_error(const ParseError& err) const;
    
    void advance();
    void consume(TokenType expected);
    bool match(TokenType type);
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool isAtEnd() const;
    [[nodiscard]] Token peek(int n = 0) const;
    [[nodiscard]] Token current_token() const;
    
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

} // namespace lmx
