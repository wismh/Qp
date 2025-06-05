#include "extern_obj.h"

#include <cstdint>

namespace qplus {

Test test_object;

}  // namespace qplus

int main() {
    const auto sum = qplus::test();
    const auto created = qplus::use_created();
    const bool ok = sum == 11 && created == 1;
    return ok ? 0 : 1;
}
