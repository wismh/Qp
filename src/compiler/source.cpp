#include "compiler/source.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace qpc {

Source Source::from_file(std::string path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open '" + path + "'");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return from_string(std::move(path), ss.str());
}

Source Source::from_string(std::string path, std::string text) {
    Source src;
    src.path_ = std::move(path);
    src.text_ = std::move(text);
    src.index_lines();
    return src;
}

void Source::index_lines() {
    line_offsets_.clear();
    line_offsets_.push_back(0);
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') {
            line_offsets_.push_back(i + 1);
        }
    }
}

Loc Source::location(std::size_t offset) const {
    if (line_offsets_.empty()) {
        return {};
    }
    if (offset > text_.size()) {
        offset = text_.size();
    }
    auto it = std::upper_bound(line_offsets_.begin(), line_offsets_.end(), offset);
    if (it != line_offsets_.begin()) {
        --it;
    }
    const auto line_index = static_cast<std::uint32_t>(it - line_offsets_.begin());
    Loc loc;
    loc.line = line_index + 1;
    loc.column = static_cast<std::uint32_t>(offset - *it) + 1;
    return loc;
}

std::string_view Source::slice(std::size_t offset, std::size_t length) const {
    if (offset > text_.size()) {
        return {};
    }
    length = std::min(length, text_.size() - offset);
    return std::string_view(text_).substr(offset, length);
}

}  // namespace qpc
