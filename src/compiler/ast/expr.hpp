#pragma once

#include "compiler/ast/lit_int.hpp"
#include "compiler/ast/lit_float.hpp"
#include "compiler/ast/lit_bool.hpp"
#include "compiler/ast/lit_char.hpp"
#include "compiler/ast/lit_string.hpp"
#include "compiler/ast/lit_null.hpp"
#include "compiler/ast/expr_ident.hpp"
#include "compiler/ast/expr_path.hpp"
#include "compiler/ast/expr_binary.hpp"
#include "compiler/ast/expr_unary.hpp"
#include "compiler/ast/expr_call.hpp"
#include "compiler/ast/expr_assign.hpp"
#include "compiler/ast/expr_field.hpp"
#include "compiler/ast/expr_index.hpp"
#include "compiler/ast/expr_struct_lit.hpp"
#include "compiler/ast/expr_match.hpp"
#include "compiler/ast/expr_list_lit.hpp"
#include "compiler/ast/expr_dict_lit.hpp"
#include "compiler/ast/expr_if.hpp"
#include "compiler/ast/expr_range.hpp"
#include "compiler/ast/expr_closure.hpp"
#include "compiler/ast/expr_cast.hpp"
#include "compiler/ast/expr_unwrap.hpp"
#include "compiler/ast/expr_new.hpp"
#include "compiler/ast/expr_coalesce.hpp"
#include "compiler/ast/expr_try.hpp"
#include <cstddef>
#include <variant>

namespace qpc {


struct Expr {
    std::size_t offset = 0;
    std::variant<LitInt, LitFloat, LitBool, LitChar, LitString, LitNull, ExprIdent, ExprPath, ExprBinary,
                 ExprUnary, ExprCall, ExprAssign, ExprField, ExprIndex, ExprStructLit, ExprMatch,
                 ExprListLit, ExprDictLit, ExprIf, ExprRange, ExprClosure, ExprCast, ExprUnwrap, ExprNew,
                 ExprCoalesce, ExprTry>
        kind;
};

}  // namespace qpc
