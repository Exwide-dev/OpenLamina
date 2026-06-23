#include "lexer.hpp"
#include "utf8.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "../../tools/error.hpp"
#include "../../tools/debug.hpp"

namespace lmx {
Lexer::Lexer(std::string filename)
    : filename(std::move(filename)) {
    pos = 0;
    line = 1;
    column = 1;
}

void Lexer::add_input(const std::string& source) {
    full_source.append(source);
    std::istringstream iss(source);
    std::string line_str;
    while (std::getline(iss, line_str)) {
        source_lines.push_back(line_str);
    }
}

void Lexer::skipBom() {
    if (pos != 0) {
        return;
    }
    const size_t bom = utf8::bom_length(full_source);
    if (bom > 0) {
        pos = bom;
        column = 1;
    }
}

bool Lexer::identifierStartsHere() const {
    return utf8::identifier_start_at(full_source, pos);
}

bool Lexer::identifierContinuesHere() const {
    return utf8::identifier_continue_at(full_source, pos);
}

size_t Lexer::consumeCodepoint() {
    if (isAtEnd()) {
        return 0;
    }

    const auto decoded = utf8::decode(full_source, pos);
    if (!decoded.ok) {
        consumeChar();
        return 1;
    }

    if (decoded.codepoint == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }

    pos += decoded.bytes;
    return decoded.bytes;
}

std::string Lexer::getTokenTypeName(const TokenType type) {
    switch (type) {
        case TokenType::END: return "END";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUM_LITERAL: return "NUM_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::KW_LET: return "let";
        case TokenType::KW_FUNC: return "func";
        case TokenType::KW_DO: return "do";
        case TokenType::KW_RETURN: return "return";
        case TokenType::KW_IF: return "if";
        case TokenType::KW_ELSE: return "else";
        case TokenType::KW_LOOP: return "loop";
        case TokenType::KW_WHILE: return "while";
        case TokenType::KW_BREAK: return "break";
        case TokenType::KW_CONTINUE: return "continue";
        case TokenType::KW_IMPORT: return "import";
        case TokenType::KW_USE: return "use";
        case TokenType::KW_AS: return "as";
        case TokenType::KW_VEC: return "vec";
        case TokenType::KW_CONST: return "const";
        case TokenType::KW_VAR: return "var";
        case TokenType::KW_INTERN: return "intern";
        case TokenType::KW_EXPORT: return "export";
        case TokenType::KW_WITH: return "with";
        case TokenType::KW_MAKE: return "make";
        case TokenType::KW_FOR: return "for";
        case TokenType::KW_IN: return "in";
        case TokenType::KW_STRUCT: return "struct";
        case TokenType::KW_TYPED: return "typed";
        case TokenType::KW_MATCH: return "match";
        case TokenType::KW_CASE: return "case";
        case TokenType::KW_TRY: return "try";
        case TokenType::KW_CATCH: return "catch";
        case TokenType::KW_THROW: return "throw";
        case TokenType::PLACEHOLDER: return "_";
        case TokenType::OPER_PLUS: return "+";
        case TokenType::OPER_MINUS: return "-";
        case TokenType::OPER_MUL: return "*";
        case TokenType::OPER_DIV: return "/";
        case TokenType::OPER_NOT: return "!";
        case TokenType::OPER_AMP: return "&";
        case TokenType::OPER_EQ: return "==";
        case TokenType::OPER_NE: return "!=";
        case TokenType::OPER_LT: return "<";
        case TokenType::OPER_GT: return ">";
        case TokenType::OPER_LE: return "<=";
        case TokenType::OPER_GE: return ">=";
        case TokenType::OPER_COMMA: return ",";
        case TokenType::OPER_DOT: return ".";
        case TokenType::OPER_COLON: return ":";
        case TokenType::OPER_ARROW: return "->";
        case TokenType::OPER_PIPE: return "|>";
        case TokenType::OPER_BAR: return "|";
        case TokenType::ASSIGN: return "=";
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::LBRACE: return "{";
        case TokenType::RBRACE: return "}";
        case TokenType::LBRACKET: return "[";
        case TokenType::RBRACKET: return "]";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::MISMATCH: return "MISMATCH";
        default: return "UNKNOWN";
    }
}

std::vector<TokenPattern> Lexer::initPatterns() {
    return {
        {R"(==)", TokenType::OPER_EQ},
        {R"(!=)", TokenType::OPER_NE},
        {R"(<=)", TokenType::OPER_LE},
        {R"(>=)", TokenType::OPER_GE},
        {R"(\|>)", TokenType::OPER_PIPE},
        {R"(\|)", TokenType::OPER_BAR},
        {R"(->)", TokenType::OPER_ARROW},
        {R"(\+)", TokenType::OPER_PLUS},
        {R"(-)", TokenType::OPER_MINUS},
        {R"(\*)", TokenType::OPER_MUL},
        {R"(/)", TokenType::OPER_DIV},
        {R"(!)", TokenType::OPER_NOT},
        {R"(&)", TokenType::OPER_AMP},
        {R"(<)", TokenType::OPER_LT},
        {R"(>)", TokenType::OPER_GT},
        {R"(,)", TokenType::OPER_COMMA},
        {R"(\.)", TokenType::OPER_DOT},
        {R"(:)", TokenType::OPER_COLON},
        {R"(=)", TokenType::ASSIGN},
        {R"(\()", TokenType::LPAREN},
        {R"(\))", TokenType::RPAREN},
        {R"(\{)", TokenType::LBRACE},
        {R"(\})", TokenType::RBRACE},
        {R"(\[)", TokenType::LBRACKET},
        {R"(\])", TokenType::RBRACKET},
    };
}

bool Lexer::isAtEnd() const {
    return pos >= full_source.size();
}

char Lexer::peekChar() const {
    if (isAtEnd()) return '\0';
    return full_source[pos];
}

char Lexer::consumeChar() {
    const char c = full_source[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    } else if (static_cast<unsigned char>(c) < 0x80) {
        column++;
    }
    return c;
}

Token Lexer::parseString() {
    std::string value;
    const int start_line = line;
    const int start_col = column;

    consumeChar();

    while (!isAtEnd() && peekChar() != '"') {
        if (peekChar() == '\\') {
            consumeChar();
            if (!isAtEnd()) {
                switch (peekChar()) {
                    case 'n': value += '\n';
                        consumeChar();
                        break;
                    case 't': value += '\t';
                        consumeChar();
                        break;
                    case 'r': value += '\r';
                        consumeChar();
                        break;
                    case '"': value += '"';
                        consumeChar();
                        break;
                    case '\\': value += '\\';
                        consumeChar();
                        break;
                    default: {
                        const size_t before = pos;
                        consumeCodepoint();
                        value.append(full_source, before, pos - before);
                        break;
                    }
                }
            }
        } else if (peekChar() == '\n') {
            return {TokenType::MISMATCH, value, start_line, start_col};
        } else {
            const size_t before = pos;
            consumeCodepoint();
            value.append(full_source, before, pos - before);
        }
    }

    if (isAtEnd()) {
        return {TokenType::MISMATCH, value, start_line, start_col};
    }

    consumeChar();
    return {TokenType::STRING_LITERAL, value, start_line, start_col};
}

bool Lexer::skipComment() {
    if (peekChar() != '/') {
        return false;
    }
    if (pos + 1 >= full_source.size()) {
        return false;
    }
    if (full_source[pos + 1] == '/') {
        consumeChar();
        consumeChar();
        while (!isAtEnd() && peekChar() != '\n') {
            consumeChar();
        }
        return true;
    }
    if (full_source[pos + 1] == '*') {
        consumeChar();
        consumeChar();
        while (!isAtEnd()) {
            if (peekChar() == '*' && pos + 1 < full_source.size() && full_source[pos + 1] == '/') {
                consumeChar();
                consumeChar();
                return true;
            }
            consumeChar();
        }
        return true;
    }
    return false;
}

Token Lexer::parseNewline() {
    const int start_line = line;
    const int start_col = column;
    consumeChar();
    return Token(TokenType::NEWLINE, "\n", start_line, start_col);
}

Token Lexer::parseIdentifierOrKeyword() {
    const int start_line = line;
    const int start_col = column;
    std::string value;

    while (!isAtEnd() && identifierContinuesHere()) {
        const size_t before = pos;
        consumeCodepoint();
        value.append(full_source, before, pos - before);
    }

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"let", TokenType::KW_LET},
        {"func", TokenType::KW_FUNC},
        {"friend", TokenType::KW_FRIEND},
        {"do", TokenType::KW_DO},
        {"return", TokenType::KW_RETURN},
        {"if", TokenType::KW_IF},
        {"else", TokenType::KW_ELSE},
        {"loop", TokenType::KW_LOOP},
        {"while", TokenType::KW_WHILE},
        {"break", TokenType::KW_BREAK},
        {"continue", TokenType::KW_CONTINUE},
        {"import", TokenType::KW_IMPORT},
        {"use", TokenType::KW_USE},
        {"as", TokenType::KW_AS},
        {"vec", TokenType::KW_VEC},
        {"const", TokenType::KW_CONST},
        {"var", TokenType::KW_VAR},
        {"intern", TokenType::KW_INTERN},
        {"export", TokenType::KW_EXPORT},
        {"with", TokenType::KW_WITH},
        {"make", TokenType::KW_MAKE},
        {"for", TokenType::KW_FOR},
        {"in", TokenType::KW_IN},
        {"struct", TokenType::KW_STRUCT},
        {"typed", TokenType::KW_TYPED},
        {"match", TokenType::KW_MATCH},
        {"case", TokenType::KW_CASE},
        {"try", TokenType::KW_TRY},
        {"catch", TokenType::KW_CATCH},
        {"throw", TokenType::KW_THROW},
        {"_", TokenType::PLACEHOLDER},
    };

    const auto it = keywords.find(value);
    if (it != keywords.end()) {
        return {it->second, value, start_line, start_col};
    }

    return {TokenType::IDENTIFIER, value, start_line, start_col};
}

Token Lexer::tryMatchPatterns() {
    const int start_line = line;
    const int start_col = column;

    if (identifierStartsHere()) {
        return parseIdentifierOrKeyword();
    }

    struct Candidate {
        TokenType type;
        std::string value;
        size_t length = 0;
    };
    std::optional<Candidate> best;

    const std::string remaining(full_source.substr(pos));
    for (const auto& pattern : initPatterns()) {
        const LexerState saved = save_state();
        std::smatch match;
        if (std::regex_search(remaining, match, pattern.regex) && match.position() == 0) {
            const std::string matched = match.str();
            if (!best || matched.length() > best->length) {
                best = Candidate{pattern.type, matched, matched.length()};
            }
        }
        restore_state(saved);
    }

    if (best) {
        for (size_t i = 0; i < best->length; ++i) {
            consumeChar();
        }
        return {best->type, best->value, start_line, start_col};
    }

    if (utf8::ascii_digit_at(full_source, pos)) {
        LOG("Find num");
        std::string value;
        while (!isAtEnd() && utf8::ascii_digit_at(full_source, pos)) {
            const size_t before = pos;
            consumeCodepoint();
            value.append(full_source, before, pos - before);
        }
        return {TokenType::NUM_LITERAL, value, start_line, start_col};
    }

    if (!isAtEnd()) {
        const size_t before = pos;
        const auto decoded = utf8::decode(full_source, pos);
        if (decoded.ok) {
            consumeCodepoint();
            std::string bad(full_source, before, pos - before);
            return {TokenType::MISMATCH, bad, start_line, start_col};
        }
        const char c = consumeChar();
        return {TokenType::MISMATCH, std::string(1, c), start_line, start_col};
    }

    return {TokenType::MISMATCH, "", start_line, start_col};
}

std::vector<Token> Lexer::tokenize() {
    skipBom();
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        while (!isAtEnd() && isWhitespace(peekChar())) {
            consumeChar();
        }

        if (isAtEnd()) {
            break;
        }

        if (skipComment()) {
            continue;
        }

        const char c = peekChar();

        if (c == '"') {
            tokens.push_back(parseString());
        } else if (c == '\n') {
            tokens.push_back(parseNewline());
        } else {
            tokens.push_back(tryMatchPatterns());
        }
    }

    tokens.emplace_back(TokenType::END, "", line, column);
    return tokens;
}

void Lexer::add_error(const std::string& message) {
    errors.emplace_back(message, line, column);
}

std::vector<Token> Lexer::lex_rest() {
    skipBom();
    clear_errors();
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        while (!isAtEnd() && isWhitespace(peekChar())) {
            consumeChar();
        }

        if (isAtEnd()) {
            break;
        }

        if (skipComment()) {
            continue;
        }

        const char c = peekChar();

        if (c == '"') {
            Token tok = parseString();
            tokens.push_back(tok);
            if (tok.type == TokenType::MISMATCH) {
                add_error("Unclosed or invalid string literal");
            }
        } else if (c == '\n') {
            tokens.push_back(parseNewline());
        } else {
            Token tok = tryMatchPatterns();
            tokens.push_back(tok);
            if (tok.type == TokenType::MISMATCH) {
                add_error("Invalid token: '" + tok.value + "'");
            }
        }
    }

    tokens.emplace_back(TokenType::END, "", line, column);

    if (!errors.empty()) {
        std::string error_msg;
        for (const auto& err : errors) {
            if (!error_msg.empty()) error_msg += "\n";
            error_msg += "Lexer error at line " + std::to_string(err.line) +
                    ", column " + std::to_string(err.column) + ": " + err.message;
        }
        throw SyntaxError(error_msg);
    }

    return tokens;
}
} // namespace lmx
