#pragma once

#include "compiler/ast.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/source.hpp"
#include "compiler/token.hpp"

#include <deque>
#include <vector>

namespace qpc {

struct ParseResult {
    std::vector<Token> tokens;
    AstFile ast;
    bool parsed = false;
};

ParseResult parse_with_mods(const Source& src, std::deque<Source>& extras, DiagnosticEngine& diags,
                            bool stop_on_error);

}  // namespace qpc
