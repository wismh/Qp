#include "compiler/frontend.hpp"

#include "compiler/lexer.hpp"
#include "compiler/packages.hpp"
#include "compiler/parser.hpp"

#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_set>
#include <utility>

namespace qpc {
namespace {

namespace fs = std::filesystem;

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

ModLookup local_mod_paths(const fs::path& mod_dir, const std::string& name) {
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
    auto local = local_mod_paths(mod_dir, m.name);
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

ParseResult parse_with_mods(const Source& src, std::deque<Source>& extras, DiagnosticEngine& diags,
                            bool stop_on_error) {
    ParseResult result;

    spdlog::debug("lex {}", src.path());
    result.tokens = lex(src, diags);
    if (stop_on_error && diags.has_errors()) {
        return result;
    }

    const std::size_t before_parse = diags.size();
    spdlog::debug("parse {}", src.path());
    result.ast = parse(src, result.tokens, diags);
    result.parsed = diags.size() == before_parse;
    if (!result.parsed) {
        return result;
    }
    if (stop_on_error && diags.has_errors()) {
        result.parsed = false;
        return result;
    }

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
            if (stop_on_error && diags.has_errors()) {
                result.parsed = false;
                return result;
            }
        }
    }

    spdlog::debug("resolve modules in {}", dir.generic_string());
    const PackageManifest* packages_ptr = packages ? &*packages : nullptr;
    const std::size_t before_mods = diags.size();
    const bool mods_ok = resolve_file_mods(result.ast, src, dir, packages_ptr, extras, loading, diags);
    if (stop_on_error && (!mods_ok || diags.size() > before_mods || diags.has_errors())) {
        result.parsed = false;
        return result;
    }

    return result;
}

}  // namespace qpc
