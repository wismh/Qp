#include "compiler/packages.hpp"

#include <toml++/toml.hpp>

#include <exception>
#include <string>
#include <utility>

namespace qpc {
namespace {

namespace fs = std::filesystem;

std::string path_msg(const fs::path& path) { return path.generic_string(); }

}  // namespace

std::optional<fs::path> find_packages_toml(const fs::path& start_dir) {
    std::error_code ec;
    fs::path dir = fs::absolute(start_dir, ec);
    if (ec) {
        dir = start_dir;
    }
    while (true) {
        const fs::path candidate = dir / "packages.toml";
        if (fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
        const fs::path parent = dir.parent_path();
        if (parent == dir) {
            break;
        }
        dir = parent;
    }
    return std::nullopt;
}

std::optional<PackageManifest> load_packages_toml(const fs::path& path, DiagnosticEngine& diags) {
    PackageManifest out;
    out.manifest_path = path;
    std::error_code ec;
    out.root_dir = fs::absolute(path.parent_path(), ec);
    if (ec) {
        out.root_dir = path.parent_path();
    }

    toml::table tbl;
    try {
        tbl = toml::parse_file(path.string());
    } catch (const toml::parse_error& err) {
        const auto& src = err.source();
        diags.error(path_msg(path), static_cast<std::uint32_t>(src.begin.line),
                    static_cast<std::uint32_t>(src.begin.column),
                    std::string("packages.toml: ") + std::string(err.description()));
        return std::nullopt;
    } catch (const std::exception& ex) {
        diags.error(path_msg(path), 1, 1, std::string("packages.toml: ") + ex.what());
        return std::nullopt;
    }

    const auto* deps = tbl["dependencies"].as_table();
    if (!deps) {
        return out;
    }

    for (const auto& [key, value] : *deps) {
        const std::string name(key.str());
        const auto* entry = value.as_table();
        if (!entry) {
            diags.error(path_msg(path), 1, 1,
                        "packages.toml: dependency '" + name + "' must be a table with path");
            return std::nullopt;
        }
        const auto* path_node = (*entry)["path"].as_string();
        if (!path_node) {
            diags.error(path_msg(path), 1, 1,
                        "packages.toml: dependency '" + name + "' needs a string 'path'");
            return std::nullopt;
        }
        fs::path dep_path(path_node->get());
        if (dep_path.empty()) {
            diags.error(path_msg(path), 1, 1,
                        "packages.toml: dependency '" + name + "' has an empty path");
            return std::nullopt;
        }
        if (!dep_path.is_absolute()) {
            dep_path = out.root_dir / dep_path;
        }
        dep_path = fs::weakly_canonical(dep_path, ec);
        if (ec) {
            dep_path = fs::absolute(out.root_dir / path_node->get(), ec);
        }
        if (!fs::is_directory(dep_path, ec)) {
            diags.error(path_msg(path), 1, 1,
                        "packages.toml: dependency '" + name + "' path is not a directory: '" +
                            dep_path.generic_string() + "'");
            return std::nullopt;
        }
        if (!out.dependencies.emplace(name, std::move(dep_path)).second) {
            diags.error(path_msg(path), 1, 1,
                        "packages.toml: duplicate dependency '" + name + "'");
            return std::nullopt;
        }
    }

    return out;
}

}  // namespace qpc
