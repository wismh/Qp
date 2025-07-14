#include "heap.h"

int main() {
    const auto before = qplus::gc_live();
    qplus::Point* p = qplus::origin();
    qplus::Point* q = qplus::make(3, 4);
    qplus::gc_collect();
    const bool ok = qplus::get_x(p) == 0 && qplus::get_x(q) == 3 && qplus::get_x(nullptr) == 0 &&
                    qplus::gc_live() > before;
    return ok ? 0 : 1;
}
