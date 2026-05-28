#pragma once

#include <string>
#include <utility>
#include <vector>
#include <cctype>

namespace lmx {

enum class TokenType {
    END,
    IDENTIFIER,
    NUM_LITERAL,
    STRING_LITERAL,
    
    KW_LET,
    KW_FUNC,
    KW_DO,
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_LOOP,
    KW_WHILE,
    KW_BREAK,
    KW_CONTINUE,
    KW_IMPORT,
    KW_USE,
    KW_AS,
    KW_VEC,
    KW_CONST,
    KW_VAR,
    KW_INTERN,
    KW_EXPORT,
    KW_WITH,
    KW_MAKE,
    
    OPER_PLUS,
    OPER_MINUS,
    OPER_MUL,
    OPER_DIV,
    OPER_NOT,
    OPER_EQ,
    OPER_NE,
    OPER_LT,
    OPER_GT,
    OPER_LE,
    OPER_GE,
    OPER_COMMA,
    OPER_DOT,
    OPER_COLON,
    ASSIGN,
    
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    NEWLINE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
    
    Token(const TokenType t, std::string v, const int l, const int c)
        : type(t), value(std::move(v)), line(l), column(c) {}
};

class Lexer {
public:
    explicit Lexer(const std::string& filename = "");
    
    void add_input(const std::string& source);
    std::vector<Token> tokenize();
    
    [[nodiscard]] const std::string& get_filename() const { return filename; }
    [[nodiscard]] const std::vector<std::string>& get_source_lines() const { return source_lines; }

private:
    std::string filename;
    std::string full_source;
    std::vector<std::string> source_lines;
    
    size_t pos = 0;
    int line = 1;
    int column = 1;
    
    static std::string getTokenTypeName(TokenType type);
    
    [[nodiscard]] char peekChar() const;
    char consumeChar();
    [[nodiscard]] bool isAtEnd() const;
    
    Token parseIdentifier();
    Token parseNumber();
    Token parseString();
    Token parseOperator();
    
    [[nodiscard]] bool isDigit(const char c) const {
        const auto uc = static_cast<unsigned char>(c);
        return uc >= '0' && uc <= '9'; 
    }
    [[nodiscard]] bool isLetter(const char c) const {
        const auto uc = static_cast<unsigned char>(c);
        return (uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') || uc == '_'; 
    }
    [[nodiscard]] bool isWhitespace(const char c) const {
        const auto uc = static_cast<unsigned char>(c);
        return (uc == ' ' || uc == '\t' || uc == '\r'); 
    }
    [[nodiscard]] bool isNewline(const char c) const {
        return c == '\n';
    }
};

} // namespace lmx
