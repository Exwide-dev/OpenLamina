#include "irgen/bytecode_file.hpp"
#include "irgen/generator.hpp"
#include "../front-end/front_end.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: dump_bc file.lm\n";
        return 1;
    }
    std::ifstream in(argv[1]);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string src = ss.str();
    lmx::ProgramASTNode* ast = lmx::parse(src, argv[1]);
    if (!ast) {
        std::cerr << "parse failed\n";
        return 1;
    }
    lm::irgen::bytecode_optimize_enabled = false;
    auto code = lm::irgen::Generator(ast).gen();
    lm::irgen::print_code(code);
    for (size_t i = 0; i < code.size(); ++i) {
        if (const auto* push = std::get_if<::irgen::PUSH>(&code[i])) {
            if (push->val.isUserFunction()) {
                const auto& f = push->val.asFunctionObject();
                std::cerr << "PUSH func " << f->name << " entry_pc=" << f->entry_pc
                          << " location=" << f->location << " at i=" << i << "\n";
            }
        }
    }
    delete ast;
    return 0;
}
