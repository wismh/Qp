#pragma once

#include "compiler/diagnostic.hpp"
#include "compiler/hir.hpp"
#include "compiler/source.hpp"

namespace qpc {

void typeck(const Source& src, HirModule& mod, DiagnosticEngine& diags);

}  // namespace qpc
