#include "test.hpp"

#include <ctime>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "irgen/opcode.hpp"
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

    const irgen::Value manual_result = lm::irgen::execute(manual_ast);
    ASSERT(!manual_result.isNone());

    delete ast;
    delete manual_ast;
}

std::string fib_format(const int n) {
    return std::format("fib({})", n);
}

void test_fib_speed() {
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
    std::cout << "Fib code: " << std::endl;
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
    const irgen::Value log_result = lm::irgen::execute(log_ast);
    ASSERT(log_result.isString());
    ASSERT(log_result.asString() == "Hello, World!");
    delete log_ast;

    const std::string debug_test = R"(
        std.decos.debug func add(a, b) {
            return a + b
        }
        add(3, 5)
    )";

    lmx::ProgramASTNode* debug_ast = parse(debug_test);
    ASSERT(debug_ast != nullptr);
    const irgen::Value debug_result = lm::irgen::execute(debug_ast);
    ASSERT(debug_result.isNumber());
    ASSERT(debug_result.asNumber() == irgen::Value(8).asNumber());
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
    const irgen::Value custom_result = lm::irgen::execute(custom_ast);
    ASSERT(custom_result.isString());
    ASSERT(custom_result.asString() == "hello, Alice");
    delete custom_ast;
}

} // namespace

int main() {
    //test::run("vm_goto", test_vm_goto);
    //test::run("parse_and_execute", test_parse_and_execute);
    test::run("fib_speed", test_fib_speed);
    test::run("decorators", test_decorators);
    std::cin.get();
    return test::summary();
}
