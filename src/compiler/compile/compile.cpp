#include "compiler/compile.hpp"

#include "compiler/lexer.hpp"
#include "compiler/lower.hpp"
#include "compiler/packages.hpp"
#include "compiler/parser.hpp"
#include "compiler/typeck.hpp"

#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <unordered_map>
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

struct ModLookup {
    fs::path file_qp;
    fs::path dir_qp;
    fs::path child_mod_dir;
};

std::optional<ModLookup> local_mod_paths(const fs::path& mod_dir, const std::string& name) {
    ModLookup out;
    out.file_qp = mod_dir / (name + ".qp");
    out.dir_qp = mod_dir / name / "mod.qp";
    out.child_mod_dir = mod_dir / name;
    return out;
}

std::optional<ModLookup> package_mod_paths(const PackageManifest& packages, const std::string& name) {
    auto it = packages.dependencies.find(name);
    if (it == packages.dependencies.end()) {
        return std::nullopt;
    }
    ModLookup out;
    out.file_qp = it->second / (name + ".qp");
    out.dir_qp = it->second / "mod.qp";
    out.child_mod_dir = it->second;
    return out;
}

bool resolve_file_mods(AstFile& file, const Source& src, const fs::path& mod_dir,
                       const PackageManifest* packages, std::deque<Source>& extras,
                       std::unordered_set<std::string>& loading, DiagnosticEngine& diags);

bool load_file_module(ModDecl& m, const Source& parent, const fs::path& mod_dir,
                      const PackageManifest* packages, std::deque<Source>& extras,
                      std::unordered_set<std::string>& loading, DiagnosticEngine& diags) {
    auto local = *local_mod_paths(mod_dir, m.name);
    bool has_file = exists_regular(local.file_qp);
    bool has_dir = exists_regular(local.dir_qp);
    ModLookup chosen = local;
    const char* via = "local";

    if (!has_file && !has_dir && packages) {
        if (auto pkg = package_mod_paths(*packages, m.name)) {
            chosen = *pkg;
            has_file = exists_regular(chosen.file_qp);
            has_dir = exists_regular(chosen.dir_qp);
            via = "package";
        }
    }

    if (has_file && has_dir) {
        diags.error(parent, m.offset, "module '" + m.name + "' has both '" + chosen.file_qp.generic_string() +
                                          "' and '" + chosen.dir_qp.generic_string() + "'");
        return false;
    }
    if (!has_file && !has_dir) {
        std::string msg = "cannot find module '" + m.name + "', expected '" + local.file_qp.generic_string() +
                          "' or '" + local.dir_qp.generic_string() + "'";
        if (packages && packages->dependencies.contains(m.name)) {
            auto pkg = package_mod_paths(*packages, m.name);
            msg += ", or package path '" + pkg->child_mod_dir.generic_string() + "'";
        }
        diags.error(parent, m.offset, std::move(msg));
        return false;
    }

    const fs::path path = has_file ? chosen.file_qp : chosen.dir_qp;
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

    spdlog::debug("mod {} -> {} ({})", m.name, child_src.path(), via);
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
    const bool ok =
        resolve_file_mods(*m.body, child_src, chosen.child_mod_dir, packages, extras, loading, diags);
    loading.erase(key);
    return ok;
}

bool resolve_file_mods(AstFile& file, const Source& src, const fs::path& mod_dir,
                       const PackageManifest* packages, std::deque<Source>& extras,
                       std::unordered_set<std::string>& loading, DiagnosticEngine& diags) {
    std::unordered_set<std::string> names;
    for (const auto& m : file.mods) {
        names.insert(m.name);
    }
    for (const auto& u : file.uses) {
        if (!u.from_load || u.path.empty()) {
            continue;
        }
        const auto& name = u.path.front();
        if (!names.insert(name).second) {
            continue;
        }
        ModDecl m;
        m.file = true;
        m.name = name;
        m.offset = u.offset;
        m.body = std::make_unique<AstFile>();
        file.mods.push_back(std::move(m));
    }
    names.clear();
    for (auto& m : file.mods) {
        if (!names.insert(m.name).second) {
            diags.error(src, m.offset, "duplicate module '" + m.name + "'");
            return false;
        }
        if (m.file) {
            if (!load_file_module(m, src, mod_dir, packages, extras, loading, diags)) {
                return false;
            }
            continue;
        }
        if (m.body &&
            !resolve_file_mods(*m.body, src, mod_dir / m.name, packages, extras, loading, diags)) {
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

    std::optional<PackageManifest> packages;
    if (!src.path().empty()) {
        if (auto manifest = find_packages_toml(dir)) {
            spdlog::debug("packages.toml {}", manifest->generic_string());
            packages = load_packages_toml(*manifest, diags);
            if (diags.has_errors()) {
                return result;
            }
        }
    }

    spdlog::debug("resolve modules in {}", dir.generic_string());
    const PackageManifest* packages_ptr = packages ? &*packages : nullptr;
    if (!resolve_file_mods(ast, src, dir, packages_ptr, extras, loading, diags) || diags.has_errors()) {
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
