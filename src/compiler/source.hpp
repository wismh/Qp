#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace qpc {

struct Loc {
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

class Source {
public:
    Source() = default;
    static Source from_file(std::string path);
    static Source from_string(std::string path, std::string text);

    [[nodiscard]] const std::string& path() const { return path_; }
    [[nodiscard]] const std::string& text() const { return text_; }
    [[nodiscard]] std::string_view view() const { return text_; }
    [[nodiscard]] Loc location(std::size_t offset) const;
    [[nodiscard]] std::string_view slice(std::size_t offset, std::size_t length) const;

private:
    void index_lines();

    std::string path_;
    std::string text_;
    std::vector<std::size_t> line_offsets_;
};

}  // namespace qpc
