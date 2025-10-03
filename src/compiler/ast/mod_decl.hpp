#pragma once

#include "compiler/ast/fwd.hpp"
#include <cstddef>
#include <memory>
#include <string>

namespace qpc {

struct ModDecl {
    bool pub = false;
    bool file = false;
    std::string name;
    std::unique_ptr<AstFile> body;
    const Source* source = nullptr;
    std::size_t offset = 0;
};

}  // namespace qpc
