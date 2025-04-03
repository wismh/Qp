#pragma once

#include "compiler/ast.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/source.hpp"
#include "compiler/token.hpp"

#include <vector>

namespace qpc {

AstFile parse(const Source& src, const std::vector<Token>& tokens, DiagnosticEngine& diags);

}  // namespace qpc
