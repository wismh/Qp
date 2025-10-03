#pragma once

#include "compiler/hir/hir_pat_wild.hpp"
#include "compiler/hir/hir_pat_binding.hpp"
#include "compiler/hir/hir_pat_variant.hpp"
#include <cstddef>
#include <variant>

namespace qpc {

struct HirPat {
    std::size_t offset = 0;
    std::variant<HirPatWild, HirPatBinding, HirPatVariant> kind;
};

}  // namespace qpc
