#pragma once

#include <string>
#include <utility>
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
        
        Token(const TokenType type, std::string  value, const int line, const int column)
            : type(type), value(std::move(value)), line(line), column(column) {}
    };

    // 定义 Token 类型到字符串的映射
    inline std::string tokenTypeToString(TokenType type) {
        switch (type) {
            case OPER_PLUS: return "OPER_PLUS";
            case OPER_MINUS: return "OPER_MINUS";
            case OPER_MUL: return "OPER_MUL";
            case OPER_DIV: return "OPER_DIV";
            case OPER_NOT: return "OPER_NOT";
            case OPER_EQ: return "OPER_EQ";
            case OPER_NE: return "OPER_NE";
            case OPER_LT: return "OPER_LT";
            case OPER_GT: return "OPER_GT";
            case OPER_LE: return "OPER_LE";
            case OPER_GE: return "OPER_GE";
            case OPER_COMMA: return "OPER_COMMA";
            case OPER_DOT: return "OPER_DOT";
            case ASSIGN: return "ASSIGN";
            case LPAREN: return "LPAREN";
            case RPAREN: return "RPAREN";
            case LBRACE: return "LBRACE";
            case RBRACE: return "RBRACE";
            case KW_LET: return "KW_LET";
            case KW_FUNC: return "KW_FUNC";
            case KW_DO: return "KW_DO";
            case KW_RETURN: return "KW_RETURN";
            case KW_IF: return "KW_IF";
            case KW_ELSE: return "KW_ELSE";
            case KW_LOOP: return "KW_LOOP";
            case KW_BREAK: return "KW_BREAK";
            case KW_CONTINUE: return "KW_CONTINUE";
            case KW_IMPORT: return "KW_IMPORT";
            case KW_USE: return "KW_USE";
            case KW_AS: return "KW_AS";
            case KW_VEC: return "KW_VEC";
            case KW_CONST: return "KW_CONST";
            case KW_VAR: return "KW_VAR";
            case LBRACKET: return "LBRACKET";
            case RBRACKET: return "RBRACKET";
            case NUM_LITERAL: return "NUM_LITERAL";
            case IDENTIFIER: return "IDENTIFIER";
            case STRING_LITERAL: return "STRING_LITERAL";
            case 0: return "EOF";
            default: return "UNKNOWN";
        }
    }
}
