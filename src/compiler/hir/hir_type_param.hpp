#pragma once

#include <optional>
#include <string>
#include <vector>

namespace qpc {

struct HirTypeParam {
    std::string name;
    std::optional<std::string> bound;
    bool pack = false;
};

inline bool last_is_pack(const std::vector<HirTypeParam>& tps) {
    return !tps.empty() && tps.back().pack;
}

inline std::size_t pack_prefix(const std::vector<HirTypeParam>& tps) {
    return last_is_pack(tps) ? tps.size() - 1 : tps.size();
}

inline bool type_arg_count_ok(const std::vector<HirTypeParam>& tps, std::size_t n) {
    if (last_is_pack(tps)) {
        return n >= pack_prefix(tps);
    }
    return n == tps.size();
}

}  // namespace qpc
