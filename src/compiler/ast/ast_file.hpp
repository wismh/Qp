#pragma once

#include "compiler/ast/enum_decl.hpp"
#include "compiler/ast/fn_decl.hpp"
#include "compiler/ast/impl_decl.hpp"
#include "compiler/ast/mod_decl.hpp"
#include "compiler/ast/static_decl.hpp"
#include "compiler/ast/struct_decl.hpp"
#include "compiler/ast/trait_decl.hpp"
#include "compiler/ast/use_decl.hpp"
#include "compiler/ast/variant_type_decl.hpp"
#include <vector>

namespace qpc {


struct AstFile {
    std::vector<UseDecl> uses;
    std::vector<ModDecl> mods;
    std::vector<StaticDecl> statics;
    std::vector<TraitDecl> traits;
    std::vector<StructDecl> structs;
    std::vector<EnumDecl> enums;
    std::vector<VariantTypeDecl> variants;
    std::vector<ImplDecl> impls;
    std::vector<FnDecl> functions;
};

}  // namespace qpc
