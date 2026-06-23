#include "tools/cli.hpp"

int main(const int argc, char* argv[]) {
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
        return cli::run_file(args.file_path, args.script_args);
    }

    return cli::run_repl();
}
