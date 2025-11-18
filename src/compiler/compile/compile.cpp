#include "compiler/compile.hpp"

#include "compiler/frontend.hpp"
#include "compiler/lower.hpp"
#include "compiler/typeck.hpp"

#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string_view>

namespace qpc {
namespace {

namespace fs = std::filesystem;

bool write_file(const fs::path& path, std::string_view text, DiagnosticEngine& diags) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        diags.error(path.string(), 1, 1, "failed to write file");
        return false;
    }
    out << text;
    return true;
}

}  // namespace

CompileResult compile_to_memory(const Source& src, DiagnosticEngine& diags) {
    CompileResult result;

    std::deque<Source> extras;
    ParseResult parsed = parse_with_mods(src, extras, diags, true);
    if (!parsed.parsed || diags.has_errors()) {
        return result;
    }

    spdlog::debug("lower {}", src.path());
    HirModule hir = lower(src, std::move(parsed.ast), diags);
    if (diags.has_errors()) {
        return result;
    }

    spdlog::debug("typeck {}", src.path());
    typeck(src, hir, diags);
    if (diags.has_errors()) {
        return result;
    }

    spdlog::debug("codegen {}", src.path());
    result.output = emit_cpp(src, hir);
    result.ok = true;
    return result;
}

bool compile_file(const fs::path& input, const fs::path& out_dir, DiagnosticEngine& diags) {
    Source src;
    try {
        src = Source::from_file(input.string());
    } catch (const std::exception& ex) {
        diags.error(input.string(), 1, 1, ex.what());
        return false;
    }

    CompileResult compiled = compile_to_memory(src, diags);
    if (!compiled.ok) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        diags.error(out_dir.string(), 1, 1, "failed to create output directory: " + ec.message());
        return false;
    }

    const auto header_path = out_dir / (compiled.output.stem + ".h");
    const auto source_path = out_dir / (compiled.output.stem + ".cpp");
    if (!write_file(header_path, compiled.output.header, diags) ||
        !write_file(source_path, compiled.output.source, diags)) {
        return false;
    }

    spdlog::info("wrote {} and {}", header_path.generic_string(), source_path.generic_string());
    return true;
}

}  // namespace qpc
