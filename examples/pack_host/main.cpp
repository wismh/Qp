#include "pack.h"

int main() {
    const bool ok = qplus::run() == 11;
    return ok ? 0 : 1;
}
