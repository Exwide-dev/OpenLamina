#include "parser.hpp"
#include "../tools/debug.hpp"
#include "../../tools/error.hpp"
#include <iostream>

namespace lmx {

Parser::Parser(Lexer& lexer) 
    : lexer(lexer), current_token(TokenType::END, "", 1, 1), 
      lookahead(TokenType::END, "", 1, 1) {
    advance();
    advance();
    LOG("Parser initialized: current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    LOG("Parser initialized: lookahead type=" << static_cast<size_t>(lookahead.type) << ", value='" << lookahead.value << "'");
}

void Parser::advance() {
    current_token = lookahead;
    lookahead = lexer.nextToken();
}

void Parser::consume(TokenType expected) {
    if (check(expected)) {
        advance();
    } else {
        throw SyntaxError("Unexpected token at line " + std::to_string(current_token.line) + ", column " + std::to_string(current_token.column) + ": expected token type " + std::to_string(static_cast<int>(expected)) + ", got '" + current_token.value + "'");
    }
}

inline bool Parser::match(const TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    return !isAtEnd() && current_token.type == type;
}

bool Parser::isAtEnd() const {
    return current_token.type == TokenType::END;
}

Token Parser::peek(const int n) const {
    if (n == 1) {
        return lookahead;
    } else if (n == 2) {
        return lexer.peekNext();
    }
    return {TokenType::END, "", 0, 0};
}

int Parser::getOperatorPrecedence(const TokenType type) const {
    switch (type) {
        case TokenType::OPER_EQ:
        case TokenType::OPER_NE:
            return 1;
        case TokenType::OPER_LT:
        case TokenType::OPER_GT:
        case TokenType::OPER_LE:
        case TokenType::OPER_GE:
            return 2;
        case TokenType::OPER_PLUS:
        case TokenType::OPER_MINUS:
            return 3;
        case TokenType::OPER_MUL:
        case TokenType::OPER_DIV:
            return 4;
        default:
            return 0;
    }
}

ProgramASTNode* Parser::parse() {
    return dynamic_cast<ProgramASTNode*>(parseProgram());
}

ASTNode* Parser::parseProgram() {
    LOG("Parsing program");
    auto statements = parseStatementList();
    auto program = new ProgramASTNode(statements);
    LOG("Program parsed successfully");
    return program;
}

std::vector<ASTNode*> Parser::parseStatementList() {
    LOG("Creating stmt_list");
    std::vector<ASTNode*> statements;
    
    while (!isAtEnd() && current_token.type != TokenType::RBRACE) {
        LOG("parseStatementList: before parseStatement, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
        LOG("parseStatementList: lookahead type=" << static_cast<size_t>(lookahead.type) << ", value='" << lookahead.value << "'");
        ASTNode* stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
        
        if (!isAtEnd() && current_token.type != TokenType::RBRACE) {
            TokenType next_type = current_token.type;
            if (next_type != TokenType::KW_FUNC && 
                next_type != TokenType::KW_DO &&
                next_type != TokenType::IDENTIFIER &&
                next_type != TokenType::NEWLINE) {
                throw SyntaxError("Expected newline after statement at line " + std::to_string(current_token.line) + ", column " + std::to_string(current_token.column));
            }
            match(TokenType::NEWLINE);
        }
    }
    
    return statements;
}

ASTNode* Parser::parseStatement() {
    LOG("Parsing stmt");
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in statement");
    }
    
    if (match(TokenType::KW_LET) || match(TokenType::KW_CONST) || 
        match(TokenType::KW_INTERN) || match(TokenType::KW_EXPORT)) {
        return parseVarDecl();
    }
    if (check(TokenType::IDENTIFIER) && lookahead.type == TokenType::ASSIGN) {
        return parseAssignStmt();
    }
    if (match(TokenType::KW_FUNC)) {
        return parseFuncDecl();
    }
    
    if (check(TokenType::KW_INTERN) || check(TokenType::KW_EXPORT)) {
        Visibility vis = (current_token.type == TokenType::KW_INTERN) ? Visibility::Internal : Visibility::Exported;
        advance();
        if (match(TokenType::KW_FUNC)) {
            return parseFuncDecl();
        }
        // 带装饰器的函数声明
        if (lookahead.type == TokenType::KW_FUNC) {
            std::vector<ExprNode*> decorators;
            while (current_token.type != TokenType::KW_FUNC) {
                decorators.push_back(parseDecoratorExpr());
            }
            advance();
            std::string name = current_token.value;
            advance();
            consume(TokenType::LPAREN);
            auto params = parseParamList();
            consume(TokenType::RPAREN);
            consume(TokenType::LBRACE);
            auto body = parseBlockStatementList();
            consume(TokenType::RBRACE);
            
            return new FuncDeclNode(name, params, new BlockStmtNode(body), vis, decorators);
        }
    }
    if (match(TokenType::KW_RETURN)) {
        return parseReturnStmt();
    }
    if (match(TokenType::KW_IF)) {
        return parseIfStmt();
    }
    if (match(TokenType::KW_LOOP)) {
        return parseLoopStmt();
    }
    if (match(TokenType::KW_WHILE)) {
        return parseWhileStmt();
    }
    if (match(TokenType::KW_BREAK)) {
        return parseBreakStmt();
    }
    if (match(TokenType::KW_CONTINUE)) {
        return parseContinueStmt();
    }
    if (match(TokenType::KW_DO)) {
        return parseDoExpr();
    }
    if (check(TokenType::LBRACE)) {
        TokenType next = lookahead.type;
        if (next == TokenType::RBRACE || 
            next == TokenType::NUM_LITERAL || 
            next == TokenType::STRING_LITERAL || 
            next == TokenType::IDENTIFIER ||
            (next == TokenType::NEWLINE && 
             (peek(2).type == TokenType::IDENTIFIER || 
              peek(2).type == TokenType::NUM_LITERAL || 
              peek(2).type == TokenType::STRING_LITERAL ||
              peek(2).type == TokenType::OPER_COLON))) {
            advance();
            auto entries = parseDictEntryList();
            consume(TokenType::RBRACE);
            return new DictionaryNode(entries);
        } else {
            advance();
            auto statements = parseBlockStatementList();
            consume(TokenType::RBRACE);
            return new BlockStmtNode(statements);
        }
    }
    if (match(TokenType::KW_IMPORT)) {
        return parseImportStmt();
    }
    if (match(TokenType::KW_USE)) {
        return parseUseStmt();
    }
    
    if (check(TokenType::IDENTIFIER) || check(TokenType::NUM_LITERAL) || 
        check(TokenType::STRING_LITERAL) || check(TokenType::LPAREN) ||
        check(TokenType::LBRACKET)) {
        
        if (lookahead.type == TokenType::KW_FUNC) {
            std::vector<ExprNode*> decorators;
            while (current_token.type != TokenType::KW_FUNC) {
                decorators.push_back(parseDecoratorExpr());
            }
            advance();
            std::string name = current_token.value;
            advance();
            consume(TokenType::LPAREN);
            auto params = parseParamList();
            consume(TokenType::RPAREN);
            consume(TokenType::LBRACE);
            auto body = parseBlockStatementList();
            consume(TokenType::RBRACE);
            
            return new FuncDeclNode(name, params, new BlockStmtNode(body), Visibility::Exported, decorators);
        }
        
        if (lookahead.type == TokenType::KW_DO) {
            std::vector<ExprNode*> decorators;
            while (current_token.type != TokenType::KW_DO) {
                decorators.push_back(parseDecoratorExpr());
            }
            advance();
            consume(TokenType::LPAREN);
            auto params = parseParamList();
            consume(TokenType::RPAREN);
            consume(TokenType::LBRACE);
            auto body = parseBlockStatementList();
            consume(TokenType::RBRACE);
            
            return new DoFuncDeclNode(params, new BlockStmtNode(body), decorators);
        }
        
        return parseExpressionNoFunc();
    }

    LOG("Nothing mached, tokentype: " << static_cast<size_t>(current_token.type));
    throw SyntaxError("Unexpected token: " + std::string(current_token.value));
}

ASTNode* Parser::parseVarDecl() {
    LOG("Parsing var_decl");
    bool is_const = false;
    Visibility visibility = Visibility::Exported;
    
    if (match(TokenType::KW_INTERN)) {
        visibility = Visibility::Internal;
        if (match(TokenType::KW_CONST)) {
            is_const = true;
        }
    } else if (match(TokenType::KW_EXPORT)) {
        visibility = Visibility::Exported;
        if (match(TokenType::KW_CONST)) {
            is_const = true;
        }
    } else if (match(TokenType::KW_CONST)) {
        is_const = true;
    } else if (match(TokenType::KW_LET)) {
        // let 声明，默认 Exported，非 const
    }
    
    std::string name = current_token.value;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::ASSIGN);
    ExprNode* value = parseExpression();
    
    return new VarDeclNode(name, value, is_const, visibility);
}

ASTNode* Parser::parseFuncDecl() {
    LOG("Parsing func_decl");
    std::string name = current_token.value;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    auto body = parseBlockStatementList();
    consume(TokenType::RBRACE);
    
    return new FuncDeclNode(name, params, new BlockStmtNode(body), Visibility::Exported);
}

ASTNode* Parser::parseReturnStmt() {
    LOG("Parsing return_stmt");
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in return stmt");
    }
    
    ExprNode* value = parseExpression();
    return new ReturnStmtNode(value);
}

ASTNode* Parser::parseIfStmt() {
    LOG("Parsing if_stmt");
    consume(TokenType::LPAREN);
    ExprNode* condition = parseExpression();
    consume(TokenType::RPAREN);
    auto* then_branch = dynamic_cast<BlockStmtNode*>(parseStatement());
    if (!then_branch) {
        then_branch = new BlockStmtNode({parseStatement()});
    }
    
    BlockStmtNode* else_branch = nullptr;
    if (match(TokenType::KW_ELSE)) {
        else_branch = dynamic_cast<BlockStmtNode*>(parseStatement());
        if (!else_branch) {
            else_branch = new BlockStmtNode({parseStatement()});
        }
    }
    
    return new IfStmtNode(condition, then_branch, else_branch);
}

ASTNode* Parser::parseLoopStmt() {
    LOG("Parsing loop_stmt");
    consume(TokenType::LBRACE);
    auto body = parseBlockStatementList();
    consume(TokenType::RBRACE);
    
    return new LoopNode(nullptr, new BlockStmtNode(body));
}

ASTNode* Parser::parseWhileStmt() {
    LOG("Parsing while_stmt");
    consume(TokenType::LPAREN);
    ExprNode* condition = parseExpression();
    consume(TokenType::RPAREN);
    auto* body = dynamic_cast<BlockStmtNode*>(parseStatement());
    if (!body) {
        body = new BlockStmtNode({parseStatement()});
    }
    
    return new WhileStmtNode(condition, body);
}

ASTNode* Parser::parseBreakStmt() {
    LOG("Parsing break_stmt");
    return new BreakNode();
}

ASTNode* Parser::parseContinueStmt() {
    LOG("Parsing continue_stmt");
    return new ContinueNode();
}

ASTNode* Parser::parseBlockStmt() {
    LOG("Parsing block_stmt");
    auto statements = parseBlockStatementList();
    consume(TokenType::RBRACE);
    
    return new BlockStmtNode(statements);
}

ASTNode* Parser::parseImportStmt() {
    LOG("Parsing import_stmt");
    auto name = parseQualifiedName();
    std::string alias;
    
    if (match(TokenType::KW_AS)) {
        alias = current_token.value;
        consume(TokenType::IDENTIFIER);
    }
    
    return new ImportNode(name, alias);
}

ASTNode* Parser::parseUseStmt() {
    LOG("Parsing use_stmt");
    auto items = parseUseList();
    std::vector<std::string> module;
    
    if (match(TokenType::KW_AS)) {
        module.push_back(current_token.value);
        consume(TokenType::IDENTIFIER);
    }
    
    return new UseNode(module, items);
}

ASTNode* Parser::parseAssignStmt() {
    LOG("Parsing assign_stmt");
    ExprNode* left = parsePostfix();
    consume(TokenType::ASSIGN);
    ExprNode* right = parseExpression();
    
    return new AssignNode(left, right);
}

ExprNode* Parser::parseExpression() {
    return parseComparison();
}

ExprNode* Parser::parseExpressionNoFunc() {
    return parseComparisonNoFunc();
}

ExprNode* Parser::parseComparison() {
    ExprNode* left = parseAdditive();
    
    while (true) {
        TokenType op_type = current_token.type;
        if (op_type != TokenType::OPER_EQ && op_type != TokenType::OPER_NE &&
            op_type != TokenType::OPER_LT && op_type != TokenType::OPER_GT &&
            op_type != TokenType::OPER_LE && op_type != TokenType::OPER_GE) {
            break;
        }
        
        std::string op_str;
        switch (op_type) {
            case TokenType::OPER_EQ: op_str = "=="; break;
            case TokenType::OPER_NE: op_str = "!="; break;
            case TokenType::OPER_LT: op_str = "<"; break;
            case TokenType::OPER_GT: op_str = ">"; break;
            case TokenType::OPER_LE: op_str = "<="; break;
            case TokenType::OPER_GE: op_str = ">="; break;
            default: op_str = "";
        }
        
        advance();
        ExprNode* right = parseAdditive();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseComparisonNoFunc() {
    ExprNode* left = parseAdditiveNoFunc();
    
    while (true) {
        TokenType op_type = current_token.type;
        if (op_type != TokenType::OPER_EQ && op_type != TokenType::OPER_NE &&
            op_type != TokenType::OPER_LT && op_type != TokenType::OPER_GT &&
            op_type != TokenType::OPER_LE && op_type != TokenType::OPER_GE) {
            break;
        }
        
        std::string op_str;
        switch (op_type) {
            case TokenType::OPER_EQ: op_str = "=="; break;
            case TokenType::OPER_NE: op_str = "!="; break;
            case TokenType::OPER_LT: op_str = "<"; break;
            case TokenType::OPER_GT: op_str = ">"; break;
            case TokenType::OPER_LE: op_str = "<="; break;
            case TokenType::OPER_GE: op_str = ">="; break;
            default: op_str = "";
        }
        
        advance();
        ExprNode* right = parseAdditiveNoFunc();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseAdditive() {
    ExprNode* left = parseMultiplicative();
    
    while (true) {
        TokenType op_type = current_token.type;
        if (op_type != TokenType::OPER_PLUS && op_type != TokenType::OPER_MINUS) {
            break;
        }
        
        std::string op_str = (op_type == TokenType::OPER_PLUS) ? "+" : "-";
        advance();
        ExprNode* right = parseMultiplicative();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseAdditiveNoFunc() {
    ExprNode* left = parseMultiplicativeNoFunc();
    
    while (true) {
        TokenType op_type = current_token.type;
        if (op_type != TokenType::OPER_PLUS && op_type != TokenType::OPER_MINUS) {
            break;
        }
        
        std::string op_str = (op_type == TokenType::OPER_PLUS) ? "+" : "-";
        advance();
        ExprNode* right = parseMultiplicativeNoFunc();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseMultiplicative() {
    ExprNode* left = parseUnary();
    
    while (true) {
        TokenType op_type = current_token.type;
        if (op_type != TokenType::OPER_MUL && op_type != TokenType::OPER_DIV) {
            break;
        }
        
        std::string op_str = (op_type == TokenType::OPER_MUL) ? "*" : "/";
        advance();
        ExprNode* right = parseUnary();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseMultiplicativeNoFunc() {
    ExprNode* left = parseUnaryNoFunc();
    
    while (true) {
        TokenType op_type = current_token.type;
        if (op_type != TokenType::OPER_MUL && op_type != TokenType::OPER_DIV) {
            break;
        }
        
        std::string op_str = (op_type == TokenType::OPER_MUL) ? "*" : "/";
        advance();
        ExprNode* right = parseUnaryNoFunc();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseUnary() {
    if (match(TokenType::OPER_NOT)) {
        ExprNode* operand = parseUnary();
        return new UnaryNode("!", operand);
    }
    
    if (match(TokenType::OPER_MINUS)) {
        ExprNode* operand = parseUnary();
        return new UnaryNode("-", operand);
    }
    
    return parsePostfix();
}

ExprNode* Parser::parseUnaryNoFunc() {
    if (match(TokenType::OPER_NOT)) {
        ExprNode* operand = parseUnaryNoFunc();
        return new UnaryNode("!", operand);
    }
    
    if (match(TokenType::OPER_MINUS)) {
        ExprNode* operand = parseUnaryNoFunc();
        return new UnaryNode("-", operand);
    }
    
    return parsePostfixNoFunc();
}

ExprNode* Parser::parsePostfix() {
    ExprNode* expr = parsePrimary();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            auto args = parseArgList();
            consume(TokenType::RPAREN);
            expr = new FuncCallExprNode(expr, args);
        } else if (match(TokenType::OPER_DOT)) {
            std::string name = current_token.value;
            consume(TokenType::IDENTIFIER);
            expr = new MemberAccessNode(expr, name);
        } else if (match(TokenType::LBRACKET)) {
            ExprNode* index = parseExpression();
            consume(TokenType::RBRACKET);
            expr = new IndexAccessNode(expr, index);
        } else {
            break;
        }
    }
    
    return expr;
}

ExprNode* Parser::parsePostfixNoFunc() {
    ExprNode* expr = parsePrimary();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            auto args = parseArgList();
            consume(TokenType::RPAREN);
            expr = new FuncCallExprNode(expr, args);
        } else if (match(TokenType::OPER_DOT)) {
            std::string name = current_token.value;
            consume(TokenType::IDENTIFIER);
            expr = new MemberAccessNode(expr, name);
        } else if (match(TokenType::LBRACKET)) {
            ExprNode* index = parseExpressionNoFunc();
            consume(TokenType::RBRACKET);
            expr = new IndexAccessNode(expr, index);
        } else {
            break;
        }
    }
    
    return expr;
}

ExprNode* Parser::parsePrimary() {
    if (check(TokenType::NUM_LITERAL)) {
        std::string value = current_token.value;
        advance();
        return new NumberNode(value);
    }
    
    if (check(TokenType::STRING_LITERAL)) {
        std::string value = current_token.value;
        advance();
        return new StringNode(value);
    }
    
    if (check(TokenType::IDENTIFIER) && lookahead.type == TokenType::KW_DO) {
        ExprNode* decorator = parseDecoratorExpr();
        advance();
        consume(TokenType::LPAREN);
        auto params = parseParamList();
        consume(TokenType::RPAREN);
        consume(TokenType::LBRACE);
        auto body = parseBlockStatementList();
        consume(TokenType::RBRACE);
        
        std::vector<ExprNode*> decors;
        decors.push_back(decorator);
        return new DoFuncDeclNode(params, new BlockStmtNode(body), decors);
    }
    
    if (check(TokenType::IDENTIFIER)) {
        std::string value = current_token.value;
        advance();
        return new VarRefNode(value);
    }
    
    if (match(TokenType::LPAREN)) {
        ExprNode* expr = parseExpression();
        consume(TokenType::RPAREN);
        return expr;
    }
    
    if (match(TokenType::LBRACKET)) {
        auto elements = parseVecElementList();
        consume(TokenType::RBRACKET);
        return new VectorNode(elements);
    }
    
    if (match(TokenType::LBRACE)) {
        auto entries = parseDictEntryList();
        consume(TokenType::RBRACE);
        return new DictionaryNode(entries);
    }
    
    if (match(TokenType::KW_DO)) {
        consume(TokenType::LPAREN);
        auto params = parseParamList();
        consume(TokenType::RPAREN);
        consume(TokenType::LBRACE);
        auto body = parseBlockStatementList();
        consume(TokenType::RBRACE);
        
        return new DoFuncDeclNode(params, new BlockStmtNode(body));
    }
    
    return nullptr;
}

ExprNode* Parser::parseDoExpr() {
    LOG("Parsing do_expr");
    
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    auto body = parseBlockStatementList();
    consume(TokenType::RBRACE);
    
    return new DoFuncDeclNode(params, new BlockStmtNode(body));
}

ExprNode* Parser::parseDecoratorExpr() {
    LOG("Parsing decorator expr");
    
    ExprNode* expr = nullptr;
    
    if (check(TokenType::IDENTIFIER)) {
        std::string name = current_token.value;
        advance();
        expr = new VarRefNode(name);
        
        while (check(TokenType::OPER_DOT)) {
            advance();
            std::string member = current_token.value;
            advance();
            expr = new MemberAccessNode(expr, member);
        }
        
        if (check(TokenType::LPAREN)) {
            advance();
            auto args = parseArgList();
            advance();
            expr = new FuncCallExprNode(expr, args);
        }
    } else if (check(TokenType::LPAREN)) {
        advance();
        expr = parseExpressionNoFunc();
        advance();
    }
    
    return expr;
}

std::vector<std::string> Parser::parseParamList() {
    std::vector<std::string> params;
    
    if (check(TokenType::RPAREN)) {
        return params;
    }
    
    params.push_back(current_token.value);
    consume(TokenType::IDENTIFIER);
    
    while (match(TokenType::OPER_COMMA)) {
        params.push_back(current_token.value);
        consume(TokenType::IDENTIFIER);
    }
    
    return params;
}

std::vector<ASTNode*> Parser::parseArgList() {
    std::vector<ASTNode*> args;
    
    if (check(TokenType::RPAREN)) {
        return args;
    }
    
    args.push_back(parseExpression());
    
    while (match(TokenType::OPER_COMMA)) {
        args.push_back(parseExpression());
    }
    
    return args;
}

std::vector<ASTNode*> Parser::parseBlockStatementList() {
    std::vector<ASTNode*> statements;
    
    while (!isAtEnd() && current_token.type != TokenType::RBRACE) {
        ASTNode* stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
        
        if (!isAtEnd() && current_token.type != TokenType::RBRACE) {
            TokenType next_type = current_token.type;
            if (next_type != TokenType::KW_FUNC && 
                next_type != TokenType::KW_DO &&
                next_type != TokenType::IDENTIFIER &&
                next_type != TokenType::NEWLINE) {
                throw SyntaxError("Expected newline after statement at line " + std::to_string(current_token.line) + ", column " + std::to_string(current_token.column));
            }
            match(TokenType::NEWLINE);
        }
    }
    
    return statements;
}

std::vector<ASTNode*> Parser::parseVecElementList() {
    std::vector<ASTNode*> elements;
    
    LOG("parseVecElementList: START");
    LOG("parseVecElementList: current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "', line=" << current_token.line << ", col=" << current_token.column);
    
    if (check(TokenType::RBRACKET)) {
        LOG("parseVecElementList: empty vec detected");
        return elements;
    }
    
    LOG("parseVecElementList: before first newline skip");
    while (match(TokenType::NEWLINE)) {
        LOG("parseVecElementList: Skipping NEWLINE in vec (start), current_token now type=" << static_cast<size_t>(current_token.type));
    }
    LOG("parseVecElementList: after first newline skip, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    
    LOG("parseVecElementList: calling parseExpression() for first element");
    elements.push_back(parseExpression());
    LOG("parseVecElementList: after parseExpression(), current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    
    while (match(TokenType::OPER_COMMA)) {
        LOG("parseVecElementList: found comma, processing next element");
        while (match(TokenType::NEWLINE)) {
            LOG("parseVecElementList: Skipping NEWLINE in vec (after comma), current_token now type=" << static_cast<size_t>(current_token.type));
        }
        LOG("parseVecElementList: before parseExpression(), current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
        
        elements.push_back(parseExpression());
        LOG("parseVecElementList: after parseExpression(), current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    }
    
    LOG("parseVecElementList: before final newline skip");
    while (match(TokenType::NEWLINE)) {
        LOG("parseVecElementList: Skipping NEWLINE in vec (end), current_token now type=" << static_cast<size_t>(current_token.type));
    }
    
    LOG("parseVecElementList: END, returning " << elements.size() << " elements");
    return elements;
}

std::vector<DictEntryNode*> Parser::parseDictEntryList() {
    std::vector<DictEntryNode*> entries;
    
    LOG("parseDictEntryList: START");
    LOG("parseDictEntryList: current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "', line=" << current_token.line << ", col=" << current_token.column);
    LOG("parseDictEntryList: lookahead type=" << static_cast<size_t>(lookahead.type) << ", value='" << lookahead.value << "', line=" << lookahead.line << ", col=" << lookahead.column);
    
    if (check(TokenType::RBRACE)) {
        LOG("parseDictEntryList: empty dict detected");
        return entries;
    }
    
    LOG("parseDictEntryList: before first newline skip");
    while (match(TokenType::NEWLINE)) {
        LOG("parseDictEntryList: Skipping NEWLINE in dict (start), current_token now type=" << static_cast<size_t>(current_token.type));
    }
    LOG("parseDictEntryList: after first newline skip, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    
    LOG("parseDictEntryList: calling parseExpression() for key");
    ExprNode* key = parseExpression();
    LOG("parseDictEntryList: after parseExpression() for key, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    LOG("parseDictEntryList: lookahead type=" << static_cast<size_t>(lookahead.type) << ", value='" << lookahead.value << "'");
    
    LOG("parseDictEntryList: before second newline skip");
    while (match(TokenType::NEWLINE)) {
        LOG("parseDictEntryList: Skipping NEWLINE in dict (after key), current_token now type=" << static_cast<size_t>(current_token.type));
    }
    LOG("parseDictEntryList: after second newline skip, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    
    LOG("parseDictEntryList: checking for colon, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    if (current_token.type == TokenType::OPER_COLON) {
        LOG("parseDictEntryList: found colon, advancing");
        advance();
        LOG("parseDictEntryList: after advancing past colon, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    } else {
        LOG("parseDictEntryList: ERROR - expected colon but got type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
        throw SyntaxError("Expected ':' in dictionary entry at line " + std::to_string(current_token.line) + ", column " + std::to_string(current_token.column));
    }
    
    LOG("parseDictEntryList: before third newline skip");
    while (match(TokenType::NEWLINE)) {
        LOG("parseDictEntryList: Skipping NEWLINE in dict (after colon), current_token now type=" << static_cast<size_t>(current_token.type));
    }
    LOG("parseDictEntryList: after third newline skip, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    
    LOG("parseDictEntryList: calling parseExpression() for value");
    ExprNode* value = parseExpression();
    LOG("parseDictEntryList: after parseExpression() for value, current_token type=" << static_cast<size_t>(current_token.type) << ", value='" << current_token.value << "'");
    entries.push_back(new DictEntryNode(key, value));
    
    while (match(TokenType::OPER_COMMA)) {
        LOG("parseDictEntryList: found comma, processing next entry");
        while (match(TokenType::NEWLINE)) {
            LOG("parseDictEntryList: Skipping NEWLINE in dict (after comma)");
        }
        
        key = parseExpression();
        
        while (match(TokenType::NEWLINE)) {
            LOG("parseDictEntryList: Skipping NEWLINE in dict (after key in loop)");
        }
        
        LOG("parseDictEntryList: consuming colon in loop");
        consume(TokenType::OPER_COLON);
        
        while (match(TokenType::NEWLINE)) {
            LOG("parseDictEntryList: Skipping NEWLINE in dict (after colon in loop)");
        }
        
        value = parseExpression();
        entries.push_back(new DictEntryNode(key, value));
    }
    
    while (match(TokenType::NEWLINE)) {
        LOG("parseDictEntryList: Skipping NEWLINE in dict (end)");
    }
    
    LOG("parseDictEntryList: END, returning " << entries.size() << " entries");
    return entries;
}

std::vector<std::string> Parser::parseQualifiedName() {
    std::vector<std::string> parts;
    
    parts.push_back(current_token.value);
    consume(TokenType::IDENTIFIER);
    
    while (match(TokenType::OPER_DOT)) {
        parts.push_back(current_token.value);
        consume(TokenType::IDENTIFIER);
    }
    
    return parts;
}

std::vector<UseItem> Parser::parseUseList() {
    std::vector<UseItem> items;
    
    items.push_back(parseUseItem());
    
    while (match(TokenType::OPER_COMMA)) {
        items.push_back(parseUseItem());
    }
    
    return items;
}

UseItem Parser::parseUseItem() {
    const std::string name = current_token.value;
    consume(TokenType::IDENTIFIER);
    
    std::string alias;
    if (match(TokenType::KW_AS)) {
        alias = current_token.value;
        consume(TokenType::IDENTIFIER);
    }
    
    return {name, alias};
}

} // namespace lmx