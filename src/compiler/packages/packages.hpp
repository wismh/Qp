#pragma once

#include "compiler/diagnostic.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace qpc {

struct PackageManifest {
    std::filesystem::path manifest_path;
    std::filesystem::path root_dir;
    /// dependency name -> absolute module root directory
    std::unordered_map<std::string, std::filesystem::path> dependencies;
};

/// Walk parents of `start_dir` for `packages.toml`. Missing file is not an error.
[[nodiscard]] std::optional<std::filesystem::path> find_packages_toml(
    const std::filesystem::path& start_dir);

/// Load and validate a packages.toml. Relative dependency paths are resolved
/// against the manifest directory. On parse/validate failure returns nullopt and
/// reports through `diags`.
[[nodiscard]] std::optional<PackageManifest> load_packages_toml(const std::filesystem::path& path,
                                                                DiagnosticEngine& diags);

}  // namespace qpc
