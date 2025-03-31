#include "compiler/diagnostic.hpp"

#include <algorithm>
#include <ostream>
#include <ranges>

namespace qpc {

void DiagnosticEngine::error(const Source& src, std::size_t offset, std::string message) {
    const Loc loc = src.location(offset);
    error(src.path(), loc.line, loc.column, std::move(message));
}

void DiagnosticEngine::error(std::string path, std::uint32_t line, std::uint32_t column,
                             std::string message) {
    diags_.push_back(Diagnostic{
        .level = DiagLevel::Error,
        .path = std::move(path),
        .line = line,
        .column = column,
        .message = std::move(message),
    });
}

bool DiagnosticEngine::has_errors() const {
    return std::ranges::any_of(diags_, [](const Diagnostic& d) { return d.level == DiagLevel::Error; });
}

void DiagnosticEngine::print(std::ostream& out) const {
    for (const auto& d : diags_) {
        const char* kind = d.level == DiagLevel::Error ? "error" : "warning";
        out << d.path << ':' << d.line << ':' << d.column << ": " << kind << ": " << d.message << '\n';
    }
}

}  // namespace qpc
