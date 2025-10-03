#pragma once

#include "compiler/hir/hir_field.hpp"
#include "compiler/hir/hir_type_param.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct HirStruct {
    bool pub = false;
    bool is_extern = false;
    bool opaque = false;
    std::string name;
    std::vector<HirTypeParam> type_params;
    std::vector<HirField> fields;
    std::size_t offset = 0;
};

}  // namespace qpc
