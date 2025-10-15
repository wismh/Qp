#include "fn_value.h"

int main() {
    const bool ok = qplus::run() == 17;
    return ok ? 0 : 1;
}
