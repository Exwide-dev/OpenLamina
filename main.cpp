#include "tools/cli.hpp"
#include "tools/utf8_io.hpp"

#include <filesystem>

namespace {

bool is_lmc_file(const std::string& path) {
    return path.size() >= 4 && path.ends_with(".lmc");
}

} // namespace

int main(const int argc, char* argv[]) {
    lm::utf8_io::init_stdio_utf8();

    const cli::Args args = cli::parse_args(argc, argv);
    cli::apply_runtime_flags(args);

    if (args.show_help) {
        cli::show_help();
        return EXIT_SUCCESS;
    }

    if (args.show_version) {
        cli::show_version();
        return EXIT_SUCCESS;
    }

    if (!args.file_path.empty()) {
        if (args.check) {
            return cli::run_check(args.file_path, args.view_log);
        }
        if (args.compile) {
            return cli::compile_file(args.file_path, args.output_path);
        }
        if (is_lmc_file(args.file_path)) {
            return cli::run_bytecode_file(args.file_path);
        }
        return cli::run_file(args.file_path, args.script_args);
    }

    return cli::run_repl();
}
