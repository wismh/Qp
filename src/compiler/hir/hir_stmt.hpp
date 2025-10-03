#pragma once

#include "compiler/hir/hir_let.hpp"
#include "compiler/hir/hir_return.hpp"
#include "compiler/hir/hir_expr_stmt.hpp"
#include "compiler/hir/hir_while.hpp"
#include "compiler/hir/hir_for.hpp"
#include "compiler/hir/hir_break.hpp"
#include "compiler/hir/hir_continue.hpp"
#include <cstddef>
#include <variant>

namespace qpc {

struct HirStmt {
    std::size_t offset = 0;
    std::variant<HirLet, HirReturn, HirExprStmt, HirWhile, HirFor, HirBreak, HirContinue> kind;
};

}  // namespace qpc
