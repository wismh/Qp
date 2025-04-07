#pragma once

#include "compiler/cpp_backend.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/source.hpp"

#include <filesystem>

namespace qpc {

struct CompileResult {
    bool ok = false;
    CppOutput output;
};

CompileResult compile_to_memory(const Source& src, DiagnosticEngine& diags);
bool compile_file(const std::filesystem::path& input, const std::filesystem::path& out_dir,
                  DiagnosticEngine& diags);

}  // namespace qpc
