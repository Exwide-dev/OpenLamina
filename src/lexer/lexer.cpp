#include "lexer.hpp"
#include <unordered_map>

namespace lmx {

Lexer::Lexer(const std::string& source) 
    : source(source), pos(0), line(1), column(1), 
      current_token(TokenType::END, "", 1, 1) {}

bool Lexer::isAtEnd() const {
    return pos >= source.size();
}

char Lexer::peekChar() const {
    if (isAtEnd()) return '\0';
    return source[pos];
}

char Lexer::consumeChar() {
    char c = source[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

Token Lexer::parseIdentifier() {
    std::string value;
    int start_line = line;
    int start_col = column;
    
    while (!isAtEnd() && (isLetter(peekChar()) || isDigit(peekChar()))) {
        value += consumeChar();
    }
    
    static std::unordered_map<std::string, TokenType> keywords = {
        {"let", TokenType::KW_LET},
        {"func", TokenType::KW_FUNC},
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
        {"make", TokenType::KW_MAKE}
    };
    
    auto it = keywords.find(value);
    if (it != keywords.end()) {
        return Token(it->second, value, start_line, start_col);
    }
    
    return Token(TokenType::IDENTIFIER, value, start_line, start_col);
}

Token Lexer::parseNumber() {
    std::string value;
    int start_line = line;
    int start_col = column;
    
    while (!isAtEnd() && isDigit(peekChar())) {
        value += consumeChar();
    }
    
    return Token(TokenType::NUM_LITERAL, value, start_line, start_col);
}

Token Lexer::parseString() {
    std::string value;
    int start_line = line;
    int start_col = column;
    
    consumeChar();
    
    while (!isAtEnd() && peekChar() != '"') {
        if (peekChar() == '\\') {
            consumeChar();
            if (!isAtEnd()) {
                switch (peekChar()) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '"': value += '"'; break;
                    case '\\': value += '\\'; break;
                    default: value += peekChar(); break;
                }
                consumeChar();
            }
        } else if (peekChar() == '\n') {
            details = "Lexer SyntaxError at line " + std::to_string(line) + ": unclosed string literal";
            return Token(TokenType::END, value, start_line, start_col);
        } else {
            value += consumeChar();
        }
    }
    
    if (isAtEnd()) {
        details = "Lexer SyntaxError at line " + std::to_string(line) + ": unclosed string literal at end of file";
        return Token(TokenType::END, value, start_line, start_col);
    }
    
    consumeChar();
    return Token(TokenType::STRING_LITERAL, value, start_line, start_col);
}

Token Lexer::parseOperator() {
    char c = peekChar();
    int start_line = line;
    int start_col = column;
    
    switch (c) {
        case '+': consumeChar(); return Token(TokenType::OPER_PLUS, "+", start_line, start_col);
        case '-': consumeChar(); return Token(TokenType::OPER_MINUS, "-", start_line, start_col);
        case '*': consumeChar(); return Token(TokenType::OPER_MUL, "*", start_line, start_col);
        case '/': consumeChar(); return Token(TokenType::OPER_DIV, "/", start_line, start_col);
        case '!': 
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenType::OPER_NE, "!=", start_line, start_col);
            }
            return Token(TokenType::OPER_NOT, "!", start_line, start_col);
        case '=': 
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenType::OPER_EQ, "==", start_line, start_col);
            }
            return Token(TokenType::ASSIGN, "=", start_line, start_col);
        case '<': 
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenType::OPER_LE, "<=", start_line, start_col);
            }
            return Token(TokenType::OPER_LT, "<", start_line, start_col);
        case '>': 
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenType::OPER_GE, ">=", start_line, start_col);
            }
            return Token(TokenType::OPER_GT, ">", start_line, start_col);
        case ',': consumeChar(); return Token(TokenType::OPER_COMMA, ",", start_line, start_col);
        case '.': consumeChar(); return Token(TokenType::OPER_DOT, ".", start_line, start_col);
        case ':': consumeChar(); return Token(TokenType::OPER_COLON, ":", start_line, start_col);
        case '(': consumeChar(); return Token(TokenType::LPAREN, "(", start_line, start_col);
        case ')': consumeChar(); return Token(TokenType::RPAREN, ")", start_line, start_col);
        case '{': consumeChar(); return Token(TokenType::LBRACE, "{", start_line, start_col);
        case '}': consumeChar(); return Token(TokenType::RBRACE, "}", start_line, start_col);
        case '[': consumeChar(); return Token(TokenType::LBRACKET, "[", start_line, start_col);
        case ']': consumeChar(); return Token(TokenType::RBRACKET, "]", start_line, start_col);
        case '\n': consumeChar(); return Token(TokenType::NEWLINE, "\n", start_line, start_col);
        default: 
            details = "Lexer SyntaxError at line " + std::to_string(line) + ": unknown character '" + c + "'";
            consumeChar();
            return Token(TokenType::END, std::string(1, c), start_line, start_col);
    }
}

Token Lexer::nextToken() {
    while (!isAtEnd() && isWhitespace(peekChar())) {
        consumeChar();
    }
    
    if (isAtEnd()) {
        return Token(TokenType::END, "", line, column);
    }
    
    char c = peekChar();
    
    if (isLetter(c)) {
        return parseIdentifier();
    } else if (isDigit(c)) {
        return parseNumber();
    } else if (c == '"') {
        return parseString();
    } else {
        return parseOperator();
    }
}

Token Lexer::peek() const {
    return current_token;
}

void Lexer::consume() {
    current_token = nextToken();
}

Token Lexer::peekNext() {
    size_t saved_pos = pos;
    int saved_line = line;
    int saved_col = column;
    
    while (!isAtEnd() && peekChar() == '\n') {
        consumeChar();
    }
    
    while (!isAtEnd() && isWhitespace(peekChar())) {
        consumeChar();
    }
    
    Token result = nextToken();
    
    pos = saved_pos;
    line = saved_line;
    column = saved_col;
    
    return result;
}

} // namespace lmx