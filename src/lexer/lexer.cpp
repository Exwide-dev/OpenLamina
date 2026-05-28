#include "lexer.hpp"
#include <unordered_map>
#include <sstream>

namespace lmx {

Lexer::Lexer(const std::string& fname) 
    : filename(fname), full_source(""), pos(0), line(1), column(1) {}

void Lexer::add_input(const std::string& source) {
    full_source = source;
    std::istringstream iss(source);
    std::string line_str;
    source_lines.clear();
    while (std::getline(iss, line_str)) {
        source_lines.push_back(line_str);
    }
    pos = 0;
    line = 1;
    column = 1;
}

std::string Lexer::getTokenTypeName(TokenType type) {
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
        case TokenType::OPER_PLUS: return "+";
        case TokenType::OPER_MINUS: return "-";
        case TokenType::OPER_MUL: return "*";
        case TokenType::OPER_DIV: return "/";
        case TokenType::OPER_NOT: return "!";
        case TokenType::OPER_EQ: return "==";
        case TokenType::OPER_NE: return "!=";
        case TokenType::OPER_LT: return "<";
        case TokenType::OPER_GT: return ">";
        case TokenType::OPER_LE: return "<=";
        case TokenType::OPER_GE: return ">=";
        case TokenType::OPER_COMMA: return ",";
        case TokenType::OPER_DOT: return ".";
        case TokenType::OPER_COLON: return ":";
        case TokenType::ASSIGN: return "=";
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::LBRACE: return "{";
        case TokenType::RBRACE: return "}";
        case TokenType::LBRACKET: return "[";
        case TokenType::RBRACKET: return "]";
        case TokenType::NEWLINE: return "NEWLINE";
        default: return "UNKNOWN";
    }
}

bool Lexer::isAtEnd() const {
    return pos >= full_source.size();
}

char Lexer::peekChar() const {
    if (isAtEnd()) return '\0';
    return full_source[pos];
}

char Lexer::consumeChar() {
    char c = full_source[pos++];
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
            return Token(TokenType::END, value, start_line, start_col);
        } else {
            value += consumeChar();
        }
    }
    
    if (isAtEnd()) {
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
            consumeChar();
            return Token(TokenType::END, std::string(1, c), start_line, start_col);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        while (!isAtEnd() && isWhitespace(peekChar())) {
            consumeChar();
        }
        
        if (isAtEnd()) {
            break;
        }
        
        char c = peekChar();
        
        if (isLetter(c)) {
            tokens.push_back(parseIdentifier());
        } else if (isDigit(c)) {
            tokens.push_back(parseNumber());
        } else if (c == '"') {
            tokens.push_back(parseString());
        } else {
            tokens.push_back(parseOperator());
        }
    }
    
    tokens.push_back(Token(TokenType::END, "", line, column));
    
    return tokens;
}

} // namespace lmx
