#include "test.hpp"

#include <ctime>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "irgen/opcode.hpp"
#include "irgen/optimizer.hpp"
namespace {

void test_vm_goto() {
    irgen::VM vm;
    const std::vector<irgen::Opcode> test_code = {
        irgen::PUSH(irgen::Value(static_cast<size_t>(1))),
        irgen::GOTO(0),
        irgen::PUSH(irgen::Value(static_cast<size_t>(2))),
        irgen::LABEL(0),
        irgen::PUSH(irgen::Value(static_cast<size_t>(3))),
    };

    for (auto& op : test_code) {
        vm.code.push_back(op);
    }

    vm.run();
    ASSERT(!vm.op_stack.empty());
}

void test_parse_and_execute() {
    const std::string source = R"(
    func add(a, b) {
        return a + b
    }
    func multiply(a, b) {
        return a * b
    }
    let x = 10
    let y = 20
    let result = add(x, y)
    let product = multiply(5, 3)
    func factorial(n) {
        if (n <= 1) {
            return 1
        }
        return n * factorial(n - 1)
    }
    let fact5 = factorial(5)
    )";

    lmx::ProgramASTNode* ast = parse(source);
    ASSERT(ast != nullptr);

    const auto* manual_ast = parse("let a = 10 + 5 * 2\na");
    ASSERT(manual_ast != nullptr);

    ASSERT(lm::irgen::execute(manual_ast, [&](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        ASSERT(!vm.op_stack.top().isNone());
        return true;
    }));

    delete ast;
    delete manual_ast;
}

std::string fib_format(const int n) {
    return std::format("fib({})", n);
}

void test_fib_speed() {
    lm::irgen::bytecode_optimize_enabled = true;

    irgen::VM vm;

    const std::string input = R"(
func fib(n) {
    if (n <= 1) {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}
)";
    lmx::ProgramASTNode* got_ast = parse(input);
    ASSERT(got_ast != nullptr);

    auto code = lm::irgen::Generator(got_ast).gen();
    std::cout << "Fib code (optimized, -O):\n";
    lm::irgen::print_code(code);
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();

    const std::vector cases = {20, 25, 30};

    for (const int n : cases) {
        constexpr int iterations = 20;
        const auto time_start = clock();
        lmx::ProgramASTNode* ast = parse(fib_format(n));
        ASSERT(ast != nullptr);

        auto tcode = lm::irgen::Generator(ast).gen();
        const size_t invoke_pc = vm.code.size();
        vm.code.insert(vm.code.end(), tcode.begin(), tcode.end());
        for (int i = 0; i < iterations; ++i) {
            vm.pc = invoke_pc;
            vm.run();
        }

        ASSERT(!vm.op_stack.empty());
        std::cerr << "  fib(" << n << ") avg "
                  << static_cast<long double>(clock() - time_start) / CLOCKS_PER_SEC / iterations
                  << " s, result: "
                  << vm.op_stack.top()
                  << std::endl;
        delete ast;
    }

    delete got_ast;
    lm::irgen::bytecode_optimize_enabled = false;
}

void test_decorators() {
    const std::string log_test = R"(
        std.decos.log func greet(name) {
            return "Hello, " + name + "!"
        }
        greet("World")
    )";

    lmx::ProgramASTNode* log_ast = parse(log_test);
    ASSERT(log_ast != nullptr);
    ASSERT(lm::irgen::execute(log_ast, [&](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& log_result = vm.op_stack.top();
        ASSERT(log_result.isString());
        ASSERT(log_result.asString() == "Hello, World!");
        return true;
    }));
    delete log_ast;

    const std::string debug_test = R"(
        std.decos.debug func add(a, b) {
            return a + b
        }
        add(3, 5)
    )";

    lmx::ProgramASTNode* debug_ast = parse(debug_test);
    ASSERT(debug_ast != nullptr);
    ASSERT(lm::irgen::execute(debug_ast, [&](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& debug_result = vm.op_stack.top();
        ASSERT(debug_result.isNumber());
        ASSERT(debug_result.asNumber() == irgen::Value(8).asNumber());
        return true;
    }));
    delete debug_ast;

    const std::string custom_test = R"(
        func make_upper(f) {
            return do(x) {
                let result = f(x)
                return result
            }
        }
        make_upper func say_hello(name) {
            return "hello, " + name
        }
        say_hello("Alice")
    )";

    lmx::ProgramASTNode* custom_ast = parse(custom_test);
    ASSERT(custom_ast != nullptr);
    ASSERT(lm::irgen::execute(custom_ast, [&](irgen::VM& vm) {
        ASSERT(!vm.op_stack.empty());
        const irgen::Value& custom_result = vm.op_stack.top();
        ASSERT(custom_result.isString());
        ASSERT(custom_result.asString() == "hello, Alice");
        return true;
    }));
    delete custom_ast;
}

} // namespace

void test_gc_memory_trend();
void test_bytecode_optimizer();
void test_bytecode_lmc_roundtrip();
void test_type_convert_sugar();
void test_exceptions_and_try_catch();
void test_friend_func_dispatch();
void test_iter_stopiteration_edges();
void test_macro_system();
void test_macro_identity();
void test_macro_variadic_log();
void test_macro_it_text();
void test_macro_no_return();
void test_macro_bug_eval_call_in_log();
void test_macro_bug_nested_macro_compose();
void test_macro_nested_splat();
void test_macro_ast_struct_match();
void test_type_handle();
void test_quote_syntax_forms();
void test_container_methods_and_format();
void test_try_catch_skips_after_struct_throw();
void test_elif_and_or_not();
void test_std_typing();
void test_generic_struct();
void test_decimal_literal();
void test_vec_template();
void test_vec_generic_struct();
void test_outside_func();
void test_outside_module_scope_error();
void test_outside_convert_overload();
void test_std_random();
void test_vector_join();
void test_comprehension_join_chain();

int main() {
    //test::run("vm_goto", test_vm_goto);
    //test::run("parse_and_execute", test_parse_and_execute);
    //test::run("fib_speed", test_fib_speed);
    //test::run("decorators", test_decorators);
    // test::run("bytecode_optimizer", test_bytecode_optimizer);
    // test::run("type_convert_sugar", test_type_convert_sugar);
    // test::run("exceptions_and_try_catch", test_exceptions_and_try_catch);
    // test::run("friend_func_dispatch", test_friend_func_dispatch);
    // test::run("iter_stopiteration_edges", test_iter_stopiteration_edges);
    // test::run("bytecode_lmc_roundtrip", test_bytecode_lmc_roundtrip);
    // test::run("macro_system", test_macro_system);
    // test::run("macro_identity", test_macro_identity);
    // test::run("macro_variadic_log", test_macro_variadic_log);
    // test::run("macro_it_text", test_macro_it_text);
    // test::run("macro_no_return", test_macro_no_return);
    // test::run("macro_bug_eval_call_in_log", test_macro_bug_eval_call_in_log);
    // test::run("macro_bug_nested_macro_compose", test_macro_bug_nested_macro_compose);
    // test::run("macro_nested_splat", test_macro_nested_splat);
    // test::run("macro_ast_struct_match", test_macro_ast_struct_match);
    // test::run("type_handle", test_type_handle);
    // test::run("quote_syntax_forms", test_quote_syntax_forms);
    // test::run("container_methods_and_format", test_container_methods_and_format);
    // test::run("try_catch_skips_after_struct_throw", test_try_catch_skips_after_struct_throw);
    // test::run("elif_and_or_not", test_elif_and_or_not);
    //test::run("gc_memory_trend", test_gc_memory_trend);
    try {
        test::run("std_typing", test_std_typing);
        test::run("generic_struct", test_generic_struct);
        test::run("decimal_literal", test_decimal_literal);
        test::run("vec_template", test_vec_template);
        test::run("vec_generic_struct", test_vec_generic_struct);
        test::run("outside_func", test_outside_func);
        test::run("outside_module_scope_error", test_outside_module_scope_error);
        test::run("outside_convert_overload", test_outside_convert_overload);
        test::run("std_random", test_std_random);
        test::run("vector_join", test_vector_join);
        test::run("comprehension_join_chain", test_comprehension_join_chain);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }
    std::cerr << "Ready to return..." << std::endl;
    return test::summary();
}
