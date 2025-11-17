#include "compiler/lsp.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool has_label(const std::vector<qpc::lsp::CompletionItem>& items, std::string_view label) {
    return std::ranges::any_of(items, [&](const qpc::lsp::CompletionItem& item) { return item.label == label; });
}

}  // namespace

TEST(Lsp, CompletionsIncludeKeywordsAndFn) {
    const std::string src = "pub fn add(a: i32, b: i32) -> i32 { a }";
    auto doc = qpc::lsp::LspDocument::from_text("test.qp", src);
    const auto items = doc.completions(src.size());
    EXPECT_TRUE(has_label(items, "fn"));
    EXPECT_TRUE(has_label(items, "let"));
    EXPECT_TRUE(has_label(items, "add"));
}

TEST(Lsp, PrefixAdDoesNotOfferLet) {
    const std::string src = "pub fn add(a: i32, b: i32) -> i32 { ad }";
    auto doc = qpc::lsp::LspDocument::from_text("test.qp", src);
    const auto off = src.find(" ad") + 3;
    const auto items = doc.completions(off);
    EXPECT_TRUE(has_label(items, "add"));
    EXPECT_FALSE(has_label(items, "let"));
}

TEST(Lsp, HoverParamInBodyIsI32) {
    const std::string src = "pub fn add(a: i32, b: i32) -> i32 { a }";
    auto doc = qpc::lsp::LspDocument::from_text("test.qp", src);
    const auto off = src.rfind('a');
    auto hover = doc.hover(off);
    ASSERT_TRUE(hover.has_value());
    EXPECT_NE(hover->contents.find("i32"), std::string::npos);
}

TEST(Lsp, ParseErrorHasLineColumn) {
    auto doc = qpc::lsp::LspDocument::from_text("test.qp", "fn (");
    ASSERT_FALSE(doc.diagnostics().empty());
    EXPECT_GE(doc.diagnostics().front().line, 1u);
    EXPECT_GE(doc.diagnostics().front().column, 1u);
}

TEST(Lsp, InitializeFramingCapabilities) {
    const std::string json = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string framed = qpc::lsp::write_framed(json);
    std::string buf = framed;
    auto body = qpc::lsp::try_read_framed(buf);
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(*body, json);
    EXPECT_TRUE(buf.empty());

    qpc::lsp::LspSession session;
    const auto out = session.handle_message(*body);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_NE(out.front().find("\"capabilities\""), std::string::npos);
    EXPECT_NE(out.front().find("completionProvider"), std::string::npos);
    EXPECT_NE(out.front().find("hoverProvider"), std::string::npos);
    EXPECT_NE(out.front().find("textDocumentSync"), std::string::npos);
}
