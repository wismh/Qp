#include "compiler/typeck.hpp"

#include "compiler/typeck/type_checker.hpp"

namespace qpc {

void typeck(const Source& src, HirModule& mod, DiagnosticEngine& diags) {
    detail::TypeChecker{src, mod, diags}.run();
}

}  // namespace qpc
