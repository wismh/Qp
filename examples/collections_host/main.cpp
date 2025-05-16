#include "collections.h"

#include <cstdint>

int main() {
    const auto red = qplus::color_code(qplus::Color::Red);
    qplus::List<std::int32_t> xs{10, 20};
    qplus::Array<std::int32_t, 2> buf{3, 4};
    qplus::Dict<qplus::String, std::int32_t> stats{{"hp", 7}};
    const auto s = qplus::sum2(xs);
    const auto b = qplus::second(buf);
    const auto hp = qplus::get_hp(stats);
    const auto made = qplus::sample_list();
    const auto map = qplus::sample_map();
    const bool ok = red == 1 && s == 30 && b == 4 && hp == 7 && made[1] == 20 && map.at("hp") == 7;
    return ok ? 0 : 1;
}
