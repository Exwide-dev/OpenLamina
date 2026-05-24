#include "front_end.hpp"
#include "../tools/debug.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../../tools/error.hpp"

std::string detail_msg;

lmx::ProgramASTNode* parse(const std::string &source) {
    LOG("\nfront-end Parsing...\n");
    
    try {
        lmx::Lexer lexer(source);
        lmx::Parser parser(lexer);
        
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