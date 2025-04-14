#include "compiler/compile.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/source.hpp"

#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

namespace {

const std::string kAddSrc = "pub fn add(a: i32, b: i32) -> i32 {\n    a + b\n}\n";

std::string many_fns(int n) {
    std::ostringstream out;
    for (int i = 0; i < n; ++i) {
        out << "fn f" << i << "(a: i32) -> i32 { a + " << i << " }\n";
    }
    return out.str();
}

void BM_LexAdd(benchmark::State& state) {
    auto src = qpc::Source::from_string("add.qp", kAddSrc);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto tokens = qpc::lex(src, diags);
        benchmark::DoNotOptimize(tokens);
        benchmark::ClobberMemory();
    }
}

void BM_ParseAdd(benchmark::State& state) {
    auto src = qpc::Source::from_string("add.qp", kAddSrc);
    qpc::DiagnosticEngine lex_diags;
    auto tokens = qpc::lex(src, lex_diags);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto ast = qpc::parse(src, tokens, diags);
        benchmark::DoNotOptimize(ast.functions.size());
        benchmark::ClobberMemory();
    }
}

void BM_CompileAdd(benchmark::State& state) {
    auto src = qpc::Source::from_string("add.qp", kAddSrc);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto result = qpc::compile_to_memory(src, diags);
        benchmark::DoNotOptimize(result.ok);
        benchmark::DoNotOptimize(result.output.source.size());
        benchmark::ClobberMemory();
    }
}

void BM_CompileManyFns(benchmark::State& state) {
    const std::string code = many_fns(static_cast<int>(state.range(0)));
    auto src = qpc::Source::from_string("many.qp", code);
    for (auto _ : state) {
        qpc::DiagnosticEngine diags;
        auto result = qpc::compile_to_memory(src, diags);
        benchmark::DoNotOptimize(result.ok);
        benchmark::ClobberMemory();
    }
}

}  // namespace

BENCHMARK(BM_LexAdd)->UseRealTime();
BENCHMARK(BM_ParseAdd)->UseRealTime();
BENCHMARK(BM_CompileAdd)->UseRealTime();
BENCHMARK(BM_CompileManyFns)->Arg(64)->Arg(256)->UseRealTime();

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
