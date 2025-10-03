#pragma once

#include <memory>

namespace qpc {


class Source;

struct HirExpr;
struct HirStmt;
struct HirPat;

using HirExprPtr = std::unique_ptr<HirExpr>;
using HirStmtPtr = std::unique_ptr<HirStmt>;
using HirPatPtr = std::unique_ptr<HirPat>;

}  // namespace qpc
