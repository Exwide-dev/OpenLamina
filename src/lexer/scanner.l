%{
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "../tools/debug.hpp"
#include "parser.tab.hpp"
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
    std::string prev_token;
    bool prev_was_keyword = false;
    
    enum class BraceType {
        NONE,
        BRACE_FUNC,
        BRACE_DICT
    };
    
    std::vector<BraceType> brace_stack;
    
    bool in_func_body() {
        for (auto it = brace_stack.rbegin(); it != brace_stack.rend(); ++it) {
            if (*it == BraceType::BRACE_FUNC) {
                return true;
            }
        }
        return false;
    }

    void addToken(TokenType type, const std::string& value) {
        tokens.emplace_back(type, value, line, column);
        column += value.length();
    }

    void handleUnknown(char c) {
        std::cerr << "Error at line " << line << ", column " << column << ": Unknown character '" << c << "'" << std::endl;
        addToken(1, std::string(1, c));
        column++;
    }
    
    void update_brace_context(const std::string& token) {
        if (token == "{") {
            bool is_func_body = prev_was_keyword;
            static const std::string func_keywords[] = {
                "func", "do", "if", "else", "loop", "while"
            };
            for (const auto& kw : func_keywords) {
                if (prev_token == kw) {
                    is_func_body = true;
                    break;
                }
            }
            
            if (is_func_body || (!brace_stack.empty() && brace_stack.back() == BraceType::BRACE_FUNC)) {
                brace_stack.push_back(BraceType::BRACE_FUNC);
            } else {
                brace_stack.push_back(BraceType::BRACE_DICT);
            }
        } else if (token == "}") {
            if (!brace_stack.empty()) {
                brace_stack.pop_back();
            }
        }
        
        prev_token = token;
        prev_was_keyword = false;
        static const std::string keywords[] = {
            "func", "do", "if", "else", "loop", "while", "let", "const", 
            "return", "break", "continue", "import", "use", "as", "var",
            "vec", "intern", "export", "with", "make"
        };
        for (const auto& kw : keywords) {
            if (token == kw) {
                prev_was_keyword = true;
                break;
            }
        }
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

\n             { 
                 line++; 
                 column = 1; 
                 if (in_func_body()) {
                     LOG("NEWLINE in func body");
                     yylval.string_val = new std::string("\n");
                     return NEWLINE;
                 }
                 LOG("NEWLINE ignored");
             }

"+"            { update_brace_context("+"); addToken(OPER_PLUS, yytext); yylval.string_val = new std::string(yytext); return OPER_PLUS; }
"-"            { update_brace_context("-"); addToken(OPER_MINUS, yytext); yylval.string_val = new std::string(yytext); return OPER_MINUS; }
"*"            { update_brace_context("*"); addToken(OPER_MUL, yytext); yylval.string_val = new std::string(yytext); return OPER_MUL; }
"/"            { update_brace_context("/"); addToken(OPER_DIV, yytext); yylval.string_val = new std::string(yytext); return OPER_DIV; }
"="            { update_brace_context("="); addToken(ASSIGN, yytext); yylval.string_val = new std::string(yytext); return ASSIGN; }
","            { update_brace_context(","); addToken(OPER_COMMA, yytext); yylval.string_val = new std::string(yytext); return OPER_COMMA; }
"("            { update_brace_context("("); addToken(LPAREN, yytext); yylval.string_val = new std::string(yytext); return LPAREN; }
")"            { update_brace_context(")"); addToken(RPAREN, yytext); yylval.string_val = new std::string(yytext); return RPAREN; }
"{"            { update_brace_context("{"); addToken(LBRACE, yytext); yylval.string_val = new std::string(yytext); return LBRACE; }
"}"            { update_brace_context("}"); addToken(RBRACE, yytext); yylval.string_val = new std::string(yytext); return RBRACE; }
"!"            { update_brace_context("!"); addToken(OPER_NOT, yytext); yylval.string_val = new std::string(yytext); return OPER_NOT; }
"=="           { update_brace_context("=="); addToken(OPER_EQ, yytext); yylval.string_val = new std::string(yytext); return OPER_EQ; }
"!="           { update_brace_context("!="); addToken(OPER_NE, yytext); yylval.string_val = new std::string(yytext); return OPER_NE; }
"<"            { update_brace_context("<"); addToken(OPER_LT, yytext); yylval.string_val = new std::string(yytext); return OPER_LT; }
">"            { update_brace_context(">"); addToken(OPER_GT, yytext); yylval.string_val = new std::string(yytext); return OPER_GT; }
"<="           { update_brace_context("<="); addToken(OPER_LE, yytext); yylval.string_val = new std::string(yytext); return OPER_LE; }
">="           { update_brace_context(">="); addToken(OPER_GE, yytext); yylval.string_val = new std::string(yytext); return OPER_GE; }
":"            { update_brace_context(":"); addToken(OPER_COLON, yytext); yylval.string_val = new std::string(yytext); return OPER_COLON; }
"."            { update_brace_context("."); addToken(OPER_DOT, yytext); yylval.string_val = new std::string(yytext); return OPER_DOT; }
"["            { update_brace_context("["); addToken(LBRACKET, yytext); yylval.string_val = new std::string(yytext); return LBRACKET; }
"]"            { update_brace_context("]"); addToken(RBRACKET, yytext); yylval.string_val = new std::string(yytext); return RBRACKET; }

"let"          { update_brace_context("let"); addToken(KW_LET, yytext); yylval.string_val = new std::string(yytext); return KW_LET; }
"func"         { update_brace_context("func"); addToken(KW_FUNC, yytext); yylval.string_val = new std::string(yytext); return KW_FUNC; }
"do"           { update_brace_context("do"); addToken(KW_DO, yytext); yylval.string_val = new std::string(yytext); return KW_DO; }
"return"       { update_brace_context("return"); addToken(KW_RETURN, yytext); yylval.string_val = new std::string(yytext); return KW_RETURN; }
"if"           { update_brace_context("if"); addToken(KW_IF, yytext); yylval.string_val = new std::string(yytext); return KW_IF; }
"else"         { update_brace_context("else"); addToken(KW_ELSE, yytext); yylval.string_val = new std::string(yytext); return KW_ELSE; }
"loop"         { update_brace_context("loop"); addToken(KW_LOOP, yytext); yylval.string_val = new std::string(yytext); return KW_LOOP; }
"while"        { update_brace_context("while"); addToken(KW_WHILE, yytext); yylval.string_val = new std::string(yytext); return KW_WHILE; }
"break"        { update_brace_context("break"); addToken(KW_BREAK, yytext); yylval.string_val = new std::string(yytext); return KW_BREAK; }
"continue"     { update_brace_context("continue"); addToken(KW_CONTINUE, yytext); yylval.string_val = new std::string(yytext); return KW_CONTINUE; }
"import"       { update_brace_context("import"); addToken(KW_IMPORT, yytext); yylval.string_val = new std::string(yytext); return KW_IMPORT; }
"use"          { update_brace_context("use"); addToken(KW_USE, yytext); yylval.string_val = new std::string(yytext); return KW_USE; }
"as"           { update_brace_context("as"); addToken(KW_AS, yytext); yylval.string_val = new std::string(yytext); return KW_AS; }
"const"        { update_brace_context("const"); addToken(KW_CONST, yytext); yylval.string_val = new std::string(yytext); return KW_CONST; }
"var"          { update_brace_context("var"); addToken(KW_VAR, yytext); yylval.string_val = new std::string(yytext); return KW_VAR; }
"vec"          { update_brace_context("vec"); addToken(KW_VEC, yytext); yylval.string_val = new std::string(yytext); return KW_VEC; }
"intern"       { update_brace_context("intern"); addToken(KW_INTERN, yytext); yylval.string_val = new std::string(yytext); return KW_INTERN; }
"export"       { update_brace_context("export"); addToken(KW_EXPORT, yytext); yylval.string_val = new std::string(yytext); return KW_EXPORT; }
"with"         { update_brace_context("with"); addToken(KW_WITH, yytext); yylval.string_val = new std::string(yytext); return KW_WITH; }
"make"         { update_brace_context("make"); addToken(KW_MAKE, yytext); yylval.string_val = new std::string(yytext); return KW_MAKE; }

[0-9][0-9_]*  {
    std::string num = yytext;
    num.erase(std::remove(num.begin(), num.end(), '_'), num.end());
    update_brace_context(num);
    addToken(NUM_LITERAL, num);
    yylval.string_val = new std::string(num);
    return NUM_LITERAL;
}

[a-zA-Z_][a-zA-Z0-9_]* {
    update_brace_context(yytext);
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
        brace_stack.clear();
        prev_token.clear();
        prev_was_keyword = false;
        yy_scan_string(source.c_str());

        int token;
        while ((token = yylex()) != 0) {
        }

        yy_delete_buffer(YY_CURRENT_BUFFER);
        tokens.emplace_back(0, "", line, column);
        return tokens;
    }
}
