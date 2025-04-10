#pragma once

#include "compiler/compile.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/source.hpp"

#include <string>
#include <utility>
#include <vector>

namespace qpc::test {

struct Parsed {
    Source source;
    DiagnosticEngine diags;
    std::vector<Token> tokens;
    AstFile ast;
};

inline Parsed parse_string(std::string code) {
    Parsed p;
    p.source = Source::from_string("test.qp", std::move(code));
    p.tokens = lex(p.source, p.diags);
    if (!p.diags.has_errors()) {
        p.ast = parse(p.source, p.tokens, p.diags);
    }
    return p;
}

struct Compiled {
    Source source;
    DiagnosticEngine diags;
    CompileResult result;
};

inline Compiled compile_string(std::string code) {
    Compiled c;
    c.source = Source::from_string("test.qp", std::move(code));
    c.result = compile_to_memory(c.source, c.diags);
    return c;
}

}  // namespace qpc::test
