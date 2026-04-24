#include "front_end.hpp"

lmx::ProgramASTNode* result = nullptr;

lmx::ProgramASTNode* parse(const std::string &source) {
    yy_scan_string(source.c_str());
    std::cout << "\nfront-end Parsing...\n";
    const int parse_result = yyparse();

    if (parse_result == 0) {
        std::cout << "\nfront-end Parsing successful!\n";
        lmx::ProgramASTNode* m_result = result;
        result = nullptr;
        return m_result;
    }
    std::cout << "\nfront-end Parsing failed!\n";
    lmx::ProgramASTNode* result = nullptr;
    return nullptr;
}