#include "null_ops.h"

int main() {
    qplus::Point s{.x = 3, .y = 4};
    std::int32_t* px = qplus::get_x(&s);
    const bool ok = px != nullptr && *px == 3 && qplus::get_x(nullptr) == nullptr &&
                    qplus::or_x(&s, 9) == 3 && qplus::or_x(nullptr, 9) == 9;
    return ok ? 0 : 1;
}
