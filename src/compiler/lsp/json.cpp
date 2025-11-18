#include "compiler/lsp/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <utility>

namespace qpc::lsp {
namespace {

void skip_ws(std::string_view text, std::size_t& i) {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
}

std::optional<Json> parse_value(std::string_view text, std::size_t& i);

bool parse_literal(std::string_view text, std::size_t& i, std::string_view lit) {
    if (text.substr(i).starts_with(lit)) {
        i += lit.size();
        return true;
    }
    return false;
}

std::optional<std::string> parse_string(std::string_view text, std::size_t& i) {
    if (i >= text.size() || text[i] != '"') {
        return std::nullopt;
    }
    ++i;
    std::string out;
    while (i < text.size()) {
        const char c = text[i++];
        if (c == '"') {
            return out;
        }
        if (c == '\\') {
            if (i >= text.size()) {
                return std::nullopt;
            }
            const char e = text[i++];
            switch (e) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(e);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    if (i + 4 > text.size()) {
                        return std::nullopt;
                    }
                    unsigned code = 0;
                    for (int n = 0; n < 4; ++n) {
                        const char h = text[i++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') {
                            code += static_cast<unsigned>(h - '0');
                        } else if (h >= 'a' && h <= 'f') {
                            code += static_cast<unsigned>(h - 'a' + 10);
                        } else if (h >= 'A' && h <= 'F') {
                            code += static_cast<unsigned>(h - 'A' + 10);
                        } else {
                            return std::nullopt;
                        }
                    }
                    if (code < 0x80) {
                        out.push_back(static_cast<char>(code));
                    } else if (code < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                    break;
                }
                default:
                    return std::nullopt;
            }
            continue;
        }
        out.push_back(c);
    }
    return std::nullopt;
}

std::optional<Json> parse_number(std::string_view text, std::size_t& i) {
    const std::size_t start = i;
    if (i < text.size() && text[i] == '-') {
        ++i;
    }
    if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
        return std::nullopt;
    }
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
            return std::nullopt;
        }
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
    }
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
            ++i;
        }
        if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
            return std::nullopt;
        }
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
    }
    try {
        return Json::number(std::stod(std::string(text.substr(start, i - start))));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<Json> parse_array(std::string_view text, std::size_t& i) {
    if (i >= text.size() || text[i] != '[') {
        return std::nullopt;
    }
    ++i;
    Json arr = Json::array();
    skip_ws(text, i);
    if (i < text.size() && text[i] == ']') {
        ++i;
        return arr;
    }
    while (i < text.size()) {
        auto v = parse_value(text, i);
        if (!v) {
            return std::nullopt;
        }
        arr.push(std::move(*v));
        skip_ws(text, i);
        if (i < text.size() && text[i] == ',') {
            ++i;
            skip_ws(text, i);
            continue;
        }
        if (i < text.size() && text[i] == ']') {
            ++i;
            return arr;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<Json> parse_object(std::string_view text, std::size_t& i) {
    if (i >= text.size() || text[i] != '{') {
        return std::nullopt;
    }
    ++i;
    Json obj = Json::object();
    skip_ws(text, i);
    if (i < text.size() && text[i] == '}') {
        ++i;
        return obj;
    }
    while (i < text.size()) {
        skip_ws(text, i);
        auto key = parse_string(text, i);
        if (!key) {
            return std::nullopt;
        }
        skip_ws(text, i);
        if (i >= text.size() || text[i] != ':') {
            return std::nullopt;
        }
        ++i;
        auto v = parse_value(text, i);
        if (!v) {
            return std::nullopt;
        }
        obj.set(std::move(*key), std::move(*v));
        skip_ws(text, i);
        if (i < text.size() && text[i] == ',') {
            ++i;
            continue;
        }
        if (i < text.size() && text[i] == '}') {
            ++i;
            return obj;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<Json> parse_value(std::string_view text, std::size_t& i) {
    skip_ws(text, i);
    if (i >= text.size()) {
        return std::nullopt;
    }
    const char c = text[i];
    if (c == 'n' && parse_literal(text, i, "null")) {
        return Json::null();
    }
    if (c == 't' && parse_literal(text, i, "true")) {
        return Json::boolean(true);
    }
    if (c == 'f' && parse_literal(text, i, "false")) {
        return Json::boolean(false);
    }
    if (c == '"') {
        auto s = parse_string(text, i);
        if (!s) {
            return std::nullopt;
        }
        return Json::string(std::move(*s));
    }
    if (c == '{') {
        return parse_object(text, i);
    }
    if (c == '[') {
        return parse_array(text, i);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        return parse_number(text, i);
    }
    return std::nullopt;
}

void append_escaped(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

}  // namespace

Json Json::null() { return {}; }

Json Json::boolean(bool v) {
    Json j;
    j.value_ = v;
    return j;
}

Json Json::number(double v) {
    Json j;
    j.value_ = v;
    return j;
}

Json Json::integer(std::int64_t v) {
    return number(static_cast<double>(v));
}

Json Json::string(std::string v) {
    Json j;
    j.value_ = std::move(v);
    return j;
}

Json Json::array(Array v) {
    Json j;
    j.value_ = std::move(v);
    return j;
}

Json Json::object(Object v) {
    Json j;
    j.value_ = std::move(v);
    return j;
}

bool Json::is_null() const { return std::holds_alternative<std::monostate>(value_); }

std::optional<bool> Json::as_bool() const {
    if (const auto* v = std::get_if<bool>(&value_)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<double> Json::as_number() const {
    if (const auto* v = std::get_if<double>(&value_)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<std::int64_t> Json::as_int() const {
    if (const auto* v = std::get_if<double>(&value_)) {
        return static_cast<std::int64_t>(*v);
    }
    return std::nullopt;
}

std::optional<std::string> Json::as_string() const {
    if (const auto* v = std::get_if<std::string>(&value_)) {
        return *v;
    }
    return std::nullopt;
}

const Json::Array* Json::as_array() const { return std::get_if<Array>(&value_); }

const Json::Object* Json::as_object() const { return std::get_if<Object>(&value_); }

const Json* Json::get(std::string_view key) const {
    const auto* obj = as_object();
    if (obj == nullptr) {
        return nullptr;
    }
    auto it = obj->find(std::string(key));
    if (it == obj->end()) {
        return nullptr;
    }
    return &it->second;
}

std::string Json::get_string(std::string_view key) const {
    const Json* v = get(key);
    if (v == nullptr) {
        return {};
    }
    return v->as_string().value_or(std::string{});
}

std::int64_t Json::get_int(std::string_view key, std::int64_t fallback) const {
    const Json* v = get(key);
    if (v == nullptr) {
        return fallback;
    }
    return v->as_int().value_or(fallback);
}

Json& Json::set(std::string key, Json value) {
    if (!std::holds_alternative<Object>(value_)) {
        value_ = Object{};
    }
    std::get<Object>(value_)[std::move(key)] = std::move(value);
    return *this;
}

void Json::push(Json value) {
    if (!std::holds_alternative<Array>(value_)) {
        value_ = Array{};
    }
    std::get<Array>(value_).push_back(std::move(value));
}

void Json::dump_into(std::string& out) const {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                out += "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                out += v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, double>) {
                if (std::isfinite(v) && std::floor(v) == v && v >= -9.0e15 && v <= 9.0e15) {
                    out += std::to_string(static_cast<std::int64_t>(v));
                } else {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%.17g", v);
                    out += buf;
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                append_escaped(out, v);
            } else if constexpr (std::is_same_v<T, Array>) {
                out.push_back('[');
                for (std::size_t i = 0; i < v.size(); ++i) {
                    if (i != 0) {
                        out.push_back(',');
                    }
                    v[i].dump_into(out);
                }
                out.push_back(']');
            } else if constexpr (std::is_same_v<T, Object>) {
                out.push_back('{');
                bool first = true;
                for (const auto& [k, val] : v) {
                    if (!first) {
                        out.push_back(',');
                    }
                    first = false;
                    append_escaped(out, k);
                    out.push_back(':');
                    val.dump_into(out);
                }
                out.push_back('}');
            }
        },
        value_);
}

std::string Json::dump() const {
    std::string out;
    dump_into(out);
    return out;
}

std::optional<Json> parse_json(std::string_view text) {
    std::size_t i = 0;
    auto v = parse_value(text, i);
    if (!v) {
        return std::nullopt;
    }
    skip_ws(text, i);
    if (i != text.size()) {
        return std::nullopt;
    }
    return v;
}

}  // namespace qpc::lsp
