#include "control.h"

#include <cstdint>

namespace qplus {

template <typename A, typename B>
std::int32_t query2(std::int32_t n) {
    return n + static_cast<std::int32_t>(sizeof(A) + sizeof(B) > 0);
}

}  // namespace qplus

int main() {
    const auto c = qplus::clamp(12, 0, 10);
    const auto s = qplus::sum_n(4);
    const auto r = qplus::sum_range();
    qplus::List<std::int32_t> xs{1, 2, 3};
    const auto l = qplus::sum_list(xs);
    const auto m = qplus::min2(3, 1);
    const auto same = qplus::same(9);
    const auto b1 = qplus::bump();
    const auto b2 = qplus::bump();
    const auto w = qplus::world_count();
    const auto q = qplus::query2<qplus::Transform, qplus::Sprite>(3);
    const auto nf = qplus::not_flag(false);
    const bool ok = c == 10 && s == 6 && r == 10 && l == 6 && m == 1 && same == 9 && b1 == 1 &&
                    b2 == 2 && w == 2 && q == 4 && nf;
    return ok ? 0 : 1;
}
