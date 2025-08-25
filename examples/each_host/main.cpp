#include "each.h"

int main() {
    const bool ok = qplus::sum_each() == 5 && qplus::zip_mul() == 20;
    return ok ? 0 : 1;
}
