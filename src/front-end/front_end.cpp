#include "front_end.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

#include "../tools/debug.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../../tools/error.hpp"
#include "../../tools/utf8_io.hpp"

std::string detail_msg;

lmx::ProgramASTNode* parse(const std::string& source, const std::string& filename) {
    LOG("\nfront-end Parsing...\n");

    try {
        const std::string utf8_source = lm::utf8_io::normalize_to_utf8(source);

        lmx::Lexer lexer(filename);
        lexer.add_input(utf8_source);
        const std::vector<lmx::Token> tokens = lexer.tokenize();

        for (const auto& token : tokens) {
            if (token.type == lmx::TokenType::END && !token.value.empty()) {
                detail_msg = "Lexer Error: unclosed string literal at line " +
                             std::to_string(token.line) + ", column " + std::to_string(token.column);
                LOG("\nfront-end Parsing failed!\n");
                LOG("Error: " << detail_msg);
                return nullptr;
            }
        }

        lmx::Parser parser(filename);
        parser.add_tokens(tokens, utf8_source);

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

lmx::ProgramASTNode* parse(const std::string& source) {
    return parse(source, "<input>");
}
