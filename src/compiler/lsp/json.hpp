#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace qpc::lsp {

class Json {
public:
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;

    Json() = default;
    static Json null();
    static Json boolean(bool v);
    static Json number(double v);
    static Json integer(std::int64_t v);
    static Json string(std::string v);
    static Json array(Array v = {});
    static Json object(Object v = {});

    [[nodiscard]] bool is_null() const;
    [[nodiscard]] std::optional<bool> as_bool() const;
    [[nodiscard]] std::optional<double> as_number() const;
    [[nodiscard]] std::optional<std::int64_t> as_int() const;
    [[nodiscard]] std::optional<std::string> as_string() const;
    [[nodiscard]] const Array* as_array() const;
    [[nodiscard]] const Object* as_object() const;
    [[nodiscard]] const Json* get(std::string_view key) const;
    [[nodiscard]] std::string get_string(std::string_view key) const;
    [[nodiscard]] std::int64_t get_int(std::string_view key, std::int64_t fallback = 0) const;

    Json& set(std::string key, Json value);
    void push(Json value);

    [[nodiscard]] std::string dump() const;

private:
    using Storage = std::variant<std::monostate, bool, double, std::string, Array, Object>;
    Storage value_;

    void dump_into(std::string& out) const;
};

[[nodiscard]] std::optional<Json> parse_json(std::string_view text);

}  // namespace qpc::lsp
