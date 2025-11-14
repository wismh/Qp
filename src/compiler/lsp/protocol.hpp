#pragma once

#include "compiler/lsp/analyze.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace qpc::lsp {

[[nodiscard]] std::string write_framed(std::string_view json);
[[nodiscard]] std::optional<std::string> try_read_framed(std::string& buffer);

class LspSession {
public:
    std::vector<std::string> handle_message(std::string_view json_body);
    [[nodiscard]] bool exit_requested() const { return exit_; }

private:
    std::vector<std::string> dispatch(const class Json& msg);
    std::string publish_diagnostics(const std::string& uri, const LspDocument& doc) const;

    std::unordered_map<std::string, LspDocument> docs_;
    bool shutdown_ = false;
    bool exit_ = false;
};

int run_lsp(std::istream& in, std::ostream& out);

}  // namespace qpc::lsp
