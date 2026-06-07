#include "cli.hpp"
#include "error.hpp"
#include "front-end/front_end.hpp"
#include "irgen/generator.hpp"
#include "repl/repl.hpp"
#include "utf8_io.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ranges>

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

        file.close();
        const std::string source = lm::utf8_io::read_file_utf8(file_path);

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

namespace {
constexpr std::string RED_BOLD = "\033[1;31m";
constexpr std::string GREEN = "\033[0;32m";
constexpr std::string YELLOW = "\033[0;33m";
constexpr std::string BLUE = "\033[0;34m";
constexpr std::string MAGENTA = "\033[0;35m";
constexpr std::string CYAN = "\033[0;36m";
constexpr std::string RESET = "\033[0m";

std::string format_traceback(const std::vector<std::string>& traceback) {
    if (traceback.empty()) return "";

    std::string result = "\n" + YELLOW + "Traceback (most recent call last):" + RESET + "\n";
    int indent = 0;
    for (const auto& it : std::ranges::reverse_view(traceback)) {
        result += "  " + std::string(indent, ' ') + "-> " + BLUE + it + RESET + "\n";
        indent += 2;
    }
    return result;
}
}

int run_repl() {
    const std::string welcome = []() -> std::string {
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
    std::cout << std::endl;

    lm::utf8_io::init_stdio_utf8();

    repl::REPL repl_instance;
    bool needs_more_input = false;
    int line_number = 1;

    while (true) {
        try {
            std::cout << CYAN << (needs_more_input ? "... " : ">>> ") << RESET;

            const auto [success, if_need_more] = repl_instance.exec_input();

            if (success && !if_need_more) {
                if (!repl_instance.vm.op_stack.empty()) {
                    const auto& top = repl_instance.vm.op_stack.top();
                    const auto& shown = top.deref();
                    if (!shown.isNone()) {
                        std::cout << shown << std::endl;
                    }
                }
                repl_instance.vm.op_stack.clear();
                line_number++;
            }

            needs_more_input = if_need_more;
        } catch (const RuntimeError& e) {
            std::cout << "\n" << RED_BOLD << "Error: " << RESET << e.what() << "\n";
            std::cout << format_traceback(repl_instance.vm.traceback);

            repl_instance.vm.pc = repl_instance.vm.code.size();
            repl_instance.vm.traceback.clear();
            repl_instance.vm.op_stack.clear();
            needs_more_input = false;
            line_number++;
        } catch (const SyntaxError& e) {
            std::cout << "\n" << RED_BOLD << "SyntaxError:" << RESET << "\n";
            std::cout << e.what() << "\n";

            needs_more_input = false;
            line_number++;
        } catch (...) {
            std::cout << "\n" << RED_BOLD << "Error: " << RESET << "An unknown exception occurred.\n";
            std::cout << "\n" << YELLOW << "This is an unexpected error. Please report this bug." << RESET;
            std::cout << "\nExiting REPL...\n";
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
