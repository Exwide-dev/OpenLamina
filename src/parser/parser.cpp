#include "parser.hpp"
#include "../../tools/debug.hpp"
#include "../../tools/error.hpp"
#include <iomanip>
#include <iostream>

namespace lmx {

Parser::Parser(std::string filename)
    : filename(std::move(filename)) {}

void Parser::add_tokens(std::vector<Token> toks, const std::string& source) {
    tokens.insert(tokens.end(), toks.begin(), toks.end());
    
    if (!source.empty()) {
        std::istringstream iss(source);
        std::string line_str;
        while (std::getline(iss, line_str)) {
            source_lines.push_back(line_str);
        }
    } else {
        int max_line = 0;
        for (const auto& tok : toks) {
            if (tok.line > max_line) {
                max_line = tok.line;
            }
        }
        
        if (max_line > static_cast<int>(source_lines.size())) {
            source_lines.resize(max_line);
        }
    }
}

std::string Parser::getTokenTypeName(const TokenType type) {
    switch (type) {
        case TokenType::END: return "END";
        case TokenType::IDENTIFIER: return "identifier";
        case TokenType::NUM_LITERAL: return "number";
        case TokenType::STRING_LITERAL: return "string";
        case TokenType::KW_LET: return "'let'";
        case TokenType::KW_FUNC: return "'func'";
        case TokenType::KW_DO: return "'do'";
        case TokenType::KW_RETURN: return "'return'";
        case TokenType::KW_IF: return "'if'";
        case TokenType::KW_ELSE: return "'else'";
        case TokenType::KW_LOOP: return "'loop'";
        case TokenType::KW_WHILE: return "'while'";
        case TokenType::KW_BREAK: return "'break'";
        case TokenType::KW_CONTINUE: return "'continue'";
        case TokenType::KW_IMPORT: return "'import'";
        case TokenType::KW_USE: return "'use'";
        case TokenType::KW_AS: return "'as'";
        case TokenType::KW_VEC: return "'vec'";
        case TokenType::KW_CONST: return "'const'";
        case TokenType::KW_VAR: return "'var'";
        case TokenType::KW_INTERN: return "'intern'";
        case TokenType::KW_EXPORT: return "'export'";
        case TokenType::KW_WITH: return "'with'";
        case TokenType::KW_MAKE: return "'make'";
        case TokenType::OPER_PLUS: return "'+'";
        case TokenType::OPER_MINUS: return "'-'";
        case TokenType::OPER_MUL: return "'*'";
        case TokenType::OPER_DIV: return "'/'";
        case TokenType::OPER_NOT: return "'!'";
        case TokenType::OPER_EQ: return "'=='";
        case TokenType::OPER_NE: return "'!='";
        case TokenType::OPER_LT: return "'<'";
        case TokenType::OPER_GT: return "'>'";
        case TokenType::OPER_LE: return "'<='";
        case TokenType::OPER_GE: return "'>='";
        case TokenType::OPER_COMMA: return "','";
        case TokenType::OPER_DOT: return "'.'";
        case TokenType::OPER_COLON: return "':'";
        case TokenType::ASSIGN: return "'='";
        case TokenType::LPAREN: return "'('";
        case TokenType::RPAREN: return "')'";
        case TokenType::LBRACE: return "'{'";
        case TokenType::RBRACE: return "'}'";
        case TokenType::LBRACKET: return "'['";
        case TokenType::RBRACKET: return "']'";
        case TokenType::NEWLINE: return "newline";
        case TokenType::MISMATCH: return "mismatch";
        default: return "unknown";
    }
}

void Parser::throw_error(const std::string& message) {
    const Token tok = current_token();
    throw_error_at(message, tok);
}

void Parser::throw_error_at(const std::string& message, const Token& token) {
    std::string error_msg;
    
    if (!filename.empty()) {
        if (token.line > 0) {
            error_msg += "at " + filename + ", line " + std::to_string(token.line) + ", column " + std::to_string(token.column) + "\n";
        } else {
            error_msg += "in " + filename + "\n";
        }
    } else if (token.line > 0) {
        error_msg += "at line " + std::to_string(token.line) + ", column " + std::to_string(token.column) + "\n";
    }

    LOG(ITIS(token.line) << ", " << ITIS(source_lines.size()));
    
    if (token.line > 0 && static_cast<size_t>(token.line) <= source_lines.size()) {
        error_msg += "  " + source_lines[token.line - 1] + "\n";
        error_msg += "  " + std::string(token.column - 1, ' ') + "^";
    }
    
    error_msg += "\n" + message;
    
    current_pos = tokens.size();
    throw SyntaxError(error_msg);
}

Token Parser::current_token() const {
    if (current_pos < tokens.size()) {
        return tokens[current_pos];
    }
    return {TokenType::END, "", 0, 0};
}

void Parser::advance() {
    if (current_pos < tokens.size()) {
        current_pos++;
    }
}

void Parser::consume(const TokenType expected) {
    if (check(expected)) {
        advance();
    } else {
        const Token tok = current_token();
        throw_error_at("expected " + getTokenTypeName(expected) + ", found " + 
                    (tok.value.empty() ? getTokenTypeName(tok.type) : "'" + tok.value + "'"), tok);
    }
}

bool Parser::match(const TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(const TokenType type) const {
    return !isAtEnd() && current_token().type == type;
}

bool Parser::isAtEnd() const {
    return current_pos >= tokens.size() || current_token().type == TokenType::END;
}

Token Parser::peek(int n) const {
    const size_t idx = current_pos + n;
    if (idx < tokens.size()) {
        return tokens[idx];
    }
    return {TokenType::END, "", 0, 0};
}

ProgramASTNode* Parser::parse() {
     LOG("Parsing program");
    const auto statements = parseStatementList();
    const auto program = new ProgramASTNode(statements);
    LOG("Program parsed successfully");
    return program;
}

std::vector<ASTNode*> Parser::parse_rest() {
    LOG("Parsing rest");
    std::vector<ASTNode*> statements;
    
    while (!isAtEnd()) {
        if (current_token().type == TokenType::END) {
            break;
        }
        
        while (!isAtEnd() && current_token().type == TokenType::NEWLINE) {
            if (!ignore_newline()) {
                break;
            }
            advance();
        }
        
        if (isAtEnd()) {
            break;
        }
        
        ASTNode* stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
        
        if (!isAtEnd() && current_token().type != TokenType::END) {
            if (!ignore_newline()) {
                if (!match(TokenType::NEWLINE) && !isAtEnd() && current_token().type != TokenType::END && current_token().type != TokenType::RBRACE) {
                    Token tok = current_token();
                    throw_error_at("unexpected token '" + tok.value + "' on same line", tok);
                }
            }
        }
    }
    
    return statements;
}

std::vector<ASTNode*> Parser::parseStatementList() {
    LOG("Creating stmt_list");
    std::vector<ASTNode*> statements;
    
    while (!isAtEnd() && current_token().type != TokenType::RBRACE) {
        while (!isAtEnd() && current_token().type == TokenType::NEWLINE) {
            if (!ignore_newline()) {
                break;
            }
            advance();
        }
        
        if (isAtEnd() || current_token().type == TokenType::RBRACE) {
            break;
        }
        
        ASTNode* stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
        
        if (!isAtEnd() && current_token().type != TokenType::RBRACE) {
            if (!ignore_newline()) {
                if (!match(TokenType::NEWLINE) && !isAtEnd() && current_token().type != TokenType::END && current_token().type != TokenType::RBRACE) {
                    Token tok = current_token();
                    throw_error_at("unexpected token '" + tok.value + "' on same line", tok);
                }
            }
        }
    }
    
    return statements;
}

std::vector<ASTNode*> Parser::parseBlockStatementList() {
    LOG("Creating block_stmt_list");
    std::vector<ASTNode*> statements;
    
    while (!isAtEnd() && current_token().type != TokenType::RBRACE) {
        while (!isAtEnd() && current_token().type == TokenType::NEWLINE) {
            advance();
        }
        
        if (isAtEnd() || current_token().type == TokenType::RBRACE) {
            break;
        }
        
        ASTNode* stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
        
        if (!isAtEnd() && current_token().type != TokenType::RBRACE) {
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
    
    if (isAtEnd()) {
        return nullptr;
    }
    
    TokenType current = current_token().type;
    
    if (current == TokenType::KW_LET || current == TokenType::KW_CONST || 
        current == TokenType::KW_INTERN || current == TokenType::KW_EXPORT) {
        return parseVarDecl();
        }

    if (current == TokenType::IDENTIFIER && peek(1).type == TokenType::ASSIGN) {
        return parseAssignStmt();
    }

    if (current == TokenType::KW_FUNC) {
        return parseFuncDecl();
    }

    if (current == TokenType::KW_RETURN) {
        return parseReturnStmt();
    }

    if (current == TokenType::KW_IF) {
        return parseIfStmt();
    }

    if (current == TokenType::KW_LOOP) {
        return parseLoopStmt();
    }

    if (current == TokenType::KW_WHILE) {
        return parseWhileStmt();
    }

    if (current == TokenType::KW_FOR) {
        return parseForLoopStmt();
    }

    if (current == TokenType::KW_BREAK) {
        return parseBreakStmt();
    }

    if (current == TokenType::KW_CONTINUE) {
        return parseContinueStmt();
    }

    if (current == TokenType::KW_IMPORT) {
        return parseImportStmt();
    }

    if (current == TokenType::KW_USE) {
        return parseUseStmt();
    }

    if (current == TokenType::IDENTIFIER || current == TokenType::NUM_LITERAL ||
        current == TokenType::STRING_LITERAL || current == TokenType::LPAREN ||
        current == TokenType::LBRACKET || current == TokenType::KW_DO ||
        current == TokenType::LBRACE) {
        ExprNode* expr = parseExpression();

        while (true) {
            if (match(TokenType::LPAREN)) {
                auto args = parseArgList();
                consume(TokenType::RPAREN);
                expr = new FuncCallExprNode(expr, args);
            } else if (match(TokenType::OPER_DOT)) {
                std::string name = current_token().value;
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
    
    LOG("Nothing matched, token type: " << static_cast<size_t>(current));
    Token tok = current_token();
    throw_error_at("unexpected token: " + (tok.value.empty() ? getTokenTypeName(tok.type) : "'" + tok.value + "'"), tok);
    return nullptr;
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
    }
    
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::ASSIGN);
    ExprNode* value = parseExpression();
    
    return new VarDeclNode(name, value, is_const, visibility);
}

ASTNode* Parser::parseAssignStmt() {
    LOG("Parsing assign_stmt");
    ExprNode* left = parsePostfixExpr();
    consume(TokenType::ASSIGN);
    ExprNode* right = parseExpression();
    
    return new AssignNode(left, right);
}

ASTNode* Parser::parseFuncDecl() {
    LOG("Parsing func_decl");
    std::vector<ExprNode*> decorators;
    
    if (check(TokenType::KW_WITH)) {
        decorators = parseWithDecoratorList();
    }
    
    consume(TokenType::KW_FUNC);
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    return new FuncDeclNode(name, params, new BlockStmtNode(body), Visibility::Exported, decorators);
}

ASTNode* Parser::parseReturnStmt() {
    LOG("Parsing return_stmt");
    consume(TokenType::KW_RETURN);
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in return stmt");
    }
    
    if (check(TokenType::RBRACE) || check(TokenType::NEWLINE) || check(TokenType::END)) {
        return new ReturnStmtNode(nullptr);
    }
    
    ExprNode* value = parseExpression();
    return new ReturnStmtNode(value);
}

ASTNode* Parser::parseIfStmt() {
    LOG("Parsing if_stmt");
    consume(TokenType::KW_IF);
    consume(TokenType::LPAREN);
    ExprNode* condition = parseExpression();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto then_body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    BlockStmtNode* else_body = nullptr;
    if (match(TokenType::KW_ELSE)) {
        consume(TokenType::LBRACE);
        push_context(ParserContext::FunctionBody);
        else_body = new BlockStmtNode(parseBlockStatementList());
        pop_context();
        consume(TokenType::RBRACE);
    }
    
    return new IfStmtNode(condition, new BlockStmtNode(then_body), else_body);
}

ASTNode* Parser::parseLoopStmt() {
    LOG("Parsing loop_stmt");
    consume(TokenType::KW_LOOP);
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    return new LoopNode(nullptr, new BlockStmtNode(body));
}

ASTNode* Parser::parseWhileStmt() {
    LOG("Parsing while_stmt");
    consume(TokenType::KW_WHILE);
    consume(TokenType::LPAREN);
    ExprNode* condition = parseExpression();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    return new WhileStmtNode(condition, new BlockStmtNode(body));
}

ASTNode* Parser::parseForLoopStmt() {
    LOG("Parsing for_loop_stmt");
    consume(TokenType::KW_FOR);
    consume(TokenType::LPAREN);
    
    std::vector<ForLoopNode::IterationItem*> items;
    
    while (!check(TokenType::RPAREN)) {
        while (match(TokenType::NEWLINE)) {
        }
        if (check(TokenType::RPAREN)) {
            break;
        }

        std::string var_name = current_token().value;
        consume(TokenType::IDENTIFIER);
        consume(TokenType::KW_IN);
        ExprNode* iterable = parseExpression();
        
        items.push_back(new ForLoopNode::IterationItem(var_name, iterable));
        
        while (match(TokenType::NEWLINE)) {
        }
        if (check(TokenType::RPAREN)) {
            break;
        }
        if (!match(TokenType::OPER_COMMA)) {
            if (!check(TokenType::IDENTIFIER)) {
                break;
            }
        }
    }
    
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    return new ForLoopNode(items, new BlockStmtNode(body));
}

ASTNode* Parser::parseBreakStmt() {
    LOG("Parsing break_stmt");
    consume(TokenType::KW_BREAK);
    return new BreakNode();
}

ASTNode* Parser::parseContinueStmt() {
    LOG("Parsing continue_stmt");
    consume(TokenType::KW_CONTINUE);
    return new ContinueNode();
}

ASTNode* Parser::parseBlockStmt() {
    LOG("Parsing block_stmt");
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto statements = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    return new BlockStmtNode(statements);
}

ASTNode* Parser::parseImportStmt() {
    LOG("Parsing import_stmt");
    consume(TokenType::KW_IMPORT);
    auto name = parseQualifiedName();
    std::string alias;
    
    if (match(TokenType::KW_AS)) {
        alias = current_token().value;
        consume(TokenType::IDENTIFIER);
    }
    
    return new ImportNode(name, alias);
}

ASTNode* Parser::parseUseStmt() {
    LOG("Parsing use_stmt");
    consume(TokenType::KW_USE);
    auto module_name = parseQualifiedName();
    consume(TokenType::OPER_DOT);
    consume(TokenType::LBRACE);
    auto items = parseUseList();
    consume(TokenType::RBRACE);
    
    return new UseNode(module_name, items);
}

ExprNode* Parser::parseExpression() {
    return parseComparisonExpr();
}

ExprNode* Parser::parseComparisonExpr() {
    LOG("Parsing comparison_expr");
    ExprNode* left = parseAdditiveExpr();
    
    while (true) {
        const TokenType op_type = current_token().type;
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
            default: ;
        }
        
        advance();
        ExprNode* right = parseAdditiveExpr();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseAdditiveExpr() {
    LOG("Parsing additive_expr");
    ExprNode* left = parseMultiplicativeExpr();
    
    while (true) {
        const TokenType op_type = current_token().type;
        if (op_type != TokenType::OPER_PLUS && op_type != TokenType::OPER_MINUS) {
            break;
        }

        const std::string op_str = (op_type == TokenType::OPER_PLUS) ? "+" : "-";
        advance();
        ExprNode* right = parseMultiplicativeExpr();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseMultiplicativeExpr() {
    LOG("Parsing multiplicative_expr");
    ExprNode* left = parseUnaryExpr();
    
    while (true) {
        TokenType op_type = current_token().type;
        if (op_type != TokenType::OPER_MUL && op_type != TokenType::OPER_DIV) {
            break;
        }
        
        std::string op_str = (op_type == TokenType::OPER_MUL) ? "*" : "/";
        advance();
        ExprNode* right = parseUnaryExpr();
        left = new BinaryNode(left, right, op_str);
    }
    
    return left;
}

ExprNode* Parser::parseUnaryExpr() {
    LOG("Parsing unary_expr");
    
    if (match(TokenType::OPER_NOT)) {
        ExprNode* operand = parseUnaryExpr();
        return new UnaryNode("!", operand);
    }
    
    if (match(TokenType::OPER_MINUS)) {
        ExprNode* operand = parseUnaryExpr();
        return new UnaryNode("-", operand);
    }
    
    return parsePostfixExpr();
}

ExprNode* Parser::parsePostfixExpr() {
    LOG("Parsing postfix_expr");
    ExprNode* expr = parseFactor();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            auto args = parseArgList();
            consume(TokenType::RPAREN);
            expr = new FuncCallExprNode(expr, args);
        } else if (match(TokenType::OPER_DOT)) {
            std::string name = current_token().value;
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

ExprNode* Parser::parseFactor() {
    LOG("Parsing factor");
    
    if (check(TokenType::NUM_LITERAL)) {
        std::string value = current_token().value;
        advance();
        return new NumberNode(value);
    }
    
    if (check(TokenType::STRING_LITERAL)) {
        std::string value = current_token().value;
        advance();
        return new StringNode(value);
    }
    
    if (check(TokenType::IDENTIFIER)) {
        std::string value = current_token().value;
        advance();
        return new VarRefNode(value);
    }
    
    if (match(TokenType::LPAREN)) {
        ExprNode* expr = parseExpression();
        consume(TokenType::RPAREN);
        return expr;
    }
    
    if (match(TokenType::KW_VEC)) {
        consume(TokenType::LBRACKET);
        push_context(ParserContext::Vec);
        auto elements = parseVecElementList();
        pop_context();
        consume(TokenType::RBRACKET);
        return new VectorNode(elements);
    }
    
    if (match(TokenType::LBRACKET)) {
        push_context(ParserContext::Vec);
        auto elements = parseVecElementList();
        pop_context();
        consume(TokenType::RBRACKET);
        return new VectorNode(elements);
    }
    
    if (match(TokenType::LBRACE)) {
        push_context(ParserContext::Dict);
        auto entries = parseDictEntryList();
        pop_context();
        consume(TokenType::RBRACE);
        return new DictionaryNode(entries);
    }
    
    if (match(TokenType::KW_DO)) {
        std::vector<ExprNode*> decorators;
        
        if (check(TokenType::KW_WITH)) {
            decorators = parseWithDecoratorList();
        }
        
        consume(TokenType::LPAREN);
        auto params = parseParamList();
        consume(TokenType::RPAREN);
        consume(TokenType::LBRACE);
        push_context(ParserContext::DoBody);
        auto body = parseBlockStatementList();
        pop_context();
        consume(TokenType::RBRACE);
        
        return new DoFuncDeclNode(params, new BlockStmtNode(body), decorators);
    }
    
    Token tok = current_token();
    throw_error_at("expected expression, found " + (tok.value.empty() ? getTokenTypeName(tok.type) : "'" + tok.value + "'"), tok);
    return nullptr;
}

ExprNode* Parser::parseDoExpr() {
    LOG("Parsing do_expr");
    std::vector<ExprNode*> decorators;
    
    if (check(TokenType::KW_WITH)) {
        decorators = parseWithDecoratorList();
    }
    
    consume(TokenType::KW_DO);
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    push_context(ParserContext::DoBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);
    
    return new DoFuncDeclNode(params, new BlockStmtNode(body), decorators);
}

std::vector<std::string> Parser::parseParamList() {
    LOG("Parsing param_list");
    std::vector<std::string> params;
    
    if (check(TokenType::RPAREN)) {
        return params;
    }
    
    params.push_back(current_token().value);
    consume(TokenType::IDENTIFIER);
    
    while (match(TokenType::OPER_COMMA)) {
        params.push_back(current_token().value);
        consume(TokenType::IDENTIFIER);
    }
    
    return params;
}

std::vector<ASTNode*> Parser::parseArgList() {
    LOG("Parsing arg_list");
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

std::vector<ASTNode*> Parser::parseVecElementList() {
    LOG("Parsing vec_element_list");
    std::vector<ASTNode*> elements;
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in vec");
    }
    
    if (check(TokenType::RBRACKET)) {
        return elements;
    }
    
    elements.push_back(parseExpression());
    
    while (match(TokenType::OPER_COMMA)) {
        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in vec after comma");
        }
        elements.push_back(parseExpression());
    }
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in vec at end");
    }
    
    return elements;
}

std::vector<DictEntryNode*> Parser::parseDictEntryList() {
    LOG("Parsing dict_entry_list");
    std::vector<DictEntryNode*> entries;
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in dict");
    }
    
    if (check(TokenType::RBRACE)) {
        return entries;
    }
    
    ExprNode* key = parseExpression();
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in dict after key");
    }
    
    consume(TokenType::OPER_COLON);
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in dict after colon");
    }
    
    ExprNode* value = parseExpression();
    entries.push_back(new DictEntryNode(key, value));
    
    while (match(TokenType::OPER_COMMA)) {
        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in dict after comma");
        }
        
        key = parseExpression();
        
        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in dict after key in loop");
        }
        
        consume(TokenType::OPER_COLON);
        
        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in dict after colon in loop");
        }
        
        value = parseExpression();
        entries.push_back(new DictEntryNode(key, value));
    }
    
    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in dict at end");
    }
    
    return entries;
}

std::vector<std::string> Parser::parseQualifiedName() {
    LOG("Parsing qualified_name");
    std::vector<std::string> parts;
    
    parts.push_back(current_token().value);
    consume(TokenType::IDENTIFIER);
    
    while (match(TokenType::OPER_DOT)) {
        parts.push_back(current_token().value);
        consume(TokenType::IDENTIFIER);
    }
    
    return parts;
}

std::vector<UseItem> Parser::parseUseList() {
    LOG("Parsing use_list");
    std::vector<UseItem> items;
    
    items.push_back(parseUseItem());
    
    while (match(TokenType::OPER_COMMA)) {
        items.push_back(parseUseItem());
    }
    
    return items;
}

UseItem Parser::parseUseItem() {
    LOG("Parsing use_item");
    const std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);
    
    std::string alias;
    if (match(TokenType::KW_AS)) {
        alias = current_token().value;
        consume(TokenType::IDENTIFIER);
    }
    
    return {name, alias};
}

std::vector<ExprNode*> Parser::parseDecorators() {
    LOG("Parsing decorators");
    std::vector<ExprNode*> decorators;
    
    decorators.push_back(parseExpression());
    
    while (check(TokenType::IDENTIFIER) || check(TokenType::LPAREN)) {
        decorators.push_back(parseExpression());
    }
    
    return decorators;
}

std::vector<ExprNode*> Parser::parseWithDecoratorList() {
    LOG("Parsing with_decorator_list");
    consume(TokenType::KW_WITH);
    auto decorators = parseDecorators();
    consume(TokenType::KW_MAKE);
    
    return decorators;
}

} // namespace lmx