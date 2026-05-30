#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "lexer/lexer.hpp"
#include "parser/ast.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "irgen/opcode.hpp"
#include "repl/repl.hpp"
#include "tools/error.hpp"
#include "parser/ast.hpp"
#include "tools/cli.hpp"

void test_code1() {
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
}

void test_code2() {
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

    std::cout << "Source code:\n" << source << "\n";

    std::cout << "\nParsing...\n";
    const lmx::ProgramASTNode* ast = parse(source);
    printAST(ast);

    std::cout << "\nTesting with manually built AST...\n";
    const auto* manual_ast = parse("let a = 10 + 5 * 2\na");
    printAST(manual_ast);

    const ::irgen::Value manual_result = lm::irgen::execute(manual_ast);
    if (!manual_result.isNone()) {
        std::cout << "Manual AST execution result: " << manual_result << std::endl;
    } else {
        std::cout << "Manual AST execution resulted in no value" << std::endl;
    }

    delete ast;
    delete manual_ast;
}

std::string test_fib_format(int n) {
    return std::format("fib({})", n);
}

void test_fib_speed() {
    irgen::VM vm;

    const std::string input =
R"(
func fib(n) {
    if (n <= 1) {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}
)";
    lmx::ProgramASTNode* got_ast = parse(input);

    auto code = lm::irgen::Generator(got_ast).gen();
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();

    const std::vector tests = {20, 25, 30};

    for (const auto& test : tests) {
        constexpr int n = 20;
        const auto tstart = clock();
        auto ast = parse(test_fib_format(test));
        auto tcode = lm::irgen::Generator(ast).gen();
        std::cerr << "AST: " << std::endl;
        printAST(ast);
        std::cerr << "Code: " << std::endl;
        std::string codes;
        size_t j = 0;
        for (auto const& elem : code) {
            std::visit([&](auto& op) {
                codes.append(std::format("{:3} | {}\n", j, op.toString()));
            }, elem);
            j++;
        }
        std::cerr << codes << std::endl;
        for (int i = 0; i < n; i++) {
            std::cerr << "Testing " << test_fib_format(test) << std::endl;
            vm.code.insert(vm.code.end(), tcode.begin(), tcode.end());
            vm.run();
        }
        std::cerr << "Result: " << vm.op_stack.top() << std::endl;
        std::cerr << "Average Time: " << static_cast<long double>(clock() - tstart) / CLOCKS_PER_SEC / n << " s\n" << std::endl;
    }

    delete got_ast;
}

void test_decorators() {
    std::cout << "\n=== Testing Decorators ===\n";
    
    try {
        std::cout << "\n1. Testing std.decos.log decorator...\n";
        const std::string log_test = R"(
            std.decos.log func greet(name) {
                return "Hello, " + name + "!"
            }
            
            print(greet("World"))
        )";
        
        lmx::ProgramASTNode* log_ast = parse(log_test);
        if (!log_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value log_result = lm::irgen::execute(log_ast);
        delete log_ast;
        std::cout << "\nLog decorator test passed!\n";
        
        std::cout << "\n2. Testing std.decos.debug decorator...\n";
        const std::string debug_test = R"(
            std.decos.debug func add(a, b) {
                return a + b
            }
            
            print(add(3, 5))
        )";
        
        lmx::ProgramASTNode* debug_ast = parse(debug_test);
        if (!debug_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value debug_result = lm::irgen::execute(debug_ast);
        delete debug_ast;
        std::cout << "\nDebug decorator test passed!\n";
        
        std::cout << "\n3. Testing custom decorator...\n";
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
            
            print(say_hello("Alice"))
        )";
        
        lmx::ProgramASTNode* custom_ast = parse(custom_test);
        if (!custom_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        printAST(custom_ast);
        const irgen::Value custom_result = lm::irgen::execute(custom_ast);
        delete custom_ast;
        std::cout << "\nCustom decorator test passed!\n";
        
        std::cout << "\n=== Decorator Tests Completed ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nError in decorator test: " << e.what() << std::endl;
    }
}

void run_test() {
    std::cout << "Testing decorators..." << std::endl;
    test_decorators();
    
    std::cout << "Testing fib speed..." << std::endl;
    test_fib_speed();
    std::cout << "\nPress Enter to continue to REPL...";
    std::cin.get();
}

int main(const int argc, char* argv[]) {
    //run_test();
    const auto [
        show_help,
        show_version,
        file_path,
        script_args
    ] = cli::parse_args(argc, argv);

    if (show_help) {
        cli::show_help();
        return EXIT_SUCCESS;
    }

    if (show_version) {
        cli::show_version();
        return EXIT_SUCCESS;
    }

    if (!file_path.empty()) {
        return cli::run_file(file_path, script_args);
    }

    return cli::run_repl();
}
