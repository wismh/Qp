#pragma once

#include "compiler/diagnostic.hpp"
#include "compiler/source.hpp"
#include "compiler/token.hpp"

#include <vector>

namespace qpc {

std::vector<Token> lex(const Source& src, DiagnosticEngine& diags);

}  // namespace qpc
