#include "compiler/compile.hpp"
#include "compiler/lsp.hpp"

#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

void print_usage() {
    std::cerr << "usage: qpc compile <file.qp> -o <dir> [-v|--verbose]\n"
              << "       qpc lsp\n";
}

void set_stdio_binary() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

}  // namespace

int main(int argc, char** argv) {
    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);

    const std::vector<std::string_view> args(argv + 1, argv + argc);

    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        print_usage();
        return args.empty() ? 1 : 0;
    }

    if (args[0] == "lsp") {
        const bool verbose =
            args.size() > 1 && (args[1] == "-v" || args[1] == "--verbose");
        spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::err);
        set_stdio_binary();
        return qpc::lsp::run_lsp(std::cin, std::cout);
    }

    if (args[0] != "compile") {
        std::cerr << "unknown command '" << args[0] << "'\n";
        print_usage();
        return 1;
    }

    std::string input;
    std::string out_dir;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-v" || args[i] == "--verbose") {
            spdlog::set_level(spdlog::level::debug);
            continue;
        }

        if (args[i] == "-o") {
            if (i + 1 >= args.size()) {
                std::cerr << "missing value for -o\n";
                return 1;
            }
            out_dir = std::string(args[++i]);
            continue;
        }

        if (args[i].starts_with('-')) {
            std::cerr << "unknown option '" << args[i] << "'\n";
            print_usage();
            return 1;
        }

        if (!input.empty()) {
            std::cerr << "unexpected argument '" << args[i] << "'\n";
            print_usage();
            return 1;
        }
        input = std::string(args[i]);
    }

    if (input.empty() || out_dir.empty()) {
        print_usage();
        return 1;
    }

    spdlog::info("compile {} -> {}", input, out_dir);
    qpc::DiagnosticEngine diags;
    if (!qpc::compile_file(input, out_dir, diags)) {
        diags.print(std::cerr);
        return 1;
    }
    return 0;
}
