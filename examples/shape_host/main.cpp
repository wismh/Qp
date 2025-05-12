#include "shape.h"

int main() {
    qplus::Shape none{qplus::Shape::None{}};
    qplus::Shape rect{qplus::Shape::Rect{.w = 3.0f, .h = 4.0f}};
    const auto empty = qplus::area(none);
    const auto filled = qplus::area(rect);
    const auto hi = qplus::greet("world");
    qplus::Point a{.x = 1, .y = 2};
    qplus::Point b{.x = 3, .y = 4};
    const auto x = qplus::add_x(a, b);
    const bool ok = empty == 0.0f && filled == 12.0f && hi == "hello, world" && x == 4;
    return ok ? 0 : 1;
}
