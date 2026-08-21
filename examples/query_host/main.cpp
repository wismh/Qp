#include "query.h"

int main() {
    const bool ok = qplus::sum_query() == 33;
    return ok ? 0 : 1;
}
