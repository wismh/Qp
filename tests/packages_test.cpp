#include "compiler/diagnostic.hpp"
#include "compiler/packages.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST(Packages, FindWalksParents) {
    const auto dir = fs::temp_directory_path() / "qplus_pkg_find";
    fs::remove_all(dir);
    const auto nested = dir / "a" / "b";
    fs::create_directories(nested);
    {
        std::ofstream{(dir / "packages.toml").string()} << "[dependencies]\n";
    }

    const auto found = qpc::find_packages_toml(nested);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(fs::weakly_canonical(*found), fs::weakly_canonical(dir / "packages.toml"));
}

TEST(Packages, LoadPathDependency) {
    const auto dir = fs::temp_directory_path() / "qplus_pkg_load";
    fs::remove_all(dir);
    const auto dep = dir / "libs" / "math";
    fs::create_directories(dep);
    {
        std::ofstream{(dir / "packages.toml").string()} << R"(
[package]
name = "app"

[dependencies]
math = { path = "libs/math" }
)";
        std::ofstream{(dep / "mod.qp").string()} << "pub fn add(a: i32, b: i32) -> i32 { a + b }\n";
    }

    qpc::DiagnosticEngine diags;
    auto manifest = qpc::load_packages_toml(dir / "packages.toml", diags);
    ASSERT_TRUE(manifest.has_value()) << (diags.all().empty() ? "" : diags.all().front().message);
    ASSERT_TRUE(manifest->dependencies.contains("math"));
    EXPECT_TRUE(fs::equivalent(manifest->dependencies.at("math"), dep));
}

TEST(Packages, MissingPathIsError) {
    const auto dir = fs::temp_directory_path() / "qplus_pkg_missing";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream{(dir / "packages.toml").string()} << R"(
[dependencies]
math = { path = "no_such_dir" }
)";
    }

    qpc::DiagnosticEngine diags;
    auto manifest = qpc::load_packages_toml(dir / "packages.toml", diags);
    EXPECT_FALSE(manifest.has_value());
    ASSERT_TRUE(diags.has_errors());
    EXPECT_NE(diags.all().front().message.find("not a directory"), std::string::npos);
}
