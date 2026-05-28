#include "cli.hpp"
#include "error.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "repl/repl.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace cli {

    Args parse_args(const int argc, char* argv[]) {
        Args args;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                args.show_help = true;
            } else if (arg == "-v" || arg == "--version") {
                args.show_version = true;
            } else if (args.file_path.empty()) {
                args.file_path = arg;
            } else {
                args.script_args.push_back(arg);
            }
        }
        
        return args;
    }

    int run_file(const std::string& file_path, const std::vector<std::string>& args) {
        try {
            if (!std::filesystem::exists(file_path)) {
                std::cerr << "Error: File not found: " << file_path << std::endl;
                return 1;
            }

            std::ifstream file(file_path);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file: " << file_path << std::endl;
                return 1;
            }

            std::string source((std::istreambuf_iterator(file)),
                             std::istreambuf_iterator<char>());
            file.close();

            lmx::ProgramASTNode* ast = parse(source, file_path);
            if (!ast) {
                std::cerr << "\nParsing failed:" << std::endl;
                std::cerr << detail_msg << std::endl;
                return 1;
            }

            ::irgen::Value result = lm::irgen::execute(ast);
            delete ast;

            return 0;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

    int run_repl() {
        std::string welcome = []() -> std::string {
            auto get_compiler = []() -> std::string {
#define OPENLAMINA_BUILD_DATE __DATE__
#define OPENLAMINA_BUILD_TIME __TIME__
#if defined(__clang__)
                return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
                return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
                return "MSVC " + std::to_string(_MSC_VER);
#else
                return "Unknown compiler";
#endif
            };

            auto get_arch = []() -> std::string {
#if defined(__x86_64__) || defined(_M_X64)
                return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
                return "ARM64";
#else
                return "Unknown";
#endif
            };

            std::string hash_part =
        #ifdef OPENLAMINA_BUILD_HASH
                std::string(" ") + OPENLAMINA_BUILD_HASH;
#else
                    "";
#endif

            return std::format(
                R"(OpenLamina REPL v{}.{}.{}, built by {} ({}), {} {} {}, C++ standard {}
Powered by Flex and Bison
Contact OpenLamina-Developing for more information)",
                OPENLAMINA_VERSION_MAJOR,
                OPENLAMINA_VERSION_MINOR,
                OPENLAMINA_VERSION_PATCH,
                get_compiler(),
                get_arch(),
                OPENLAMINA_BUILD_DATE,
                OPENLAMINA_BUILD_TIME,
                hash_part,
                __cplusplus
            );
#undef OPENLAMINA_BUILD_HASH
#undef OPENLAMINA_BUILD_DATE
#undef OPENLAMINA_BUILD_TIME
        }();

        std::cout << welcome << std::endl;

        repl::REPL repl_instance;
        bool needs_more_input = false;
        
        while (true) {
            try {
                std::cout << (needs_more_input ? "... " : ">>> ");
                
                repl::REPL::ExecResult result = repl_instance.exec_input();
                
                if (result.success && !result.needs_more_input) {
                    if (!repl_instance.vm.op_stack.empty()) {
                        auto top = repl_instance.vm.op_stack.top();
                        if (!top.isNone()) {
                            std::cout << top << std::endl;
                        }
                    }
                    repl_instance.vm.op_stack.clear();
                }
                
                needs_more_input = result.needs_more_input;
                
            } catch (const RuntimeError& e) {
                std::cout << "\nRuntimeError: " << e.what() << std::endl;
                repl_instance.vm.pc = repl_instance.vm.code.size();
                std::cout << "Traceback: " << [&]{
                    std::string result;
                    for (const auto& call: repl_instance.vm.traceback) {
                        result.append(call + " -> ");
                    }
                    return result.substr(0, result.size() - 4);
                }() << std::endl;
                needs_more_input = false;
            } catch (const SyntaxError& e) {
                std::cout << "\nSyntaxError:" << detail_msg << std::endl;
                needs_more_input = false;
            } catch (const std::exception& e) {
                std::cerr << "Occured an exception: " << e.what()
                          << "\nExit." << std::endl;
                throw;
            }
        }
    }

    void show_help() {
        std::cout << 
            "Usage: OpenLamina [options] [file]\n\n"
            "Options:\n"
            "  -h, --help     Show this help message\n"
            "  -v, --version  Show version information\n\n"
            "If no file is specified, the REPL will start.\n"
            "If a file is specified, it will be executed.\n"
            << std::endl;
    }

    void show_version() {
        std::cout << "OpenLamina v"
            << OPENLAMINA_VERSION_MAJOR << "."
            << OPENLAMINA_VERSION_MINOR << "."
            << OPENLAMINA_VERSION_PATCH << std::endl;
    }

} // namespace cli
