#include "repl.hpp"
#include "../front-end/front_end.hpp"

#include "irgen/generator.hpp"

void repl::REPL::exec_input(const std::function<std::string()>& input_func) {
    const std::string input = input_func();
    lmx::ProgramASTNode* got_ast = parse(input);
    auto code = lm::irgen::Generator(got_ast).gen();
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();
}
