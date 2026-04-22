// src/lexer/lex.cpp
#include "lex.hpp"
#include "token.hpp"
#include "../../lexer_generated.h"

namespace lexer {
    std::vector<Token> lex(const std::string& source);
    std::vector<Token> Lexer::tokenize() const {
        return lex(source_code);
    }

} // namespace lexer