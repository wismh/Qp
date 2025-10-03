#pragma once

#include <memory>

namespace qpc {


struct Expr;
struct Stmt;
struct Pat;
struct Block;
struct AstFile;
class Source;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using PatPtr = std::unique_ptr<Pat>;

}  // namespace qpc
