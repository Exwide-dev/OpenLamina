#include "front_end.hpp"
#include "../tools/debug.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../../tools/error.hpp"

std::string detail_msg;

std::string format_error_message(const lmx::ParseError& err, bool with_color = true) {
    std::ostringstream oss;
    
    if (!err.filename.empty()) {
        if (err.line > 0) {
            oss << "  --> " << err.filename << ":" << err.line << ":" << err.column << "\n";
        } else {
            oss << "  --> " << err.filename << "\n";
        }
    }
    
    oss << "   |\n";
    
    if (err.line > 0) {
        std::ostringstream line_num;
        line_num << err.line;
        std::string line_num_str = line_num.str();
        
        oss << " " << std::setw(line_num_str.length()) << " " << " | ";
        
        if (!err.context_line.empty()) {
            oss << err.context_line << "\n";
        } else {
            oss << "\n";
        }
        
        oss << " " << line_num_str << " | ";
        
        if (!err.context_line.empty()) {
            size_t spaces_to_add = err.column - 1;
            oss << std::string(spaces_to_add, ' ');
            size_t underline_len = std::min(err.context_line.length() - err.column + 1, 
                                           static_cast<size_t>(err.message.length()));
            if (underline_len == 0) underline_len = 1;
            if (with_color) {
                oss << "\033[1;31m" << std::string(underline_len, '~') << "\033[0m";
            } else {
                oss << std::string(underline_len, '~');
            }
        }
        oss << "\n";
    }
    
    oss << "   | ";
    if (with_color) {
        oss << "\033[1;31merror:\033[0m ";
    } else {
        oss << "error: ";
    }
    oss << err.message << "\n";
    
    return oss.str();
}

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
        parser.add_tokens(tokens);
        auto source_lines = lexer.get_source_lines();
        const_cast<std::vector<std::string>&>(parser.get_source_lines()) = source_lines;
        
        lmx::ProgramASTNode* result = parser.parse();
        
        if (parser.has_error()) {
            const auto& err = parser.get_error();
            detail_msg = format_error_message(err);
            LOG("\nfront-end Parsing failed!\n");
            LOG("Error: " << detail_msg);
            delete result;
            return nullptr;
        }
        
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
