#pragma once

#include "compiler/ast/pat_wild.hpp"
#include "compiler/ast/pat_ident.hpp"
#include "compiler/ast/pat_variant.hpp"
#include <cstddef>
#include <variant>

namespace qpc {

struct Pat {
    std::size_t offset = 0;
    std::variant<PatWild, PatIdent, PatVariant> kind;
};

}  // namespace qpc
