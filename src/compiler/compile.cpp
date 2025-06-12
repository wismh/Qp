#include "compiler/compile.hpp"

#include "compiler/lexer.hpp"
#include "compiler/lower.hpp"
#include "compiler/parser.hpp"
#include "compiler/typeck.hpp"

#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

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

bool exists_regular(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

std::string canon_key(const fs::path& path) {
    std::error_code ec;
    auto canon = fs::weakly_canonical(path, ec);
    if (ec) {
        canon = fs::absolute(path, ec);
        if (ec) {
            return path.generic_string();
        }
    }
    return canon.generic_string();
}

bool resolve_file_mods(AstFile& file, const Source& src, const fs::path& mod_dir,
                       std::deque<Source>& extras, std::unordered_set<std::string>& loading,
                       DiagnosticEngine& diags);

bool load_file_module(ModDecl& m, const Source& parent, const fs::path& mod_dir,
                      std::deque<Source>& extras, std::unordered_set<std::string>& loading,
                      DiagnosticEngine& diags) {
    const fs::path file_qp = mod_dir / (m.name + ".qp");
    const fs::path dir_qp = mod_dir / m.name / "mod.qp";
    const bool has_file = exists_regular(file_qp);
    const bool has_dir = exists_regular(dir_qp);
    if (has_file && has_dir) {
        diags.error(parent, m.offset, "module '" + m.name + "' has both '" + file_qp.generic_string() +
                                          "' and '" + dir_qp.generic_string() + "'");
        return false;
    }
    if (!has_file && !has_dir) {
        diags.error(parent, m.offset, "cannot find module '" + m.name + "', expected '" +
                                          file_qp.generic_string() + "' or '" + dir_qp.generic_string() + "'");
        return false;
    }

    const fs::path path = has_file ? file_qp : dir_qp;
    const auto key = canon_key(path);
    if (loading.contains(key)) {
        diags.error(parent, m.offset, "cyclic module '" + m.name + "'");
        return false;
    }

    try {
        extras.push_back(Source::from_file(path.string()));
    } catch (const std::exception& ex) {
        diags.error(parent, m.offset, ex.what());
        return false;
    }

    Source& child_src = extras.back();
    m.source = &child_src;
    loading.insert(key);

    spdlog::debug("mod {} -> {}", m.name, child_src.path());
    const auto tokens = lex(child_src, diags);
    if (diags.has_errors()) {
        loading.erase(key);
        return false;
    }
    AstFile ast = parse(child_src, tokens, diags);
    if (diags.has_errors()) {
        loading.erase(key);
        return false;
    }
    m.body = std::make_unique<AstFile>(std::move(ast));
    const bool ok = resolve_file_mods(*m.body, child_src, mod_dir / m.name, extras, loading, diags);
    loading.erase(key);
    return ok;
}

bool resolve_file_mods(AstFile& file, const Source& src, const fs::path& mod_dir,
                       std::deque<Source>& extras, std::unordered_set<std::string>& loading,
                       DiagnosticEngine& diags) {
    std::unordered_set<std::string> names;
    for (auto& m : file.mods) {
        if (!names.insert(m.name).second) {
            diags.error(src, m.offset, "duplicate module '" + m.name + "'");
            return false;
        }
        if (m.file) {
            if (!load_file_module(m, src, mod_dir, extras, loading, diags)) {
                return false;
            }
            continue;
        }
        if (m.body && !resolve_file_mods(*m.body, src, mod_dir / m.name, extras, loading, diags)) {
            return false;
        }
    }
    return true;
}

}  // namespace

CompileResult compile_to_memory(const Source& src, DiagnosticEngine& diags) {
    CompileResult result;

    spdlog::debug("lex {}", src.path());
    const auto tokens = lex(src, diags);
    if (diags.has_errors()) {
        return result;
    }

    spdlog::debug("parse {}", src.path());
    AstFile ast = parse(src, tokens, diags);
    if (diags.has_errors()) {
        return result;
    }

    std::deque<Source> extras;
    std::unordered_set<std::string> loading;
    fs::path root(src.path());
    fs::path dir = root.parent_path();
    if (dir.empty()) {
        dir = ".";
    }
    if (!src.path().empty()) {
        loading.insert(canon_key(root));
    }
    spdlog::debug("resolve modules in {}", dir.generic_string());
    if (!resolve_file_mods(ast, src, dir, extras, loading, diags) || diags.has_errors()) {
        return result;
    }

    spdlog::debug("lower {}", src.path());
    HirModule hir = lower(src, std::move(ast), diags);
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
