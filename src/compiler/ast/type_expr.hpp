#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace qpc {


struct TypeExpr {
    enum class Kind { Named, Unit, List, Array, Dict, Tuple, Fn, Nullable, Dyn };

    Kind kind = Kind::Named;
    std::string name;
    std::size_t array_len = 0;
    std::vector<TypeExpr> args;
    std::size_t offset = 0;
    bool pack_expand = false;

    static TypeExpr named(std::string n) {
        TypeExpr t;
        t.kind = Kind::Named;
        t.name = std::move(n);
        return t;
    }

    static TypeExpr unit() {
        TypeExpr t;
        t.kind = Kind::Unit;
        t.name = "()";
        return t;
    }
};

}  // namespace qpc
