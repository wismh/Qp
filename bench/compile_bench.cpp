#include "compiler/compile.hpp"
#include "compiler/cpp_backend.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/lexer.hpp"
#include "compiler/lower.hpp"
#include "compiler/parser.hpp"
#include "compiler/source.hpp"
#include "compiler/typeck.hpp"

#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

const std::string kAddSrc = "pub fn add(a: i32, b: i32) -> i32 {\n    a + b\n}\n";

const std::string kRichSrc = R"(
pub enum Color { Red, Green, Blue }

pub struct Point {
    mut x: i32,
    mut y: i32,
}

impl Point {
    fn add(self, o: Point) -> Point {
        Point { x: self.x + o.x, y: self.y + o.y }
    }
}

pub fn sum(xs: [i32]) -> i32 {
    let mut s = 0;
    for x in xs {
        s = s + x;
    }
    s
}

pub fn label(n: i32) -> string {
    "n=" + to_string(n)
}

pub fn wrap(n: i32) -> i32? {
    n
}

pub fn area(p: Point?) -> i32 {
    if let v = p {
        v.x * v.y
    } else {
        0
    }
}
)";

std::string many_fns(int n) {
    std::ostringstream out;
    for (int i = 0; i < n; ++i) {
        out << "fn f" << i << "(a: i32) -> i32 { a + " << i << " }\n";
    }
    return out.str();
}

struct Frontend {
    qpc::Source source;
    std::vector<qpc::Token> tokens;
    qpc::AstFile ast;
    qpc::HirModule hir;
};

bool prepare_lexed(Frontend& fe, std::string path, std::string code) {
    fe.source = qpc::Source::from_string(std::move(path), std::move(code));
    qpc::DiagnosticEngine diags;
    fe.tokens = qpc::lex(fe.source, diags);
    return !diags.has_errors();
}

bool prepare_lowered(Frontend& fe) {
    qpc::DiagnosticEngine diags;
    // parse again: lower consumes AstFile
    fe.ast = qpc::parse(fe.source, fe.tokens, diags);
    if (diags.has_errors()) {
        return false;
    }
    fe.hir = qpc::lower(fe.source, std::move(fe.ast), diags);
    return !diags.has_errors();
}

bool prepare_typed(Frontend& fe) {
    if (!prepare_lowered(fe)) {
        return false;
    }
    qpc::DiagnosticEngine diags;
    qpc::typeck(fe.source, fe.hir, diags);
    return !diags.has_errors();
}

void BM_Lex(benchmark::State& state) {
    const std::string code = state.range(0) == 0 ? kAddSrc : many_fns(static_cast<int>(state.range(0)));
    auto src = qpc::Source::from_string("bench.qp", code);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto tokens = qpc::lex(src, diags);
        benchmark::DoNotOptimize(tokens.size());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(code.size()));
}

void BM_Parse(benchmark::State& state) {
    const std::string code = state.range(0) == 0 ? kAddSrc : many_fns(static_cast<int>(state.range(0)));
    Frontend fe;
    if (!prepare_lexed(fe, "bench.qp", code)) {
        state.SkipWithError("lex failed");
        return;
    }
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto ast = qpc::parse(fe.source, fe.tokens, diags);
        benchmark::DoNotOptimize(ast.functions.size());
        benchmark::ClobberMemory();
    }
}

void BM_Lower(benchmark::State& state) {
    const std::string code = state.range(0) == 0 ? kAddSrc : many_fns(static_cast<int>(state.range(0)));
    Frontend fe;
    if (!prepare_lexed(fe, "bench.qp", code)) {
        state.SkipWithError("lex failed");
        return;
    }
    for (auto _ : state) {
        state.PauseTiming();
        qpc::DiagnosticEngine prep;
        auto ast = qpc::parse(fe.source, fe.tokens, prep);
        if (prep.has_errors()) {
            state.SkipWithError("parse failed");
            return;
        }
        state.ResumeTiming();

        qpc::DiagnosticEngine diags;
        auto hir = qpc::lower(fe.source, std::move(ast), diags);
        benchmark::DoNotOptimize(hir.functions.size());
        benchmark::ClobberMemory();
    }
}

void BM_Typeck(benchmark::State& state) {
    const std::string code = state.range(0) == 0 ? kAddSrc : many_fns(static_cast<int>(state.range(0)));
    Frontend fe;
    if (!prepare_lexed(fe, "bench.qp", code)) {
        state.SkipWithError("lex failed");
        return;
    }
    for (auto _ : state) {
        state.PauseTiming();
        qpc::DiagnosticEngine prep;
        auto ast = qpc::parse(fe.source, fe.tokens, prep);
        if (prep.has_errors()) {
            state.SkipWithError("parse failed");
            return;
        }
        auto hir = qpc::lower(fe.source, std::move(ast), prep);
        if (prep.has_errors()) {
            state.SkipWithError("lower failed");
            return;
        }
        state.ResumeTiming();

        qpc::DiagnosticEngine diags;
        qpc::typeck(fe.source, hir, diags);
        benchmark::DoNotOptimize(diags.has_errors());
        benchmark::DoNotOptimize(hir.functions.size());
        benchmark::ClobberMemory();
    }
}

void BM_Codegen(benchmark::State& state) {
    const std::string code = state.range(0) == 0 ? kAddSrc : many_fns(static_cast<int>(state.range(0)));
    Frontend fe;
    if (!prepare_lexed(fe, "bench.qp", code) || !prepare_typed(fe)) {
        state.SkipWithError("frontend failed");
        return;
    }
    // typed hir is reused; emit_cpp is const on module
    for (auto _ : state) {
        auto out = qpc::emit_cpp(fe.source, fe.hir);
        benchmark::DoNotOptimize(out.header.size());
        benchmark::DoNotOptimize(out.source.size());
        benchmark::ClobberMemory();
    }
}

void BM_Compile(benchmark::State& state) {
    const std::string code = state.range(0) == 0 ? kAddSrc : many_fns(static_cast<int>(state.range(0)));
    auto src = qpc::Source::from_string("bench.qp", code);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto result = qpc::compile_to_memory(src, diags);
        benchmark::DoNotOptimize(result.ok);
        benchmark::DoNotOptimize(result.output.source.size());
        benchmark::ClobberMemory();
    }
}

void BM_Rich_Lex(benchmark::State& state) {
    auto src = qpc::Source::from_string("rich.qp", kRichSrc);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto tokens = qpc::lex(src, diags);
        benchmark::DoNotOptimize(tokens.size());
        benchmark::ClobberMemory();
    }
}

void BM_Rich_Parse(benchmark::State& state) {
    Frontend fe;
    if (!prepare_lexed(fe, "rich.qp", kRichSrc)) {
        state.SkipWithError("lex failed");
        return;
    }
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto ast = qpc::parse(fe.source, fe.tokens, diags);
        benchmark::DoNotOptimize(ast.functions.size() + ast.structs.size());
        benchmark::ClobberMemory();
    }
}

void BM_Rich_Lower(benchmark::State& state) {
    Frontend fe;
    if (!prepare_lexed(fe, "rich.qp", kRichSrc)) {
        state.SkipWithError("lex failed");
        return;
    }
    for (auto _ : state) {
        state.PauseTiming();
        qpc::DiagnosticEngine prep;
        auto ast = qpc::parse(fe.source, fe.tokens, prep);
        state.ResumeTiming();
        qpc::DiagnosticEngine diags;
        auto hir = qpc::lower(fe.source, std::move(ast), diags);
        benchmark::DoNotOptimize(hir.functions.size());
        benchmark::ClobberMemory();
    }
}

void BM_Rich_Typeck(benchmark::State& state) {
    Frontend fe;
    if (!prepare_lexed(fe, "rich.qp", kRichSrc)) {
        state.SkipWithError("lex failed");
        return;
    }
    for (auto _ : state) {
        state.PauseTiming();
        qpc::DiagnosticEngine prep;
        auto ast = qpc::parse(fe.source, fe.tokens, prep);
        auto hir = qpc::lower(fe.source, std::move(ast), prep);
        state.ResumeTiming();
        qpc::DiagnosticEngine diags;
        qpc::typeck(fe.source, hir, diags);
        benchmark::DoNotOptimize(diags.has_errors());
        benchmark::ClobberMemory();
    }
}

void BM_Rich_Codegen(benchmark::State& state) {
    Frontend fe;
    if (!prepare_lexed(fe, "rich.qp", kRichSrc) || !prepare_typed(fe)) {
        state.SkipWithError("frontend failed");
        return;
    }
    for (auto _ : state) {
        auto out = qpc::emit_cpp(fe.source, fe.hir);
        benchmark::DoNotOptimize(out.source.size());
        benchmark::ClobberMemory();
    }
}

void BM_Rich_Compile(benchmark::State& state) {
    auto src = qpc::Source::from_string("rich.qp", kRichSrc);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto result = qpc::compile_to_memory(src, diags);
        benchmark::DoNotOptimize(result.ok);
        benchmark::ClobberMemory();
    }
}

void BM_CompileExampleFile(benchmark::State& state) {
    const fs::path input = fs::path(QPLUS_SOURCE_DIR) / "examples" / "control.qp";
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto src = qpc::Source::from_file(input.string());
        auto result = qpc::compile_to_memory(src, diags);
        benchmark::DoNotOptimize(result.ok);
        benchmark::DoNotOptimize(result.output.source.size());
        benchmark::ClobberMemory();
    }
}

}  // namespace

// range(0)==0 → small `add`; otherwise N free functions
BENCHMARK(BM_Lex)->Arg(0)->Arg(64)->Arg(256)->UseRealTime();
BENCHMARK(BM_Parse)->Arg(0)->Arg(64)->Arg(256)->UseRealTime();
BENCHMARK(BM_Lower)->Arg(0)->Arg(64)->Arg(256)->UseRealTime();
BENCHMARK(BM_Typeck)->Arg(0)->Arg(64)->Arg(256)->UseRealTime();
BENCHMARK(BM_Codegen)->Arg(0)->Arg(64)->Arg(256)->UseRealTime();
BENCHMARK(BM_Compile)->Arg(0)->Arg(64)->Arg(256)->UseRealTime();

BENCHMARK(BM_Rich_Lex)->UseRealTime();
BENCHMARK(BM_Rich_Parse)->UseRealTime();
BENCHMARK(BM_Rich_Lower)->UseRealTime();
BENCHMARK(BM_Rich_Typeck)->UseRealTime();
BENCHMARK(BM_Rich_Codegen)->UseRealTime();
BENCHMARK(BM_Rich_Compile)->UseRealTime();

BENCHMARK(BM_CompileExampleFile)->UseRealTime();

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::off);
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
