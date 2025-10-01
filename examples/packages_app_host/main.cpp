#include "app.h"

int main() {
    const bool ok = qplus::run() == 45;
    return ok ? 0 : 1;
}
