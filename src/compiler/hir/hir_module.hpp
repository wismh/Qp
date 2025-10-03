#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/hir/hir_c_enum.hpp"
#include "compiler/hir/hir_fn.hpp"
#include "compiler/hir/hir_impl.hpp"
#include "compiler/hir/hir_static.hpp"
#include "compiler/hir/hir_struct.hpp"
#include "compiler/hir/hir_trait.hpp"
#include "compiler/hir/hir_use.hpp"
#include "compiler/hir/hir_variant.hpp"
#include <string>
#include <vector>

namespace qpc {


struct HirModule {
    std::string name;
    const Source* source = nullptr;
    std::vector<HirUse> uses;
    std::vector<HirModule> mods;
    std::vector<HirStatic> statics;
    std::vector<HirTrait> traits;
    std::vector<HirStruct> structs;
    std::vector<HirCEnum> enums;
    std::vector<HirVariant> variants;
    std::vector<HirImpl> impls;
    std::vector<HirFn> functions;
};

}  // namespace qpc
