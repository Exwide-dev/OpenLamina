#include <iostream>
#include <vector>
#include <string>
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


// 存储解析结果

// yylval 由 Bison 自动定义

// 声明 Flex 函数

extern "C" {
    void yy_delete_buffer(void*);
}

// 打印 AST 节点


std::string get_compiler() {
#if defined(__clang__)
    return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "Unknown compiler";
#endif
}

// 编译日期和时间（自动获取）
#define OPENLAMINA_BUILD_DATE __DATE__
#define OPENLAMINA_BUILD_TIME __TIME__

// Git 提交哈希（需要在编译时通过 CMake 传入）
#ifndef OPENLAMINA_BUILD_HASH
#define OPENLAMINA_BUILD_HASH ""
#endif

namespace {
    std::string welcome = std::format(
        R"(OpenLamina REPL v{}.{}.{}, built by {} ({}), {} {} {}
Powered by Flex and Bison
Contact OpenLamina-Developing for more information)",
        OPENLAMINA_VERSION_MAJOR,
        OPENLAMINA_VERSION_MINOR,
        OPENLAMINA_VERSION_PATCH,
        get_compiler(),
#if defined(__x86_64__) || defined(_M_X64)
        "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
        "ARM64"
#else
        "Unknown"
#endif
        ,OPENLAMINA_BUILD_DATE,
        OPENLAMINA_BUILD_TIME,
        OPENLAMINA_BUILD_HASH
    );
}
void test_code1() {
    LOG("Test LABEL & DOTO");

    irgen::VM vm;
    const std::vector<irgen::Opcode*> test_code = {
        new irgen::PUSH(irgen::Value(static_cast<size_t>(1))),
        new irgen::GOTO(irgen::Value(std::string("skip"))),
        new irgen::PUSH(irgen::Value(static_cast<size_t>(2))),
        new irgen::LABEL(irgen::Value("skip")),
        new irgen::PUSH(irgen::Value(static_cast<size_t>(3))),
    };

    for (auto* op : test_code) {
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

[[noreturn]] int main() {
    std::cout << welcome << std::endl;
    auto repl = repl::REPL();
    while (true) {
        try {
            if (repl.exec_input([&]() -> std::string {
               std::cout << "\n>>> ";
               std::string line;
               std::getline(std::cin, line);
               return line;
           })) {
                if (!repl.vm.op_stack.empty()) {
                    std::cout << repl.vm.op_stack.top() << std::endl;
                }
            }
        } catch (const RuntimeError &e) {
            std::cout << "RuntimeError: " << e.what() << std::endl;
            repl.vm.pc = repl.vm.code.size();
        } catch (const SyntaxError &e) {
            std::cout << "SyntaxError: " << e.what() << std::endl;
        }
    }
}
