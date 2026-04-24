#include "repl.hpp"
#include "../front-end/front_end.hpp"
#include "parser.tab.hpp"
extern bool has_err;
extern std::string error_msg();
#include "irgen/generator.hpp"

bool repl::REPL::exec_input(const std::function<std::string()> &input_func) {
    const std::string input = input_func();
    lmx::ProgramASTNode* got_ast = parse(input);

    if (has_err) {
        std::cerr << "Parser error!" << std::endl;
        std::cerr << error_msg() << std::endl;
        has_err = false;
        return false;
    }

    auto code = lm::irgen::Generator(got_ast).gen();
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();

    delete got_ast;
    return true;
}
