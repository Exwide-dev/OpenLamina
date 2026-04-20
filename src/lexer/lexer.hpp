#pragma once

#include <vector>
#include <string>
#include "token.hpp"

namespace lexer {

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();
    
private:
    std::string source;
    int line;
    int column;
    size_t position;
    
    char peek();
    char advance();
    bool isAtEnd();
    void skipWhitespace();
    Token scanToken();
    Token makeToken(TokenType type, const std::string& value);
    Token makeUnknownToken(char c);
    std::string scanString();
    std::string scanNumber();
    std::string scanIdentifier();
    TokenType checkKeyword(const std::string& identifier);
};

} // namespace lexer

