#pragma once

#include "compiler/ast.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/hir.hpp"
#include "compiler/source.hpp"

namespace qpc {

HirModule lower(const Source& src, AstFile ast, DiagnosticEngine& diags);

}  // namespace qpc
