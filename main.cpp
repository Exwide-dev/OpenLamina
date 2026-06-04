#include "tools/cli.hpp"

int main(const int argc, char* argv[]) {
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
