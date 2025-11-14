#pragma once

#include "compiler/ast.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/hir.hpp"
#include "compiler/source.hpp"
#include "compiler/token.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace qpc::lsp {

struct LspPos {
    std::uint32_t line = 0;
    std::uint32_t character = 0;
};

enum class CompletionKind {
    Keyword,
    Type,
    Function,
    Variable,
    Constant,
};

struct CompletionItem {
    std::string label;
    CompletionKind kind = CompletionKind::Keyword;
    std::string detail;
};

struct Hover {
    std::string contents;
};

class LspDocument {
public:
    static LspDocument from_text(std::string path, std::string text);

    [[nodiscard]] LspPos pos_from_offset(std::size_t offset) const;
    [[nodiscard]] std::size_t offset_from_pos(LspPos pos) const;
    [[nodiscard]] std::string ident_prefix(std::size_t offset) const;
    [[nodiscard]] std::vector<CompletionItem> completions(std::size_t offset) const;
    [[nodiscard]] std::optional<Hover> hover(std::size_t offset) const;
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const { return diags_.all(); }
    [[nodiscard]] const Source& source() const { return source_; }

private:
    struct Symbol {
        std::string name;
        CompletionKind kind = CompletionKind::Variable;
        std::string detail;
        std::size_t offset = 0;
        std::size_t fn_offset = 0;
        bool local = false;
    };

    void analyze();
    void collect_file(const AstFile& file);
    void collect_fn(const FnDecl& fn);
    void collect_block(const Block& block, std::size_t fn_offset);
    void collect_stmt(const Stmt& stmt, std::size_t fn_offset);
    void collect_expr(const Expr* expr, std::size_t fn_offset);
    void add_symbol(std::string name, CompletionKind kind, std::string detail, std::size_t offset,
                    std::size_t fn_offset, bool local);

    [[nodiscard]] const Token* token_at(std::size_t offset) const;
    [[nodiscard]] std::size_t enclosing_fn_offset(std::size_t offset) const;
    [[nodiscard]] std::optional<Hover> hover_from_hir(std::size_t ident_offset, std::string_view name) const;
    [[nodiscard]] std::optional<Hover> hover_from_symbols(std::size_t ident_offset,
                                                          std::string_view name) const;

    Source source_;
    std::deque<Source> extras_;
    DiagnosticEngine diags_;
    std::vector<Token> tokens_;
    std::vector<Symbol> symbols_;
    HirModule hir_;
    bool lowered_ = false;
    bool typed_ = false;
};

}  // namespace qpc::lsp
