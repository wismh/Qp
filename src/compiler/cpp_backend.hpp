#pragma once

#include "compiler/hir.hpp"
#include "compiler/source.hpp"

#include <string>

namespace qpc {

struct CppOutput {
    std::string header;
    std::string source;
    std::string stem;
};

CppOutput emit_cpp(const Source& src, const HirModule& mod);

}  // namespace qpc
