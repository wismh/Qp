#pragma once

#include "compiler/hir/hir_lit_int.hpp"
#include "compiler/hir/hir_lit_float.hpp"
#include "compiler/hir/hir_lit_bool.hpp"
#include "compiler/hir/hir_lit_char.hpp"
#include "compiler/hir/hir_lit_string.hpp"
#include "compiler/hir/hir_lit_null.hpp"
#include "compiler/hir/hir_var.hpp"
#include "compiler/hir/hir_binary.hpp"
#include "compiler/hir/hir_unary.hpp"
#include "compiler/hir/hir_call.hpp"
#include "compiler/hir/hir_assign.hpp"
#include "compiler/hir/hir_field_access.hpp"
#include "compiler/hir/hir_index.hpp"
#include "compiler/hir/hir_struct_lit.hpp"
#include "compiler/hir/hir_enum_lit.hpp"
#include "compiler/hir/hir_method_call.hpp"
#include "compiler/hir/hir_field_assign.hpp"
#include "compiler/hir/hir_index_assign.hpp"
#include "compiler/hir/hir_match.hpp"
#include "compiler/hir/hir_list_lit.hpp"
#include "compiler/hir/hir_dict_lit.hpp"
#include "compiler/hir/hir_if.hpp"
#include "compiler/hir/hir_range.hpp"
#include "compiler/hir/hir_closure.hpp"
#include "compiler/hir/hir_cast.hpp"
#include "compiler/hir/hir_unwrap.hpp"
#include "compiler/hir/hir_new.hpp"
#include "compiler/hir/hir_coalesce.hpp"
#include "compiler/hir/hir_try.hpp"
#include "compiler/type.hpp"
#include <cstddef>
#include <string>
#include <variant>

namespace qpc {


struct HirExpr {
    Type ty;
    std::string coerce_dyn;
    bool coerce_nullable = false;
    std::size_t offset = 0;
    std::variant<HirLitInt, HirLitFloat, HirLitBool, HirLitChar, HirLitString, HirLitNull, HirVar, HirBinary,
                 HirUnary, HirCall, HirAssign, HirFieldAccess, HirIndex, HirStructLit, HirEnumLit,
                 HirMethodCall, HirFieldAssign, HirIndexAssign, HirMatch, HirListLit, HirDictLit,
                 HirIf, HirRange, HirClosure, HirCast, HirUnwrap, HirNew, HirCoalesce, HirTry>
        kind;
};

}  // namespace qpc
