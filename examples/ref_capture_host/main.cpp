#include "ref_capture.h"

int main() {
    const bool ok = qplus::bump() == 2 && qplus::add_into(3) == 4;
    return ok ? 0 : 1;
}
