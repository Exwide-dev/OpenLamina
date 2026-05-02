%{
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "../tools/debug.hpp"
#include "parser.tab.hpp" // Bison 生成的头文件
#include "lexer/token.hpp"
#include "lexer/lexer_vars.hpp"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif

namespace lexer {
    std::vector<Token> tokens;
    int line = 1;
    int column = 1;
    std::string string_buf;
    std::string details;

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
%option outfile="lexer_generated.cpp"
%option header-file="lexer_generated.h"

%x STRING

%%

[ \t\r]+       { column += yyleng; }
\n             { line++; column = 1; LOG("Out of STRING, \\n collect!"); }

"+"            { addToken(OPER_PLUS, yytext); yylval.string_val = new std::string(yytext); return OPER_PLUS; }
"-"            { addToken(OPER_MINUS, yytext); yylval.string_val = new std::string(yytext); return OPER_MINUS; }
"*"            { addToken(OPER_MUL, yytext); yylval.string_val = new std::string(yytext); return OPER_MUL; }
"/"            { addToken(OPER_DIV, yytext); yylval.string_val = new std::string(yytext); return OPER_DIV; }
"="            { addToken(ASSIGN, yytext); yylval.string_val = new std::string(yytext); return ASSIGN; }
","            { addToken(OPER_COMMA, yytext); yylval.string_val = new std::string(yytext); return OPER_COMMA; }
"("            { addToken(LPAREN, yytext); yylval.string_val = new std::string(yytext); return LPAREN; }
")"            { addToken(RPAREN, yytext); yylval.string_val = new std::string(yytext); return RPAREN; }
"{"            { addToken(LBRACE, yytext); yylval.string_val = new std::string(yytext); return LBRACE; }
"}"            { addToken(RBRACE, yytext); yylval.string_val = new std::string(yytext); return RBRACE; }
"!"            { addToken(OPER_NOT, yytext); yylval.string_val = new std::string(yytext); return OPER_NOT; }
"=="           { addToken(OPER_EQ, yytext); yylval.string_val = new std::string(yytext); return OPER_EQ; }
"!="           { addToken(OPER_NE, yytext); yylval.string_val = new std::string(yytext); return OPER_NE; }
"<"            { addToken(OPER_LT, yytext); yylval.string_val = new std::string(yytext); return OPER_LT; }
">"            { addToken(OPER_GT, yytext); yylval.string_val = new std::string(yytext); return OPER_GT; }
"<="           { addToken(OPER_LE, yytext); yylval.string_val = new std::string(yytext); return OPER_LE; }
">="           { addToken(OPER_GE, yytext); yylval.string_val = new std::string(yytext); return OPER_GE; }

"let"          { addToken(KW_LET, yytext); yylval.string_val = new std::string(yytext); return KW_LET; }
"func"         { addToken(KW_FUNC, yytext); yylval.string_val = new std::string(yytext); return KW_FUNC; }
"do"           { addToken(KW_DO, yytext); yylval.string_val = new std::string(yytext); return KW_DO; }
"return"       { addToken(KW_RETURN, yytext); yylval.string_val = new std::string(yytext); return KW_RETURN; }
"if"           { addToken(KW_IF, yytext); yylval.string_val = new std::string(yytext); return KW_IF; }
"else"         { addToken(KW_ELSE, yytext); yylval.string_val = new std::string(yytext); return KW_ELSE; }
"loop"         { addToken(KW_LOOP, yytext); yylval.string_val = new std::string(yytext); return KW_LOOP; }
"while"         { addToken(KW_WHILE, yytext); yylval.string_val = new std::string(yytext); return KW_WHILE; }
"break"        { addToken(KW_BREAK, yytext); yylval.string_val = new std::string(yytext); return KW_BREAK; }
"continue"     { addToken(KW_CONTINUE, yytext); yylval.string_val = new std::string(yytext); return KW_CONTINUE; }
"import"       { addToken(KW_IMPORT, yytext); yylval.string_val = new std::string(yytext); return KW_IMPORT; }
"use"          { addToken(KW_USE, yytext); yylval.string_val = new std::string(yytext); return KW_USE; }
"as"           { addToken(KW_AS, yytext); yylval.string_val = new std::string(yytext); return KW_AS; }
"."            { addToken(OPER_DOT, yytext); yylval.string_val = new std::string(yytext); return OPER_DOT; }

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

\"              { string_buf.clear(); BEGIN(STRING); }

<STRING>{
    \"          { BEGIN(INITIAL); addToken(STRING_LITERAL, string_buf); yylval.string_val = new std::string(string_buf); return STRING_LITERAL; }
    \\n         { string_buf += '\n'; }
    \\t         { string_buf += '\t'; }
    \\r         { string_buf += '\r'; }
    \\\"        { string_buf += '"'; }
    \\\\        { string_buf += '\\'; }
    \\.         { string_buf += yytext[1]; }
    [^\\\"\n]+  { string_buf += yytext; }
    \n          { LOG("In STRING, \\n collect!");
                  BEGIN(INITIAL);
                  details = "Lexer SyntaxError at line " + std::to_string(line) + ": unclosed string literal";
                  addToken(1, string_buf);
                  yylval.string_val = new std::string(string_buf);
                  return 1;
                }
    <<EOF>>     { LOG("In STRING, EOF collect!");
                  BEGIN(INITIAL);
                  details = "Lexer SyntaxError at line " + std::to_string(line) + ": unclosed string literal at end of file";
                  addToken(1, string_buf);
                  yylval.string_val = new std::string(string_buf);
                  return 1;
                }
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
        details.clear();
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