#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "lexer_generated.h"
#include "parser.tab.hpp"
#include "lexer/lex.hpp"
#include "lexer/token.hpp"
#include "parser/ast.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "irgen/opcode.hpp"
#include "repl/repl.hpp"
#include "tools/error.hpp"
#include "parser/ast.hpp"
#include "tools/cli.hpp"


// 存储解析结果

// yylval 由 Bison 自动定义

// 声明 Flex 函数

extern "C" {
    void yy_delete_buffer(void*);
}

// 打印 AST 节点

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

    // 解析并显示 AST
    std::cout << "\nParsing...\n";
    const lmx::ProgramASTNode* ast = parse(source);
    printAST(ast);

    // 手动构建一个简单的 AST 进行测试
    std::cout << "\nTesting with manually built AST...\n";
    const auto* manual_ast = parse("let a = 10 + 5 * 2\na");
    printAST(manual_ast);

    const ::irgen::Value manual_result = lm::irgen::execute(manual_ast);
    if (!manual_result.isNone()) {
        std::cout << "Manual AST execution result: " << manual_result << std::endl;
    } else {
        std::cout << "Manual AST execution resulted in no value" << std::endl;
    }

    // 释放内存
    delete ast;
    delete manual_ast;
}

std::string test_fib_format(int n) {
    return std::format("fib({})", n);
}

void test_module_system() {
    std::cout << "\n=== Testing Module System ===\n";
    std::cout << "Current directory: " << std::filesystem::current_path() << "\n";
    
    try {
        const std::string simple_test = R"(
            let x = 42
            print("Hello World")
            x
        )";
        
        std::cout << "\nTesting basic execution...\n";
        lmx::ProgramASTNode* simple_ast = parse(simple_test);
        if (!simple_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value simple_result = lm::irgen::execute(simple_ast);
        std::cout << "Simple test result: " << simple_result << "\n";
        delete simple_ast;
        std::cout << "Basic execution test passed!\n";
        
        std::cout << "\nChecking for test_module.lm...\n";
        std::filesystem::path module_path = "test_module.lm";
        if (!std::filesystem::exists(module_path)) {
            module_path = std::filesystem::path(__FILE__).parent_path().parent_path() / "test_module.lm";
        }
        /*if (std::filesystem::exists(module_path)) {
            std::cout << "test_module.lm found!\n";
        } else {
            std::cout << "test_module.lm NOT found!\n";
            std::cout << "Module system test skipped (test file not found)\n";
            return;
        }*/
        
        const std::string import_test = R"(import test_module)";
        
        std::cout << "\nTesting basic import...\n";
        lmx::ProgramASTNode* import_ast = parse(import_test);
        if (!import_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value import_result = lm::irgen::execute(import_ast);
        delete import_ast;
        std::cout << "Basic import test passed!\n";
        
        const std::string alias_test = R"(
            import test_module as tm
            print(tm.greeting)
        )";
        
        std::cout << "\nTesting import with alias and usage...\n";
        lmx::ProgramASTNode* alias_ast = parse(alias_test);
        if (!alias_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value alias_result = lm::irgen::execute(alias_ast);
        delete alias_ast;
        std::cout << "Alias import test passed!\n";
        
        std::cout << "\n=== Module System Tests Completed ===\n";
    } catch (const std::exception& e) {
        std::cerr << "Error in module system test: " << e.what() << std::endl;
    }
}

void test_fib_speed() {
    irgen::VM vm;

    const std::string input =
R"(
func fib(n) { if (n <= 1) { return n } return fib(n - 1) + fib(n - 2) }
)";
    lmx::ProgramASTNode* got_ast = parse(input);

    auto code = lm::irgen::Generator(got_ast).gen();
    vm.code.insert(vm.code.end(), code.begin(), code.end());
    vm.run();

    const std::vector tests = {20, 25, 30};
    constexpr int n = 20;

    for (const auto& test : tests) {
        const auto tstart = clock();
        for (int i = 0; i < n; i++) {
            auto tcode = lm::irgen::Generator(parse(test_fib_format(test))).gen();
            std::cerr << "Testing " << test_fib_format(test) << std::endl;
            vm.code.insert(vm.code.end(), tcode.begin(), tcode.end());
            vm.run();
        }
        std::cerr << "Result: " << vm.op_stack.top() << std::endl;
        std::cerr << "Average Time: " << static_cast<long double>(clock() - tstart) / CLOCKS_PER_SEC / n << " s\n" << std::endl;
    }

    delete got_ast;
}

int main(const int argc, char* argv[]) {
    std::cout << "Testing module system..." << std::endl;
    test_module_system();
    std::cout << "Testing fib speed..." << std::endl;
    //test_fib_speed();
    //std::cout << "\nPress Enter to continue to REPL...";
    std::cin.get();
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
