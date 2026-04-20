%{
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "parser.tab.hpp" // Bison 生成的头文件
#include "lexer/token.hpp"

namespace lexer {
    std::vector<Token> tokens;
    int line = 1;
    int column = 1;
    
    void addToken(TokenType type, const std::string& value) {
        tokens.emplace_back(type, value, line, column);
        column += value.length();
    }
    
    void handleUnknown(char c) {
        std::cerr << "Error at line " << line << ", column " << column << ": Unknown character '" << c << "'" << std::endl;
        addToken(1, std::string(1, c));
        column++;
    }
}

using namespace lexer;


%}

%option noyywrap

%%

[ \t\r]+       { column += yyleng; }
\n             { line++; column = 1; }

"+"            { addToken(OPER_PLUS, yytext); yylval.string_val = new std::string(yytext); return OPER_PLUS; }
"-"            { addToken(OPER_MINUS, yytext); yylval.string_val = new std::string(yytext); return OPER_MINUS; }
"*"            { addToken(OPER_MUL, yytext); yylval.string_val = new std::string(yytext); return OPER_MUL; }
"/"            { addToken(OPER_DIV, yytext); yylval.string_val = new std::string(yytext); return OPER_DIV; }
"="            { addToken(ASSIGN, yytext); yylval.string_val = new std::string(yytext); return ASSIGN; }
"("            { addToken(LPAREN, yytext); yylval.string_val = new std::string(yytext); return LPAREN; }
")"            { addToken(RPAREN, yytext); yylval.string_val = new std::string(yytext); return RPAREN; }

"let"          { addToken(KW_LET, yytext); yylval.string_val = new std::string(yytext); return KW_LET; }

[0-9][0-9_]*  { 
    std::string num = yytext;
    num.erase(std::remove(num.begin(), num.end(), '_'), num.end());
    addToken(NUM_LITERAL, num);
    yylval.string_val = new std::string(num);
    return NUM_LITERAL;
}

[a-zA-Z_][a-zA-Z0-9_]* { 
    addToken(IDENTIFIER, yytext);
    yylval.string_val = new std::string(yytext);
    return IDENTIFIER;
}

.             { 
    handleUnknown(yytext[0]);
    return 1;
}

%%

namespace lexer {
    std::vector<Token> lex(const std::string& source) {
        tokens.clear();
        line = 1;
        column = 1;
        yy_scan_string(source.c_str());
        
        // 循环获取所有 token
        int token;
        while ((token = yylex()) != 0) {
            // 已经在规则中添加了 token
        }
        
        yy_delete_buffer(YY_CURRENT_BUFFER);
        tokens.emplace_back(0, "", line, column);
        return tokens;
    }
}
