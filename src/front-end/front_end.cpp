#include "front_end.hpp"
#include "../tools/debug.hpp"

lmx::ProgramASTNode* result = nullptr;

lmx::ProgramASTNode* parse(const std::string &source) {
    yy_scan_string(source.c_str());
    LOG("\nfront-end Parsing...\n");
    const int parse_result = yyparse();

    if (parse_result == 0) {
        LOG("\nfront-end Parsing successful!\n");
        lmx::ProgramASTNode* m_result = result;
        result = nullptr;
        return m_result;
    }
    LOG("\nfront-end Parsing failed!\n");
    return nullptr;
}