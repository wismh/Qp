#pragma once

#include "compiler/source.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace qpc {

enum class DiagLevel { Error, Warning };

struct Diagnostic {
    DiagLevel level = DiagLevel::Error;
    std::string path;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::string message;
};

class DiagnosticEngine {
public:
    void error(const Source& src, std::size_t offset, std::string message);
    void error(std::string path, std::uint32_t line, std::uint32_t column, std::string message);

    [[nodiscard]] bool has_errors() const;
    [[nodiscard]] const std::vector<Diagnostic>& all() const { return diags_; }
    void print(std::ostream& out) const;

private:
    std::vector<Diagnostic> diags_;
};

}  // namespace qpc
