#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

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

void test_mod_intern_export() {
    std::cout << "\n=== Testing intern/export Variable Visibility ===\n";
    
    try {
        std::cout << "\n1. Testing local variable declarations...\n";
        const std::string local_test = R"(
            intern secret = "secret_value"
            export public_data = "public_value"
            default_export = "default_value"
            intern const PI = 3141592653
            export const MAX_SIZE = 100
            
            print("Accessing internal var in same module:")
            print(secret)
            
            print("\nAccessing exported var in same module:")
            print(public_data)
            
            print("\nAccessing default export var in same module:")
            print(default_export)
            
            print("\nAccessing internal const in same module:")
            print(PI)
            
            print("\nAccessing exported const in same module:")
            print(MAX_SIZE)
        )";
        
        lmx::ProgramASTNode* local_ast = parse(local_test);
        if (!local_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value local_result = lm::irgen::execute(local_ast);
        delete local_ast;
        std::cout << "\nLocal variable test passed!\n";
        
        std::cout << "\n2. Creating test modules...\n";
        std::filesystem::path test_dir = std::filesystem::current_path() / "test_visibility";
        if (!std::filesystem::exists(test_dir)) {
            std::filesystem::create_directory(test_dir);
        }
        
        std::ofstream intern_module(test_dir / "intern_module.lm");
        intern_module << R"(
            intern internal_var = "I'm internal"
            export external_var = "I'm external"
            auto_export = "I'm auto-exported"
            
            intern func internal_func(x) { return x * 2 }
            export func external_func(x) { return x + 10 }
        )";
        intern_module.close();
        
        std::cout << "Test module created at: " << test_dir / "intern_module.lm" << "\n";
        
        std::cout << "\n3. Testing module import and access...\n";
        const std::string import_test = R"(
            import test_visibility.intern_module as mod
            
            print("Accessing exported var from module:")
            print(mod.external_var)
            
            print("\nAccessing auto-export var from module:")
            print(mod.auto_export)
            
            print("\nAccessing exported function from module:")
            print(mod.external_func(5))
        )";
        
        lmx::ProgramASTNode* import_ast = parse(import_test);
        if (!import_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value import_result = lm::irgen::execute(import_ast);
        delete import_ast;
        std::cout << "\nExported access test passed!\n";
        
        std::cout << "\n4. Testing access to internal variable from another module (should fail)...\n";
        const std::string access_internal_test = R"(
            import test_visibility.intern_module as mod
            print(mod.internal_var)
        )";
        
        lmx::ProgramASTNode* internal_access_ast = parse(access_internal_test);
        if (!internal_access_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        
        try {
            const ::irgen::Value internal_access_result = lm::irgen::execute(internal_access_ast);
            std::cout << "ERROR: Should have failed to access internal var!\n";
        } catch (const std::exception& e) {
            std::cout << "Expected error (internal var access denied): " << e.what() << "\n";
            std::cout << "Internal var protection test passed!\n";
        }
        delete internal_access_ast;
        
        std::cout << "\n5. Testing intern const and export const...\n";
        const std::string const_test = R"(
            intern const INTERNAL_CONST = 42
            export const EXPORT_CONST = 100
            
            print("Internal const:")
            print(INTERNAL_CONST)
            
            print("\nExport const:")
            print(EXPORT_CONST)
        )";
        
        lmx::ProgramASTNode* const_ast = parse(const_test);
        if (!const_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value const_result = lm::irgen::execute(const_ast);
        delete const_ast;
        std::cout << "\nConst test passed!\n";
        
        std::cout << "\n6. Testing that internal function cannot be accessed externally...\n";
        const std::string func_access_test = R"(
            import test_visibility.intern_module as mod
            print(mod.internal_func(5))
        )";
        
        lmx::ProgramASTNode* func_access_ast = parse(func_access_test);
        if (!func_access_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        
        try {
            const ::irgen::Value func_access_result = lm::irgen::execute(func_access_ast);
            std::cout << "ERROR: Should have failed to access internal function!\n";
        } catch (const std::exception& e) {
            std::cout << "Expected error (internal func access denied): " << e.what() << "\n";
            std::cout << "Internal function protection test passed!\n";
        }
        delete func_access_ast;
        
        std::filesystem::remove_all(test_dir);
        std::cout << "\n=== intern/export Visibility Tests Completed ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nError in intern/export test: " << e.what() << std::endl;
    }
}

void test_decorators() {
    std::cout << "\n=== Testing Decorators ===\n";
    
    try {
        std::cout << "\n1. Testing std.decos.log decorator...\n";
        const std::string log_test = R"(
            with std.decos.log make func greet(name) {
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
            with std.decos.debug make func add(a, b) {
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
            
            with make_upper make func say_hello(name) {
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
        
        std::cout << "\n4. Testing multiple decorators...\n";
        const std::string multi_test = R"(
            func repeat_twice(f) {
                do(x) {
                    return f(f(x))
                }
            }
            
            func add_one(f) {
                do(x) {
                    return f(x) + 1
                }
            }
            
            with repeat_twice add_one make func increment(x) {
                return x + 10
            }
            
            print(increment(5))
        )";
        
        lmx::ProgramASTNode* multi_ast = parse(multi_test);
        if (!multi_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value multi_result = lm::irgen::execute(multi_ast);
        delete multi_ast;
        std::cout << "\nMultiple decorators test passed!\n";
        
        std::cout << "\n5. Testing decorated lambda...\n";
        const std::string lambda_test = R"(
            func square(f) {
                do(x) {
                    let res = f(x)
                    return res * res
                }
            }
            
            let doubled = with square make do(x) {
                return x * 2
            }

            print(doubled(3))
        )";
        
        lmx::ProgramASTNode* lambda_ast = parse(lambda_test);
        if (!lambda_ast) {
            std::cout << "Parse failed!\n";
            return;
        }
        const ::irgen::Value lambda_result = lm::irgen::execute(lambda_ast);
        delete lambda_ast;
        std::cout << "\nDecorated lambda test passed!\n";
        
        std::cout << "\n=== Decorator Tests Completed ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nError in decorator test: " << e.what() << std::endl;
    }
}

void run_test() {
    /*std::cout << "Testing module system..." << std::endl;
    test_module_system();
    std::cout << "Testing intern/export visibility..." << std::endl;
    test_mod_intern_export();*/
    std::cout << "Testing decorators..." << std::endl;
    test_decorators();
    /*std::cout << "Testing fib speed..." << std::endl;
    test_fib_speed();*/
    std::cout << "\nPress Enter to continue to REPL...";
    std::cin.get();
}

int main(const int argc, char* argv[]) {
    run_test();
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
