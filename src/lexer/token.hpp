#pragma once

#include <string>
#include "parser.tab.hpp" // Bison 生成的头文件，包含 Token 类型宏

namespace lexer {
    // 使用 Bison 生成的 Token 类型
    using TokenType = int;
    
    // Token 结构体
    struct Token {
        TokenType type;
        std::string value;
        int line;
        int column;
        
        Token(TokenType type, const std::string& value, int line, int column)
            : type(type), value(value), line(line), column(column) {}
    };

    // 定义 Token 类型到字符串的映射
    inline std::string tokenTypeToString(TokenType type) {
        switch (type) {
            case OPER_PLUS: return "OPER_PLUS";
            case OPER_MINUS: return "OPER_MINUS";
            case OPER_MUL: return "OPER_MUL";
            case OPER_DIV: return "OPER_DIV";
            case ASSIGN: return "ASSIGN";
            case LPAREN: return "LPAREN";
            case RPAREN: return "RPAREN";
            case KW_LET: return "KW_LET";
            case NUM_LITERAL: return "NUM_LITERAL";
            case IDENTIFIER: return "IDENTIFIER";
            case 0: return "EOF";
            default: return "UNKNOWN";
        }
    }
}
