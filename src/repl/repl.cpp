#include "repl.hpp"
#include "../front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "../tools/error.hpp"

extern bool has_err;
extern std::string error_msg();

std::string get_complete_err_msg() {
    return error_msg();
}

bool repl::REPL::exec_input(const std::function<std::string()> &input_func) {
    const std::string input = input_func();
    if (input.empty()) return false;
    lmx::ProgramASTNode* got_ast = parse(input);

    if (has_err) {
        has_err = false;
        throw SyntaxError(error_msg());
    }

    auto code = lm::irgen::Generator(got_ast).gen();
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();

    vm.call_stack.clear();
    delete got_ast;
    return true;
}
