#include "expand.h"

int main() {
    const bool ok = qplus::run() == 30;
    return ok ? 0 : 1;
}
