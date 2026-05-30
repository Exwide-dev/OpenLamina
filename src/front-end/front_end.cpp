#include "front_end.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

#include "../tools/debug.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../../tools/error.hpp"

std::string detail_msg;

lmx::ProgramASTNode* parse(const std::string &source, const std::string& filename) {
    LOG("\nfront-end Parsing...\n");
    
    try {
        lmx::Lexer lexer(filename);
        lexer.add_input(source);
        std::vector<lmx::Token> tokens = lexer.tokenize();
        
        for (const auto& tok : tokens) {
            if (tok.type == lmx::TokenType::END && !tok.value.empty()) {
                detail_msg = "Lexer Error: unclosed string literal at line " + 
                           std::to_string(tok.line) + ", column " + std::to_string(tok.column);
                LOG("\nfront-end Parsing failed!\n");
                LOG("Error: " << detail_msg);
                return nullptr;
            }
        }
        
        lmx::Parser parser(filename);
        parser.add_tokens(tokens, source);
        
        lmx::ProgramASTNode* result = parser.parse();
        
        LOG("\nfront-end Parsing successful!\n");
        return result;
    } catch (const SyntaxError& e) {
        detail_msg = e.what();
        LOG("\nfront-end Parsing failed!\n");
        LOG("Error: " << detail_msg);
        return nullptr;
    }
}

lmx::ProgramASTNode* parse(const std::string &source) {
    return parse(source, "<input>");
}