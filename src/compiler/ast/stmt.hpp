#pragma once

#include "compiler/ast/stmt_let.hpp"
#include "compiler/ast/stmt_return.hpp"
#include "compiler/ast/stmt_expr.hpp"
#include "compiler/ast/stmt_while.hpp"
#include "compiler/ast/stmt_for.hpp"
#include "compiler/ast/stmt_break.hpp"
#include "compiler/ast/stmt_continue.hpp"
#include <cstddef>
#include <variant>

namespace qpc {

struct Stmt {
    std::size_t offset = 0;
    std::variant<StmtLet, StmtReturn, StmtExpr, StmtWhile, StmtFor, StmtBreak, StmtContinue> kind;
};

}  // namespace qpc
