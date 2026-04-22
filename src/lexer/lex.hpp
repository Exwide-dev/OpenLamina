#pragma once

#include <vector>
#include <string>
#include "token.hpp"

namespace lexer {

class Lexer {
    size_t line{0}, column{0};
    std::string source_code;
public:
    explicit Lexer(std::string source_code): source_code(std::move(source_code)) {};
    ~Lexer() = default;
    
    [[nodiscard]] std::vector<Token> tokenize() const;
};

} // namespace lexer
