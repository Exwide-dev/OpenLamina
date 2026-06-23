#include "parser.hpp"
#include "../../tools/debug.hpp"
#include "../../tools/error.hpp"
#include <format>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace lmx {
namespace {

ExprNode* cloneExpr(const ExprNode* node) {
    if (node == nullptr) {
        return nullptr;
    }

    switch (node->kind) {
        case ASTNodeType::Number:
            return new NumberNode(dynamic_cast<const NumberNode*>(node)->value);
        case ASTNodeType::String:
            return new StringNode(dynamic_cast<const StringNode*>(node)->value);
        case ASTNodeType::Bool:
            return new BoolNode(dynamic_cast<const BoolNode*>(node)->value);
        case ASTNodeType::VarRef: {
            const auto* ref = dynamic_cast<const VarRefNode*>(node);
            return new VarRefNode(ref->name);
        }
        case ASTNodeType::Unary: {
            const auto* unary = dynamic_cast<const UnaryNode*>(node);
            return new UnaryNode(unary->op, cloneExpr(unary->operand));
        }
        case ASTNodeType::Binary: {
            const auto* binary = dynamic_cast<const BinaryNode*>(node);
            return new BinaryNode(cloneExpr(binary->left), cloneExpr(binary->right), binary->op);
        }
        case ASTNodeType::FuncCallExpr: {
            const auto* call = dynamic_cast<const FuncCallExprNode*>(node);
            std::vector<CallArgument> args;
            args.reserve(call->args.size());
            for (const auto& arg : call->args) {
                args.emplace_back(cloneExpr(arg.value), arg.name);
            }
            return new FuncCallExprNode(cloneExpr(call->func_expr), std::move(args));
        }
        case ASTNodeType::MemberAccess: {
            const auto* access = dynamic_cast<const MemberAccessNode*>(node);
            return new MemberAccessNode(cloneExpr(access->object), access->member);
        }
        case ASTNodeType::TypeConvert: {
            const auto* convert = dynamic_cast<const TypeConvertExprNode*>(node);
            return new TypeConvertExprNode(
                cloneExpr(convert->type_expr),
                cloneExpr(convert->value_expr)
            );
        }
        case ASTNodeType::IndexAccess: {
            const auto* access = dynamic_cast<const IndexAccessNode*>(node);
            return new IndexAccessNode(cloneExpr(access->object), cloneExpr(access->index));
        }
        case ASTNodeType::Vector: {
            const auto* vec = dynamic_cast<const VectorNode*>(node);
            std::vector<ASTNode*> elements;
            elements.reserve(vec->elements.size());
            for (const auto element : vec->elements) {
                elements.push_back(cloneExpr(dynamic_cast<ExprNode*>(element)));
            }
            return new VectorNode(std::move(elements));
        }
        case ASTNodeType::Dictionary: {
            const auto* dict = dynamic_cast<const DictionaryNode*>(node);
            std::vector<DictEntryNode*> entries;
            entries.reserve(dict->entries.size());
            for (const auto entry : dict->entries) {
                entries.push_back(new DictEntryNode(
                    cloneExpr(entry->key),
                    cloneExpr(entry->value)
                ));
            }
            return new DictionaryNode(std::move(entries));
        }
        default:
            throw std::runtime_error("unsupported expression in pipeline placeholder substitution");
    }
}

ExprNode* substitutePipelinePlaceholder(ExprNode* expr, const ExprNode* value) {
    if (expr == nullptr) {
        return nullptr;
    }

    if (expr->kind == ASTNodeType::Placeholder) {
        delete expr;
        return cloneExpr(value);
    }

    switch (expr->kind) {
        case ASTNodeType::Unary: {
            auto* unary = dynamic_cast<UnaryNode*>(expr);
            unary->operand = substitutePipelinePlaceholder(unary->operand, value);
            return expr;
        }
        case ASTNodeType::Binary: {
            auto* binary = dynamic_cast<BinaryNode*>(expr);
            binary->left = substitutePipelinePlaceholder(binary->left, value);
            binary->right = substitutePipelinePlaceholder(binary->right, value);
            return expr;
        }
        case ASTNodeType::FuncCallExpr: {
            auto* call = dynamic_cast<FuncCallExprNode*>(expr);
            call->func_expr = substitutePipelinePlaceholder(call->func_expr, value);
            for (auto& arg : call->args) {
                arg.value = substitutePipelinePlaceholder(arg.value, value);
            }
            return expr;
        }
        case ASTNodeType::MemberAccess: {
            auto* access = dynamic_cast<MemberAccessNode*>(expr);
            access->object = substitutePipelinePlaceholder(access->object, value);
            return expr;
        }
        case ASTNodeType::IndexAccess: {
            auto* access = dynamic_cast<IndexAccessNode*>(expr);
            access->object = substitutePipelinePlaceholder(access->object, value);
            access->index = substitutePipelinePlaceholder(access->index, value);
            return expr;
        }
        case ASTNodeType::Vector: {
            auto* vec = dynamic_cast<VectorNode*>(expr);
            for (auto& element : vec->elements) {
                element = substitutePipelinePlaceholder(dynamic_cast<ExprNode*>(element), value);
            }
            return expr;
        }
        case ASTNodeType::Dictionary: {
            auto* dict = dynamic_cast<DictionaryNode*>(expr);
            for (const auto entry : dict->entries) {
                entry->key = substitutePipelinePlaceholder(entry->key, value);
                entry->value = substitutePipelinePlaceholder(entry->value, value);
            }
            return expr;
        }
        default:
            return expr;
    }
}

bool containsPlaceholder(const ExprNode* expr) {
    if (expr == nullptr) {
        return false;
    }

    if (expr->kind == ASTNodeType::Placeholder) {
        return true;
    }

    switch (expr->kind) {
        case ASTNodeType::Unary:
            return containsPlaceholder(dynamic_cast<const UnaryNode*>(expr)->operand);
        case ASTNodeType::Binary: {
            const auto* binary = dynamic_cast<const BinaryNode*>(expr);
            return containsPlaceholder(binary->left) || containsPlaceholder(binary->right);
        }
        case ASTNodeType::FuncCallExpr: {
            const auto* call = dynamic_cast<const FuncCallExprNode*>(expr);
            if (containsPlaceholder(call->func_expr)) {
                return true;
            }
            for (const auto& arg : call->args) {
                if (containsPlaceholder(arg.value)) {
                    return true;
                }
            }
            return false;
        }
        case ASTNodeType::MemberAccess:
            return containsPlaceholder(dynamic_cast<const MemberAccessNode*>(expr)->object);
        case ASTNodeType::IndexAccess: {
            const auto* access = dynamic_cast<const IndexAccessNode*>(expr);
            return containsPlaceholder(access->object) || containsPlaceholder(access->index);
        }
        case ASTNodeType::Vector: {
            const auto* vec = dynamic_cast<const VectorNode*>(expr);
            for (const auto element : vec->elements) {
                if (containsPlaceholder(dynamic_cast<const ExprNode*>(element))) {
                    return true;
                }
            }
            return false;
        }
        case ASTNodeType::Dictionary: {
            const auto* dict = dynamic_cast<const DictionaryNode*>(expr);
            for (const auto entry : dict->entries) {
                if (containsPlaceholder(entry->key) || containsPlaceholder(entry->value)) {
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

} // namespace

Parser::Parser(std::string filename)
    : filename(std::move(filename)) {
}

void Parser::append_source_lines(const std::string& source) {
    if (source.empty()) {
        return;
    }
    std::istringstream iss(source);
    std::string line_str;
    while (std::getline(iss, line_str)) {
        source_lines.push_back(line_str);
    }
}

void Parser::set_source_lines(const std::string& source) {
    source_lines.clear();
    append_source_lines(source);
}

void Parser::add_tokens(std::vector<Token> toks, const std::string& source) {
    tokens.insert(tokens.end(), toks.begin(), toks.end());
    append_source_lines(source);

    if (source.empty()) {
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

void Parser::append_tokens(std::vector<Token> toks, const std::string& source) {
    if (!tokens.empty() && tokens.back().type == TokenType::END) {
        tokens.pop_back();
    }
    current_pos = tokens.size();

    add_tokens(std::move(toks), source);
}

std::string Parser::getTokenTypeName(const TokenType type) {
    switch (type) {
        case TokenType::END: return "END";
        case TokenType::IDENTIFIER: return "identifier";
        case TokenType::NUM_LITERAL: return "number";
        case TokenType::STRING_LITERAL: return "string";
        case TokenType::KW_LET: return "'let'";
        case TokenType::KW_FUNC: return "'func'";
        case TokenType::KW_FRIEND: return "'friend'";
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
        case TokenType::KW_MATCH: return "'match'";
        case TokenType::KW_CASE: return "'case'";
        case TokenType::KW_TRY: return "'try'";
        case TokenType::KW_CATCH: return "'catch'";
        case TokenType::KW_THROW: return "'throw'";
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
        case TokenType::OPER_ARROW: return "'->'";
        case TokenType::OPER_PIPE: return "'|>'";
        case TokenType::OPER_BAR: return "'|'";
        case TokenType::ASSIGN: return "'='";
        case TokenType::LPAREN: return "'('";
        case TokenType::RPAREN: return "')'";
        case TokenType::LBRACE: return "'{'";
        case TokenType::RBRACE: return "'}'";
        case TokenType::LBRACKET: return "'['";
        case TokenType::RBRACKET: return "']'";
        case TokenType::NEWLINE: return "newline";
        case TokenType::PLACEHOLDER: return "'_'";
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
            error_msg += "at " + filename + ", line " + std::to_string(token.line) + ", column " + std::to_string(
                token.column
            ) + "\n";
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
        throw_error_at(
            "expected " + getTokenTypeName(expected) + ", found " +
            (tok.value.empty() ? getTokenTypeName(tok.type) : "'" + tok.value + "'"),
            tok
        );
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
    program->source_lines = source_lines;
    program->source_filename = filename;
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
                if (!match(TokenType::NEWLINE) && !isAtEnd() && current_token().type != TokenType::END &&
                    current_token().type != TokenType::RBRACE) {
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

    if (current == TokenType::KW_FUNC || looksLikeDecoratedFuncDecl() ||
        looksLikeFuncDeclWithLeadingModifier()) {
        return parseFuncDecl();
    }

    if (current == TokenType::KW_LET || current == TokenType::KW_CONST ||
        current == TokenType::KW_INTERN || current == TokenType::KW_EXPORT) {
        return parseVarDecl();
    }

    if (looksLikeAssignStmt()) {
        return parseAssignStmt();
    }

    if (current == TokenType::KW_TYPED || current == TokenType::KW_STRUCT) {
        return parseStructDecl();
    }

    if (current == TokenType::KW_FRIEND) {
        return parseFriendFuncDecl();
    }

    if (current == TokenType::KW_WITH) {
        const ParserState saved = save_state();
        parseWithDecoratorList();
        const bool is_do = check(TokenType::KW_DO);
        restore_state(saved);
        return is_do ? parseDecoratedDoDecl() : parseFuncDecl();
    }

    if (current == TokenType::KW_DO || looksLikeDecoratedDoDecl()) {
        return parseExpression();
    }

    if (current == TokenType::KW_RETURN) {
        return parseReturnStmt();
    }

    if (current == TokenType::KW_THROW) {
        return parseThrowStmt();
    }

    if (current == TokenType::KW_TRY) {
        return parseTryStmt();
    }

    if (current == TokenType::KW_IF) {
        return parseIfStmt();
    }

    if (current == TokenType::KW_MATCH) {
        return parseMatchStmt();
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
        current == TokenType::LBRACE || current == TokenType::OPER_MUL ||
        current == TokenType::OPER_AMP || current == TokenType::OPER_MINUS ||
        current == TokenType::OPER_NOT) {
        ExprNode* expr = parseExpression();

        while (true) {
            if (check(TokenType::LPAREN)) {
                const int line = current_token().line;
                match(TokenType::LPAREN);
                auto args = parseArgList();
                consume(TokenType::RPAREN);
                expr = make_node_at<FuncCallExprNode>(line, expr, args);
            } else if (check(TokenType::OPER_DOT)) {
                const int line = current_token().line;
                match(TokenType::OPER_DOT);
                std::string name = current_token().value;
                consume(TokenType::IDENTIFIER);
                expr = make_node_at<MemberAccessNode>(line, expr, name);
            } else if (check(TokenType::LBRACKET)) {
                const int line = current_token().line;
                match(TokenType::LBRACKET);
                ExprNode* index = parseExpression();
                consume(TokenType::RBRACKET);
                expr = make_node_at<IndexAccessNode>(line, expr, index);
            } else {
                break;
            }
        }

        return expr;
    }

    LOG("Nothing matched, token type: " << static_cast<size_t>(current));
    Token tok = current_token();
    std::string token_label;
    if (tok.value.empty()) {
        token_label = getTokenTypeName(tok.type);
    } else if (tok.type == TokenType::MISMATCH) {
        token_label = "invalid UTF-8 or character (bytes: ";
        for (unsigned char b : tok.value) {
            token_label += std::format("{:02X} ", b);
        }
        if (!tok.value.empty()) {
            token_label.pop_back();
        }
        token_label += ")";
    } else {
        token_label = "'" + tok.value + "'";
    }
    throw_error_at("unexpected token: " + token_label, tok);
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
    const int decl_line = current_token().line;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::ASSIGN);
    ExprNode* value = parseExpression();

    return make_node_at<VarDeclNode>(decl_line, name, value, is_const, visibility);
}

ASTNode* Parser::parseAssignStmt() {
    LOG("Parsing assign_stmt");
    ExprNode* left = parseUnaryExpr();
    const int assign_line = current_token().line;
    consume(TokenType::ASSIGN);
    ExprNode* right = parseExpression();

    return make_node_at<AssignNode>(assign_line, left, right);
}

bool Parser::looksLikeDecoratedFuncDecl() {
    if (isAtEnd() || check(TokenType::KW_FUNC)) {
        return false;
    }

    const TokenType start = current_token().type;
    if (start != TokenType::IDENTIFIER && start != TokenType::LPAREN &&
        start != TokenType::STRING_LITERAL) {
        return false;
    }

    const ParserState saved = save_state();
    parsePostfixExpr(false);
    const bool is_decorated = check(TokenType::KW_FUNC);
    restore_state(saved);
    return is_decorated;
}

bool Parser::looksLikeFuncDeclWithLeadingModifier() {
    if (!check(TokenType::KW_INTERN) && !check(TokenType::KW_EXPORT)) {
        return false;
    }

    const ParserState saved = save_state();
    advance();
    const bool is_func = check(TokenType::KW_FUNC) || looksLikeDecoratedFuncDecl();
    restore_state(saved);
    return is_func;
}

bool Parser::parseFuncVisibilityModifier(Visibility& visibility) {
    if (match(TokenType::KW_INTERN)) {
        visibility = Visibility::Internal;
        return true;
    }
    if (match(TokenType::KW_EXPORT)) {
        visibility = Visibility::Exported;
        return true;
    }
    return false;
}

bool Parser::looksLikeDecoratedDoDecl() {
    if (isAtEnd() || check(TokenType::KW_DO) || check(TokenType::KW_WITH)) {
        return false;
    }

    const TokenType start = current_token().type;
    if (start != TokenType::IDENTIFIER && start != TokenType::LPAREN &&
        start != TokenType::STRING_LITERAL) {
        return false;
    }

    const ParserState saved = save_state();
    parsePostfixExpr(false);
    const bool is_decorated = check(TokenType::KW_DO);
    restore_state(saved);
    return is_decorated;
}

bool Parser::looksLikeAssignStmt() {
    if (!check(TokenType::IDENTIFIER) && !check(TokenType::OPER_MUL)) {
        return false;
    }

    const ParserState saved = save_state();
    parseUnaryExpr();
    const bool is_assign = check(TokenType::ASSIGN);
    restore_state(saved);
    return is_assign;
}

TypeNode* Parser::parseTypeName() {
    const int line = current_token().line;
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);
    return make_node_at<TypeNode>(line, name);
}

TypeNode* Parser::parseOptionalReturnType() {
    if (!match(TokenType::OPER_ARROW)) {
        return nullptr;
    }
    return parseTypeName();
}

ASTNode* Parser::parseStructDecl() {
    LOG("Parsing struct_decl");
    bool typed = false;
    if (match(TokenType::KW_TYPED)) {
        typed = true;
    }
    const int struct_line = current_token().line;
    consume(TokenType::KW_STRUCT);
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);

    std::string base_name;
    if (match(TokenType::OPER_COLON)) {
        base_name = current_token().value;
        consume(TokenType::IDENTIFIER);
    }

    consume(TokenType::LBRACE);

    std::vector<StructField> fields;
    std::vector<FuncDeclNode*> methods;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        while (match(TokenType::NEWLINE)) {
        }

        if (check(TokenType::RBRACE)) {
            break;
        }

        if (check(TokenType::KW_FUNC)) {
            methods.push_back(parseStructMethod());
            if (!check(TokenType::RBRACE)) {
                match(TokenType::NEWLINE);
            }
            continue;
        }

        bool is_var = false;
        if (match(TokenType::KW_VAR)) {
            is_var = true;
        } else {
            consume(TokenType::KW_LET);
        }

        std::string field_name = current_token().value;
        consume(TokenType::IDENTIFIER);

        std::string type_name;
        bool has_type_annotation = false;
        if (match(TokenType::OPER_COLON)) {
            TypeNode* field_type = parseTypeName();
            type_name = field_type->name;
            delete field_type;
            has_type_annotation = true;
        } else if (typed) {
            throw_error_at("typed struct field must declare a type (e.g. a: num)", current_token());
        }

        ExprNode* default_init = nullptr;
        if (match(TokenType::ASSIGN)) {
            default_init = parseExpression();
        }

        fields.push_back(
            StructField{
                field_name,
                type_name,
                has_type_annotation,
                is_var,
                default_init
            }
        );

        if (!check(TokenType::RBRACE)) {
            match(TokenType::NEWLINE);
        }
    }

    consume(TokenType::RBRACE);
    return make_node_at<StructDeclNode>(
        struct_line,
        name,
        typed,
        std::move(fields),
        std::move(methods),
        std::move(base_name)
    );
}

FuncDeclNode* Parser::parseStructMethod() {
    LOG("Parsing struct_method");
    const int func_line = current_token().line;
    consume(TokenType::KW_FUNC);
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    TypeNode* ret_type = parseOptionalReturnType();

    if (params.empty() || params[0].name != "self") {
        throw_error_at("struct method first parameter must be 'self'", current_token());
    }

    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    auto* node = make_node_at<FuncDeclNode>(
        func_line,
        name,
        params,
        make_node_at<BlockStmtNode>(body_line, body),
        Visibility::Internal
    );
    node->ret_type = ret_type;
    return node;
}

std::vector<ForLoopNode::IterationItem*> Parser::parseComprehensionBindings() {
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

    return items;
}

ExprNode* Parser::finishComprehensionAfterFor(ExprNode* result_expr, const int line) {
    consume(TokenType::LPAREN);
    auto items = parseComprehensionBindings();
    consume(TokenType::RPAREN);

    ExprNode* guard = nullptr;
    if (match(TokenType::KW_IF)) {
        consume(TokenType::LPAREN);
        guard = parseExpression();
        consume(TokenType::RPAREN);
    }

    return make_node_at<ComprehensionNode>(line, result_expr, std::move(items), guard);
}

ASTNode* Parser::parseFriendFuncDecl() {
    LOG("Parsing friend_func_decl");
    const int decl_line = current_token().line;
    consume(TokenType::KW_FRIEND);
    consume(TokenType::KW_FUNC);
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);

    std::vector<FuncParam> params;
    BlockStmtNode* body = nullptr;
    TypeNode* ret_type = nullptr;

    if (match(TokenType::LPAREN)) {
        params = parseParamList();
        consume(TokenType::RPAREN);
        ret_type = parseOptionalReturnType();
        const int body_line = current_token().line;
        consume(TokenType::LBRACE);
        push_context(ParserContext::FunctionBody);
        auto body_stmts = parseBlockStatementList();
        pop_context();
        consume(TokenType::RBRACE);
        body = make_node_at<BlockStmtNode>(body_line, body_stmts);
    }

    return make_node_at<FriendFuncDeclNode>(decl_line, name, std::move(params), body, ret_type);
}

ASTNode* Parser::parseFuncDecl() {
    LOG("Parsing func_decl");
    Visibility visibility = Visibility::Exported;
    (void)parseFuncVisibilityModifier(visibility);

    std::vector<ExprNode*> decorators;
    if (check(TokenType::KW_WITH)) {
        decorators = parseWithDecoratorList();
    } else if (!check(TokenType::KW_FUNC)) {
        while (!check(TokenType::KW_FUNC) && !isAtEnd()) {
            if (check(TokenType::KW_INTERN) || check(TokenType::KW_EXPORT)) {
                break;
            }
            decorators.push_back(parsePostfixExpr(false));
        }
    }

    const int func_line = current_token().line;
    consume(TokenType::KW_FUNC);
    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    TypeNode* ret_type = parseOptionalReturnType();
    (void)parseFuncVisibilityModifier(visibility);

    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    auto* node = make_node_at<FuncDeclNode>(
        func_line,
        name,
        params,
        make_node_at<BlockStmtNode>(body_line, body),
        visibility,
        decorators
    );
    node->ret_type = ret_type;
    return node;
}

ASTNode* Parser::parseDecoratedDoDecl() {
    LOG("Parsing decorated_do_decl");
    std::vector<ExprNode*> decorators;

    if (check(TokenType::KW_WITH)) {
        decorators = parseWithDecoratorList();
    } else if (!check(TokenType::KW_DO)) {
        while (!check(TokenType::KW_DO)) {
            decorators.push_back(parsePostfixExpr(false));
        }
    }

    const int do_line = current_token().line;
    consume(TokenType::KW_DO);
    return finishDoFuncDecl(std::move(decorators), do_line);
}

DoFuncDeclNode* Parser::finishDoFuncDecl(std::vector<ExprNode*> decorators, const int do_line) {
    const int decl_line = do_line > 0 ? do_line : current_token().line;
    consume(TokenType::LPAREN);
    auto params = parseParamList();
    consume(TokenType::RPAREN);
    TypeNode* ret_type = parseOptionalReturnType();
    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::DoBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    auto* node = make_node_at<DoFuncDeclNode>(
        decl_line,
        params,
        make_node_at<BlockStmtNode>(body_line, body),
        std::move(decorators)
    );
    node->ret_type = ret_type;
    return node;
}

ASTNode* Parser::parseReturnStmt() {
    LOG("Parsing return_stmt");
    const int ret_line = current_token().line;
    consume(TokenType::KW_RETURN);

    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in return stmt");
    }

    if (check(TokenType::RBRACE) || check(TokenType::NEWLINE) || check(TokenType::END)) {
        return make_node_at<ReturnStmtNode>(ret_line, nullptr);
    }

    ExprNode* value = parseExpression();
    return make_node_at<ReturnStmtNode>(ret_line, value);
}

ASTNode* Parser::parseThrowStmt() {
    LOG("Parsing throw_stmt");
    const int throw_line = current_token().line;
    consume(TokenType::KW_THROW);

    while (match(TokenType::NEWLINE)) {
    }

    ExprNode* value = parseExpression();
    return make_node_at<ThrowStmtNode>(throw_line, value);
}

CatchClauseNode* Parser::parseCatchClause() {
    LOG("Parsing catch_clause");
    const int catch_line = current_token().line;
    consume(TokenType::KW_CATCH);
    consume(TokenType::LPAREN);

    std::string var_name;
    std::string type_name;
    bool catch_all = false;

    if (check(TokenType::OPER_DOT)) {
        consume(TokenType::OPER_DOT);
        consume(TokenType::OPER_DOT);
        consume(TokenType::OPER_DOT);
        catch_all = true;
    } else {
        var_name = current_token().value;
        consume(TokenType::IDENTIFIER);
        if (match(TokenType::OPER_COLON)) {
            if (check(TokenType::OPER_DOT)) {
                consume(TokenType::OPER_DOT);
                consume(TokenType::OPER_DOT);
                consume(TokenType::OPER_DOT);
                catch_all = true;
            } else {
                TypeNode* type = parseTypeName();
                type_name = type->name;
                delete type;
            }
        } else {
            catch_all = true;
        }
    }

    consume(TokenType::RPAREN);

    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body_stmts = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    return make_node_at<CatchClauseNode>(
        catch_line,
        std::move(var_name),
        std::move(type_name),
        catch_all,
        make_node_at<BlockStmtNode>(body_line, body_stmts)
    );
}

ASTNode* Parser::parseTryStmt() {
    LOG("Parsing try_stmt");
    const int try_line = current_token().line;
    consume(TokenType::KW_TRY);

    const int try_body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto try_stmts = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    while (match(TokenType::NEWLINE)) {
    }

    std::vector<CatchClauseNode*> catches;
    while (check(TokenType::KW_CATCH)) {
        catches.push_back(parseCatchClause());
        while (match(TokenType::NEWLINE)) {
        }
    }

    BlockStmtNode* else_body = nullptr;
    if (match(TokenType::KW_ELSE)) {
        const int else_line = current_token().line;
        consume(TokenType::LBRACE);
        push_context(ParserContext::FunctionBody);
        auto else_stmts = parseBlockStatementList();
        pop_context();
        consume(TokenType::RBRACE);
        else_body = make_node_at<BlockStmtNode>(else_line, else_stmts);
    }

    if (catches.empty()) {
        throw_error_at("try statement requires at least one catch clause", current_token());
    }

    return make_node_at<TryStmtNode>(
        try_line,
        make_node_at<BlockStmtNode>(try_body_line, try_stmts),
        std::move(catches),
        else_body
    );
}

ASTNode* Parser::parseIfStmt() {
    LOG("Parsing if_stmt");
    const int if_line = current_token().line;
    consume(TokenType::KW_IF);
    consume(TokenType::LPAREN);
    ExprNode* condition = parseExpression();
    consume(TokenType::RPAREN);
    const int then_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto then_body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    BlockStmtNode* else_body = nullptr;
    if (match(TokenType::KW_ELSE)) {
        const int else_line = current_token().line;
        consume(TokenType::LBRACE);
        push_context(ParserContext::FunctionBody);
        else_body = make_node_at<BlockStmtNode>(else_line, parseBlockStatementList());
        pop_context();
        consume(TokenType::RBRACE);
    }

    return make_node_at<IfStmtNode>(
        if_line,
        condition,
        make_node_at<BlockStmtNode>(then_line, then_body),
        else_body
    );
}

ASTNode* Parser::parseMatchStmt() {
    LOG("Parsing match_stmt");
    const int match_line = current_token().line;
    consume(TokenType::KW_MATCH);
    consume(TokenType::LPAREN);
    ExprNode* subject = parseExpression();
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);

    while (match(TokenType::NEWLINE)) {
    }

    std::vector<MatchCaseNode*> cases;
    while (check(TokenType::KW_CASE)) {
        const int case_line = current_token().line;
        consume(TokenType::KW_CASE);
        MatchPatternNode* pattern = parseMatchPattern();
        const int body_line = current_token().line;
        consume(TokenType::LBRACE);
        push_context(ParserContext::FunctionBody);
        auto body_stmts = parseBlockStatementList();
        pop_context();
        consume(TokenType::RBRACE);
        cases.push_back(make_node_at<MatchCaseNode>(
            case_line,
            pattern,
            make_node_at<BlockStmtNode>(body_line, body_stmts)
        ));

        while (match(TokenType::NEWLINE)) {
        }
    }

    while (match(TokenType::NEWLINE)) {
    }

    consume(TokenType::RBRACE);

    BlockStmtNode* else_body = nullptr;
    if (match(TokenType::KW_ELSE)) {
        const int else_line = current_token().line;
        consume(TokenType::LBRACE);
        push_context(ParserContext::FunctionBody);
        else_body = make_node_at<BlockStmtNode>(else_line, parseBlockStatementList());
        pop_context();
        consume(TokenType::RBRACE);
    }

    return make_node_at<MatchStmtNode>(match_line, subject, std::move(cases), else_body);
}

MatchPatternNode* Parser::parseMatchValuePattern() {
    const int pat_line = current_token().line;
    consume(TokenType::LPAREN);
    ExprNode* expr = parseExpression();
    consume(TokenType::RPAREN);
    return make_node_at<MatchPatternNode>(pat_line, MatchPatternKind::Expr, expr);
}

MatchPatternNode* Parser::parseMatchVectorElement() {
    if (check(TokenType::LBRACKET)) {
        return parseMatchVectorPattern();
    }

    if (check(TokenType::IDENTIFIER)) {
        const int elem_line = current_token().line;
        const std::string name = current_token().value;
        consume(TokenType::IDENTIFIER);
        return make_node_at<MatchPatternNode>(
            elem_line,
            MatchPatternKind::Bind,
            nullptr,
            name
        );
    }

    const int elem_line = current_token().line;
    ExprNode* expr = parseExpression();
    return make_node_at<MatchPatternNode>(elem_line, MatchPatternKind::Expr, expr);
}

MatchPatternNode* Parser::parseMatchVectorPattern() {
    const int pat_line = current_token().line;
    consume(TokenType::LBRACKET);

    while (match(TokenType::NEWLINE)) {
    }

    std::vector<MatchPatternNode*> elements;
    if (!check(TokenType::RBRACKET)) {
        while (true) {
            while (match(TokenType::NEWLINE)) {
            }

            elements.push_back(parseMatchVectorElement());

            while (match(TokenType::NEWLINE)) {
            }

            if (!match(TokenType::OPER_COMMA)) {
                break;
            }
        }
    }

    while (match(TokenType::NEWLINE)) {
    }

    consume(TokenType::RBRACKET);
    return make_node_at<MatchPatternNode>(
        pat_line,
        MatchPatternKind::Vector,
        nullptr,
        std::string{},
        std::move(elements)
    );
}

MatchPatternNode* Parser::parseMatchPatternUnit() {
    if (check(TokenType::LBRACKET)) {
        return parseMatchVectorPattern();
    }
    return parseMatchValuePattern();
}

MatchPatternNode* Parser::parseMatchPattern() {
    MatchPatternNode* first = parseMatchPatternUnit();
    std::vector<MatchPatternNode*> alternatives;
    alternatives.push_back(first);

    while (match(TokenType::OPER_BAR)) {
        alternatives.push_back(parseMatchPatternUnit());
    }

    if (alternatives.size() == 1) {
        return first;
    }

    return make_node_at<MatchPatternNode>(
        first->source_line,
        MatchPatternKind::Or,
        nullptr,
        std::string{},
        std::vector<MatchPatternNode*>{},
        std::move(alternatives)
    );
}

ASTNode* Parser::parseLoopStmt() {
    LOG("Parsing loop_stmt");
    const int loop_line = current_token().line;
    consume(TokenType::KW_LOOP);

    ExprNode* count_expr = nullptr;
    if (match(TokenType::LPAREN)) {
        count_expr = parseExpression();
        consume(TokenType::RPAREN);
    }

    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    return make_node_at<LoopNode>(
        loop_line,
        count_expr,
        make_node_at<BlockStmtNode>(body_line, body)
    );
}

ASTNode* Parser::parseWhileStmt() {
    LOG("Parsing while_stmt");
    const int while_line = current_token().line;
    consume(TokenType::KW_WHILE);
    consume(TokenType::LPAREN);
    ExprNode* condition = parseExpression();
    consume(TokenType::RPAREN);
    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    return make_node_at<WhileStmtNode>(
        while_line,
        condition,
        make_node_at<BlockStmtNode>(body_line, body)
    );
}

ASTNode* Parser::parseForLoopStmt() {
    LOG("Parsing for_loop_stmt");
    const int for_line = current_token().line;
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
    const int body_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto body = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    return make_node_at<ForLoopNode>(for_line, items, make_node_at<BlockStmtNode>(body_line, body));
}

ASTNode* Parser::parseBreakStmt() {
    LOG("Parsing break_stmt");
    const int break_line = current_token().line;
    consume(TokenType::KW_BREAK);
    return make_node_at<BreakNode>(break_line);
}

ASTNode* Parser::parseContinueStmt() {
    LOG("Parsing continue_stmt");
    const int continue_line = current_token().line;
    consume(TokenType::KW_CONTINUE);
    return make_node_at<ContinueNode>(continue_line);
}

ASTNode* Parser::parseBlockStmt() {
    LOG("Parsing block_stmt");
    const int block_line = current_token().line;
    consume(TokenType::LBRACE);
    push_context(ParserContext::FunctionBody);
    auto statements = parseBlockStatementList();
    pop_context();
    consume(TokenType::RBRACE);

    return make_node_at<BlockStmtNode>(block_line, statements);
}

ASTNode* Parser::parseImportStmt() {
    LOG("Parsing import_stmt");
    const int import_line = current_token().line;
    consume(TokenType::KW_IMPORT);
    auto name = parseQualifiedName();
    std::string alias;

    if (match(TokenType::KW_AS)) {
        alias = current_token().value;
        consume(TokenType::IDENTIFIER);
    }

    return make_node_at<ImportNode>(import_line, name, alias);
}

ASTNode* Parser::parseUseStmt() {
    LOG("Parsing use_stmt");
    const int use_line = current_token().line;
    consume(TokenType::KW_USE);

    std::vector<std::string> module_name;
    module_name.push_back(current_token().value);
    consume(TokenType::IDENTIFIER);

    while (check(TokenType::OPER_DOT)) {
        advance();
        if (check(TokenType::LBRACE)) {
            break;
        }
        if (!check(TokenType::IDENTIFIER)) {
            throw_error("expected identifier or '{' after '.' in use statement");
        }
        module_name.push_back(current_token().value);
        consume(TokenType::IDENTIFIER);
    }

    consume(TokenType::LBRACE);
    auto items = parseUseList();
    consume(TokenType::RBRACE);

    return make_node_at<UseNode>(use_line, module_name, items);
}

ExprNode* Parser::parseExpression() {
    return parsePipelineExpr();
}

bool Parser::atPipelineStepBoundary() const {
    if (!pipeline_step_mode_) {
        return false;
    }
    if (check(TokenType::NEWLINE) || check(TokenType::END)) {
        return true;
    }
    return check(TokenType::OPER_PIPE) && current_token().line == pipeline_step_line_;
}

ExprNode* Parser::parsePipelineExpr() {
    LOG("Parsing pipeline_expr");
    ExprNode* expr = parseComparisonExpr();

    while (true) {
        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE before pipeline step");
        }

        if (!check(TokenType::OPER_PIPE)) {
            break;
        }

        const Token pipe_tok = current_token();
        const int pipe_line = pipe_tok.line;
        advance();

        if (check(TokenType::NEWLINE) || check(TokenType::END)) {
            throw_error_at("pipeline operator '|>' requires an expression on the same line", pipe_tok);
        }
        if (current_token().line != pipe_line) {
            throw_error_at("pipeline operator '|>' and its expression must be on the same line", pipe_tok);
        }

        pipeline_step_mode_ = true;
        pipeline_step_line_ = pipe_line;
        ExprNode* step = parseComparisonExpr();
        pipeline_step_mode_ = false;

        if (!containsPlaceholder(step)) {
            throw_error_at("pipeline step must contain placeholder '_'", pipe_tok);
        }

        expr = substitutePipelinePlaceholder(step, expr);
    }

    return expr;
}

ExprNode* Parser::parseComparisonExpr() {
    LOG("Parsing comparison_expr");
    ExprNode* left = parseAdditiveExpr();

    while (!atPipelineStepBoundary()) {
        const TokenType op_type = current_token().type;
        if (op_type != TokenType::OPER_EQ && op_type != TokenType::OPER_NE &&
            op_type != TokenType::OPER_LT && op_type != TokenType::OPER_GT &&
            op_type != TokenType::OPER_LE && op_type != TokenType::OPER_GE) {
            break;
        }

        std::string op_str;
        switch (op_type) {
            case TokenType::OPER_EQ: op_str = "==";
                break;
            case TokenType::OPER_NE: op_str = "!=";
                break;
            case TokenType::OPER_LT: op_str = "<";
                break;
            case TokenType::OPER_GT: op_str = ">";
                break;
            case TokenType::OPER_LE: op_str = "<=";
                break;
            case TokenType::OPER_GE: op_str = ">=";
                break;
            default: ;
        }

        const int op_line = current_token().line;
        advance();
        ExprNode* right = parseAdditiveExpr();
        left = make_node_at<BinaryNode>(op_line, left, right, op_str);
    }

    return left;
}

ExprNode* Parser::parseAdditiveExpr() {
    LOG("Parsing additive_expr");
    ExprNode* left = parseMultiplicativeExpr();

    while (!atPipelineStepBoundary()) {
        const TokenType op_type = current_token().type;
        if (op_type != TokenType::OPER_PLUS && op_type != TokenType::OPER_MINUS) {
            break;
        }

        const std::string op_str = (op_type == TokenType::OPER_PLUS) ? "+" : "-";
        const int op_line = current_token().line;
        advance();
        ExprNode* right = parseMultiplicativeExpr();
        left = make_node_at<BinaryNode>(op_line, left, right, op_str);
    }

    return left;
}

ExprNode* Parser::parseMultiplicativeExpr() {
    LOG("Parsing multiplicative_expr");
    ExprNode* left = parseUnaryExpr();

    while (!atPipelineStepBoundary()) {
        TokenType op_type = current_token().type;
        if (op_type != TokenType::OPER_MUL && op_type != TokenType::OPER_DIV) {
            break;
        }

        std::string op_str = (op_type == TokenType::OPER_MUL) ? "*" : "/";
        const int op_line = current_token().line;
        advance();
        ExprNode* right = parseUnaryExpr();
        left = make_node_at<BinaryNode>(op_line, left, right, op_str);
    }

    return left;
}

ExprNode* Parser::parseUnaryExpr() {
    LOG("Parsing unary_expr");

    if (atPipelineStepBoundary()) {
        throw_error("expected pipeline step expression after '|>'");
    }

    if (check(TokenType::OPER_NOT)) {
        const int op_line = current_token().line;
        match(TokenType::OPER_NOT);
        ExprNode* operand = parseUnaryExpr();
        return make_node_at<UnaryNode>(op_line, "!", operand);
    }

    if (check(TokenType::OPER_MINUS)) {
        const int op_line = current_token().line;
        match(TokenType::OPER_MINUS);
        ExprNode* operand = parseUnaryExpr();
        return make_node_at<UnaryNode>(op_line, "-", operand);
    }

    if (check(TokenType::OPER_MUL)) {
        const int op_line = current_token().line;
        match(TokenType::OPER_MUL);
        ExprNode* operand = parseUnaryExpr();
        return make_node_at<UnaryNode>(op_line, "*", operand);
    }

    if (check(TokenType::OPER_AMP)) {
        const int op_line = current_token().line;
        match(TokenType::OPER_AMP);
        ExprNode* operand = parseUnaryExpr();
        return make_node_at<UnaryNode>(op_line, "&", operand);
    }

    return parsePostfixExpr();
}

ExprNode* Parser::parsePostfixExpr(const bool parse_do_suffix) {
    LOG("Parsing postfix_expr");
    ExprNode* expr = parseFactor();

    while (true) {
        if (atPipelineStepBoundary()) {
            break;
        }

        if (check(TokenType::LPAREN)) {
            const int line = current_token().line;
            match(TokenType::LPAREN);
            skipNewlines();
            auto args = parseArgList();
            skipNewlines();
            consume(TokenType::RPAREN);
            expr = make_node_at<FuncCallExprNode>(line, expr, args);
        } else if (check(TokenType::OPER_DOT)) {
            const int line = current_token().line;
            match(TokenType::OPER_DOT);
            if (check(TokenType::LPAREN)) {
                match(TokenType::LPAREN);
                skipNewlines();
                ExprNode* value = parseExpression();
                skipNewlines();
                consume(TokenType::RPAREN);
                expr = make_node_at<TypeConvertExprNode>(line, expr, value);
            } else {
                std::string name = current_token().value;
                consume(TokenType::IDENTIFIER);
                expr = make_node_at<MemberAccessNode>(line, expr, name);
            }
        } else if (check(TokenType::LBRACKET)) {
            const int line = current_token().line;
            match(TokenType::LBRACKET);
            ExprNode* index = pipeline_step_mode_ ? parseComparisonExpr() : parseExpression();
            consume(TokenType::RBRACKET);
            expr = make_node_at<IndexAccessNode>(line, expr, index);
        } else if (parse_do_suffix && check(TokenType::KW_DO)) {
            std::vector<ExprNode*> decorators;
            decorators.push_back(expr);
            consume(TokenType::KW_DO);
            expr = finishDoFuncDecl(std::move(decorators));
        } else {
            break;
        }
    }

    return expr;
}

ExprNode* Parser::parseFactor() {
    LOG("Parsing factor");

    if (check(TokenType::NUM_LITERAL)) {
        const int line = current_token().line;
        std::string value = current_token().value;
        advance();
        return make_node_at<NumberNode>(line, value);
    }

    if (check(TokenType::STRING_LITERAL)) {
        const int line = current_token().line;
        std::string value = current_token().value;
        advance();
        return make_node_at<StringNode>(line, value);
    }

    if (check(TokenType::PLACEHOLDER)) {
        if (!pipeline_step_mode_) {
            throw_error("placeholder '_' is only valid in pipeline steps");
        }
        const int line = current_token().line;
        advance();
        return make_node_at<PlaceholderNode>(line);
    }

    if (check(TokenType::IDENTIFIER)) {
        const int line = current_token().line;
        std::string value = current_token().value;
        advance();
        return make_node_at<VarRefNode>(line, value);
    }

    if (match(TokenType::LPAREN)) {
        skipNewlines();
        ExprNode* expr = pipeline_step_mode_ ? parseComparisonExpr() : parseExpression();
        skipNewlines();
        consume(TokenType::RPAREN);
        return expr;
    }

    if (check(TokenType::KW_VEC) || check(TokenType::LBRACKET)) {
        const int vec_line = current_token().line;
        match(TokenType::KW_VEC);
        consume(TokenType::LBRACKET);
        push_context(ParserContext::Vec);

        while (match(TokenType::NEWLINE)) {
        }

        if (check(TokenType::RBRACKET)) {
            pop_context();
            consume(TokenType::RBRACKET);
            return make_node_at<VectorNode>(vec_line, std::vector<ASTNode*>{});
        }

        ExprNode* first = parseExpression();

        while (match(TokenType::NEWLINE)) {
        }

        if (match(TokenType::KW_FOR)) {
            ExprNode* comp = finishComprehensionAfterFor(first, vec_line);
            while (match(TokenType::NEWLINE)) {
            }
            pop_context();
            consume(TokenType::RBRACKET);
            return comp;
        }

        std::vector<ASTNode*> elements;
        elements.push_back(first);

        while (match(TokenType::OPER_COMMA)) {
            while (match(TokenType::NEWLINE)) {
            }
            elements.push_back(parseExpression());
        }

        while (match(TokenType::NEWLINE)) {
        }

        pop_context();
        consume(TokenType::RBRACKET);
        return make_node_at<VectorNode>(vec_line, elements);
    }

    if (check(TokenType::LBRACE)) {
        const int dict_line = current_token().line;
        match(TokenType::LBRACE);
        push_context(ParserContext::Dict);
        auto entries = parseDictEntryList();
        pop_context();
        consume(TokenType::RBRACE);
        return make_node_at<DictionaryNode>(dict_line, entries);
    }

    if (check(TokenType::KW_WITH)) {
        const ParserState saved = save_state();
        parseWithDecoratorList();
        const bool is_do = check(TokenType::KW_DO);
        restore_state(saved);
        if (is_do) {
            return dynamic_cast<ExprNode*>(parseDecoratedDoDecl());
        }
    }

    if (check(TokenType::KW_DO)) {
        const int do_line = current_token().line;
        match(TokenType::KW_DO);
        return finishDoFuncDecl({}, do_line);
    }

    Token tok = current_token();
    throw_error_at(
        "expected expression, found " + (tok.value.empty() ? getTokenTypeName(tok.type) : "'" + tok.value + "'"),
        tok
    );
    return nullptr;
}

ExprNode* Parser::parseDoExpr() {
    return dynamic_cast<ExprNode*>(parseDecoratedDoDecl());
}

std::vector<FuncParam> Parser::parseParamList() {
    LOG("Parsing param_list");
    std::vector<FuncParam> params;

    skipNewlines();
    if (check(TokenType::RPAREN)) {
        return params;
    }

    std::string name = current_token().value;
    consume(TokenType::IDENTIFIER);

    std::string type_name;
    bool has_type = false;
    if (match(TokenType::OPER_COLON)) {
        TypeNode* param_type = parseTypeName();
        type_name = param_type->name;
        delete param_type;
        has_type = true;
    }

    ExprNode* default_value = nullptr;
    if (match(TokenType::ASSIGN)) {
        default_value = parseExpression();
    }
    params.emplace_back(std::move(name), default_value, std::move(type_name), has_type);

    while (match(TokenType::OPER_COMMA)) {
        skipNewlines();
        name = current_token().value;
        consume(TokenType::IDENTIFIER);
        type_name.clear();
        has_type = false;
        if (match(TokenType::OPER_COLON)) {
            TypeNode* param_type = parseTypeName();
            type_name = param_type->name;
            delete param_type;
            has_type = true;
        }
        default_value = nullptr;
        if (match(TokenType::ASSIGN)) {
            default_value = parseExpression();
        }
        params.emplace_back(std::move(name), default_value, std::move(type_name), has_type);
    }

    skipNewlines();
    return params;
}

std::vector<CallArgument> Parser::parseArgList() {
    LOG("Parsing arg_list");
    std::vector<CallArgument> args;

    skipNewlines();
    if (check(TokenType::RPAREN)) {
        return args;
    }

    while (true) {
        skipNewlines();
        if (check(TokenType::IDENTIFIER)) {
            const ParserState saved = save_state();
            const std::string name = current_token().value;
            advance();
            if (match(TokenType::ASSIGN)) {
                args.emplace_back(parseExpression(), name);
            } else {
                restore_state(saved);
                args.emplace_back(parseExpression());
            }
        } else {
            args.emplace_back(parseExpression());
        }

        if (!match(TokenType::OPER_COMMA)) {
            break;
        }
        skipNewlines();
    }

    skipNewlines();
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

    const int entry_line = current_token().line;
    ExprNode* key = parseExpression();

    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in dict after key");
    }

    consume(TokenType::OPER_COLON);

    while (match(TokenType::NEWLINE)) {
        LOG("Skipping NEWLINE in dict after colon");
    }

    ExprNode* value = parseExpression();
    entries.push_back(make_node_at<DictEntryNode>(entry_line, key, value));

    while (match(TokenType::OPER_COMMA)) {
        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in dict after comma");
        }

        const int loop_entry_line = current_token().line;
        key = parseExpression();

        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in dict after key in loop");
        }

        consume(TokenType::OPER_COLON);

        while (match(TokenType::NEWLINE)) {
            LOG("Skipping NEWLINE in dict after colon in loop");
        }

        value = parseExpression();
        entries.push_back(make_node_at<DictEntryNode>(loop_entry_line, key, value));
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