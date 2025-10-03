#pragma once

#include "compiler/hir/hir_fn.hpp"
#include "compiler/hir/hir_type_param.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace qpc {

struct HirImpl {
    std::optional<std::string> trait_name;
    std::string type_name;
    std::vector<HirTypeParam> type_params;
    std::vector<HirFn> methods;
    std::size_t offset = 0;
};

}  // namespace qpc
