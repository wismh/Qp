#include "vec2.h"

int main() {
    qplus::Vec2 a{.x = 1.0f, .y = 2.0f};
    qplus::Vec2 b{.x = 3.0f, .y = 4.0f};
    auto c = a.add(b);
    c.scale(0.5f);
    const auto x = qplus::add_x(a, b);
    const bool ok = c.x == 2.0f && c.y == 3.0f && x == 4.0f && a.length_sq() == 5.0f;
    return ok ? 0 : 1;
}
