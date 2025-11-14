#include "compiler/lsp/protocol.hpp"

#include "compiler/lsp/json.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <utility>

namespace qpc::lsp {
namespace {

std::string trim_left(std::string s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    s.erase(0, i);
    return s;
}

std::string percent_decode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            const int hi = hex(s[i + 1]);
            const int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string uri_to_path(std::string uri) {
    constexpr std::string_view prefix = "file://";
    if (uri.starts_with(prefix)) {
        uri.erase(0, prefix.size());
    }
    uri = percent_decode(uri);
    if (uri.size() >= 3 && uri[0] == '/' && std::isalpha(static_cast<unsigned char>(uri[1])) &&
        uri[2] == ':') {
        uri.erase(0, 1);
    }
    return uri;
}

Json copy_id(const Json& msg) {
    if (const Json* id = msg.get("id")) {
        return *id;
    }
    return Json::null();
}

Json make_response(const Json& id, Json result) {
    Json out = Json::object();
    out.set("jsonrpc", Json::string("2.0"));
    out.set("id", id);
    out.set("result", std::move(result));
    return out;
}

Json make_error(const Json& id, int code, std::string message) {
    Json err = Json::object();
    err.set("code", Json::integer(code));
    err.set("message", Json::string(std::move(message)));
    Json out = Json::object();
    out.set("jsonrpc", Json::string("2.0"));
    out.set("id", id);
    out.set("error", std::move(err));
    return out;
}

int completion_kind(CompletionKind kind) {
    switch (kind) {
        case CompletionKind::Keyword:
            return 14;
        case CompletionKind::Type:
            return 22;
        case CompletionKind::Function:
            return 3;
        case CompletionKind::Variable:
            return 6;
        case CompletionKind::Constant:
            return 21;
    }
    return 1;
}

Json initialize_result() {
    Json caps = Json::object();
    caps.set("textDocumentSync", Json::integer(1));
    Json completion = Json::object();
    completion.set("triggerCharacters", Json::array());
    caps.set("completionProvider", std::move(completion));
    caps.set("hoverProvider", Json::boolean(true));

    Json info = Json::object();
    info.set("name", Json::string("qpc"));
    info.set("version", Json::string("0.3.0"));

    Json result = Json::object();
    result.set("capabilities", std::move(caps));
    result.set("serverInfo", std::move(info));
    return result;
}

LspPos position_from(const Json* pos) {
    LspPos out;
    if (pos == nullptr) {
        return out;
    }
    out.line = static_cast<std::uint32_t>(pos->get_int("line"));
    out.character = static_cast<std::uint32_t>(pos->get_int("character"));
    return out;
}

}  // namespace

std::string write_framed(std::string_view json) {
    return "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + std::string(json);
}

std::optional<std::string> try_read_framed(std::string& buffer) {
    std::size_t header_end = std::string::npos;
    std::size_t sep_len = 0;
    const auto crlf = buffer.find("\r\n\r\n");
    const auto lf = buffer.find("\n\n");
    if (crlf != std::string::npos && (lf == std::string::npos || crlf <= lf)) {
        header_end = crlf;
        sep_len = 4;
    } else if (lf != std::string::npos) {
        header_end = lf;
        sep_len = 2;
    } else {
        return std::nullopt;
    }

    const std::string headers = buffer.substr(0, header_end);
    std::size_t length = 0;
    bool got = false;
    std::size_t line_start = 0;
    while (line_start <= headers.size()) {
        std::size_t line_end = headers.find('\n', line_start);
        if (line_end == std::string::npos) {
            line_end = headers.size();
        }
        std::string line = headers.substr(line_start, line_end - line_start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        constexpr std::string_view prefix = "Content-Length:";
        if (line.starts_with(prefix)) {
            try {
                length = std::stoul(trim_left(line.substr(prefix.size())));
                got = true;
            } catch (...) {
                return std::nullopt;
            }
        }
        if (line_end == headers.size()) {
            break;
        }
        line_start = line_end + 1;
    }
    if (!got) {
        return std::nullopt;
    }

    const std::size_t body_start = header_end + sep_len;
    if (buffer.size() < body_start + length) {
        return std::nullopt;
    }
    std::string body = buffer.substr(body_start, length);
    buffer.erase(0, body_start + length);
    return body;
}

std::string LspSession::publish_diagnostics(const std::string& uri, const LspDocument& doc) const {
    Json list = Json::array();
    for (const auto& d : doc.diagnostics()) {
        const auto start_pos = LspPos{
            .line = d.line == 0 ? 0 : d.line - 1,
            .character = d.column == 0 ? 0 : d.column - 1,
        };
        Json start = Json::object();
        start.set("line", Json::integer(start_pos.line));
        start.set("character", Json::integer(start_pos.character));
        Json end = Json::object();
        end.set("line", Json::integer(start_pos.line));
        end.set("character", Json::integer(start_pos.character + 1));
        Json range = Json::object();
        range.set("start", std::move(start));
        range.set("end", std::move(end));

        Json item = Json::object();
        item.set("range", std::move(range));
        item.set("severity", Json::integer(d.level == DiagLevel::Error ? 1 : 2));
        item.set("source", Json::string("qpc"));
        item.set("message", Json::string(d.message));
        list.push(std::move(item));
    }

    Json params = Json::object();
    params.set("uri", Json::string(uri));
    params.set("diagnostics", std::move(list));
    Json msg = Json::object();
    msg.set("jsonrpc", Json::string("2.0"));
    msg.set("method", Json::string("textDocument/publishDiagnostics"));
    msg.set("params", std::move(params));
    return msg.dump();
}

std::vector<std::string> LspSession::handle_message(std::string_view json_body) {
    auto parsed = parse_json(json_body);
    if (!parsed) {
        return {make_error(Json::null(), -32700, "Parse error").dump()};
    }
    return dispatch(*parsed);
}

std::vector<std::string> LspSession::dispatch(const Json& msg) {
    const std::string method = msg.get_string("method");
    const Json id = copy_id(msg);
    const Json* params = msg.get("params");

    if (method == "initialize") {
        return {make_response(id, initialize_result()).dump()};
    }
    if (method == "initialized" || method == "textDocument/didSave") {
        return {};
    }
    if (method == "shutdown") {
        shutdown_ = true;
        return {make_response(id, Json::null()).dump()};
    }
    if (method == "exit") {
        exit_ = true;
        return {};
    }

    if (method == "textDocument/didOpen") {
        const Json* td = params != nullptr ? params->get("textDocument") : nullptr;
        if (td == nullptr) {
            return {};
        }
        const std::string uri = td->get_string("uri");
        const std::string text = td->get_string("text");
        auto doc = LspDocument::from_text(uri_to_path(uri), text);
        const std::string note = publish_diagnostics(uri, doc);
        docs_.insert_or_assign(uri, std::move(doc));
        return {note};
    }
    if (method == "textDocument/didChange") {
        const Json* td = params != nullptr ? params->get("textDocument") : nullptr;
        if (td == nullptr) {
            return {};
        }
        const std::string uri = td->get_string("uri");
        std::string text;
        if (const Json* changes = params->get("contentChanges")) {
            if (const auto* arr = changes->as_array(); arr != nullptr && !arr->empty()) {
                if (const Json* t = arr->front().get("text")) {
                    text = t->as_string().value_or("");
                }
            }
        }
        auto doc = LspDocument::from_text(uri_to_path(uri), std::move(text));
        const std::string note = publish_diagnostics(uri, doc);
        docs_.insert_or_assign(uri, std::move(doc));
        return {note};
    }
    if (method == "textDocument/didClose") {
        const Json* td = params != nullptr ? params->get("textDocument") : nullptr;
        if (td != nullptr) {
            docs_.erase(td->get_string("uri"));
        }
        return {};
    }

    if (method == "textDocument/completion") {
        const Json* td = params != nullptr ? params->get("textDocument") : nullptr;
        const std::string uri = td != nullptr ? td->get_string("uri") : "";
        auto it = docs_.find(uri);
        Json items = Json::array();
        if (it != docs_.end()) {
            const LspPos pos = position_from(params != nullptr ? params->get("position") : nullptr);
            const std::size_t offset = it->second.offset_from_pos(pos);
            for (const auto& c : it->second.completions(offset)) {
                Json item = Json::object();
                item.set("label", Json::string(c.label));
                item.set("kind", Json::integer(completion_kind(c.kind)));
                if (!c.detail.empty()) {
                    item.set("detail", Json::string(c.detail));
                }
                items.push(std::move(item));
            }
        }
        Json result = Json::object();
        result.set("isIncomplete", Json::boolean(false));
        result.set("items", std::move(items));
        return {make_response(id, std::move(result)).dump()};
    }

    if (method == "textDocument/hover") {
        const Json* td = params != nullptr ? params->get("textDocument") : nullptr;
        const std::string uri = td != nullptr ? td->get_string("uri") : "";
        auto it = docs_.find(uri);
        if (it == docs_.end()) {
            return {make_response(id, Json::null()).dump()};
        }
        const LspPos pos = position_from(params != nullptr ? params->get("position") : nullptr);
        const std::size_t offset = it->second.offset_from_pos(pos);
        auto hover = it->second.hover(offset);
        if (!hover) {
            return {make_response(id, Json::null()).dump()};
        }
        Json contents = Json::object();
        contents.set("kind", Json::string("plaintext"));
        contents.set("value", Json::string(hover->contents));
        Json result = Json::object();
        result.set("contents", std::move(contents));
        return {make_response(id, std::move(result)).dump()};
    }

    if (msg.get("id") != nullptr) {
        return {make_error(id, -32601, "Method not found: " + method).dump()};
    }
    return {};
}

int run_lsp(std::istream& in, std::ostream& out) {
    LspSession session;
    while (!session.exit_requested() && in) {
        std::size_t length = 0;
        bool got_length = false;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                break;
            }
            constexpr std::string_view prefix = "Content-Length:";
            if (line.starts_with(prefix)) {
                try {
                    length = std::stoul(trim_left(line.substr(prefix.size())));
                    got_length = true;
                } catch (...) {
                    got_length = false;
                }
            }
        }
        if (!in) {
            break;
        }
        if (!got_length) {
            continue;
        }
        std::string body(length, '\0');
        if (!in.read(body.data(), static_cast<std::streamsize>(length))) {
            break;
        }
        for (const auto& msg : session.handle_message(body)) {
            const std::string framed = write_framed(msg);
            out.write(framed.data(), static_cast<std::streamsize>(framed.size()));
            out.flush();
        }
    }
    return 0;
}

}  // namespace qpc::lsp
