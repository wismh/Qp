#include "compiler/parser.hpp"

#include "compiler/parser/parser_detail.hpp"

namespace qpc {

AstFile parse(const Source& src, const std::vector<Token>& tokens, DiagnosticEngine& diags) {
    return detail::Parser{src, tokens, diags}.parse_file();
}

}  // namespace qpc
