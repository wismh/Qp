#include "compiler/lexer.hpp"

#include <string_view>

namespace qpc {
namespace {

[[nodiscard]] constexpr bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

[[nodiscard]] constexpr bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

[[nodiscard]] constexpr bool is_ident_continue(char c) {
    return is_ident_start(c) || is_digit(c);
}

[[nodiscard]] TokenKind keyword_kind(std::string_view text) {
    if (text == "fn") {
        return TokenKind::KwFn;
    }
    if (text == "let") {
        return TokenKind::KwLet;
    }
    if (text == "mut") {
        return TokenKind::KwMut;
    }
    if (text == "return") {
        return TokenKind::KwReturn;
    }
    if (text == "pub") {
        return TokenKind::KwPub;
    }
    if (text == "struct") {
        return TokenKind::KwStruct;
    }
    if (text == "impl") {
        return TokenKind::KwImpl;
    }
    if (text == "self") {
        return TokenKind::KwSelf;
    }
    if (text == "enum") {
        return TokenKind::KwEnum;
    }
    if (text == "variant") {
        return TokenKind::KwVariant;
    }
    if (text == "match") {
        return TokenKind::KwMatch;
    }
    if (text == "for") {
        return TokenKind::KwFor;
    }
    if (text == "true") {
        return TokenKind::KwTrue;
    }
    if (text == "false") {
        return TokenKind::KwFalse;
    }
    if (text == "extern") {
        return TokenKind::KwExtern;
    }
    if (text == "if") {
        return TokenKind::KwIf;
    }
    if (text == "else") {
        return TokenKind::KwElse;
    }
    if (text == "while") {
        return TokenKind::KwWhile;
    }
    if (text == "in") {
        return TokenKind::KwIn;
    }
    if (text == "break") {
        return TokenKind::KwBreak;
    }
    if (text == "continue") {
        return TokenKind::KwContinue;
    }
    if (text == "mod") {
        return TokenKind::KwMod;
    }
    if (text == "use") {
        return TokenKind::KwUse;
    }
    if (text == "trait") {
        return TokenKind::KwTrait;
    }
    if (text == "as") {
        return TokenKind::KwAs;
    }
    if (text == "null") {
        return TokenKind::KwNull;
    }
    return TokenKind::Ident;
}

[[nodiscard]] TokenKind single_punct(char c) {
    switch (c) {
        case '(':
            return TokenKind::LParen;
        case ')':
            return TokenKind::RParen;
        case '[':
            return TokenKind::LBracket;
        case ']':
            return TokenKind::RBracket;
        case '{':
            return TokenKind::LBrace;
        case '}':
            return TokenKind::RBrace;
        case '.':
            return TokenKind::Dot;
        case '<':
            return TokenKind::Lt;
        case '>':
            return TokenKind::Gt;
        case '!':
            return TokenKind::Bang;
        case '&':
            return TokenKind::Eof;
        case '|':
            return TokenKind::Pipe;
        case ':':
            return TokenKind::Colon;
        case ',':
            return TokenKind::Comma;
        case ';':
            return TokenKind::Semicolon;
        case '=':
            return TokenKind::Equal;
        case '+':
            return TokenKind::Plus;
        case '-':
            return TokenKind::Minus;
        case '*':
            return TokenKind::Star;
        case '/':
            return TokenKind::Slash;
        case '%':
            return TokenKind::Percent;
        case '?':
            return TokenKind::Question;
        default:
            return TokenKind::Eof;
    }
}

struct Lexer {
    const Source& src;
    std::string_view text;
    DiagnosticEngine& diags;
    std::size_t i = 0;
    std::vector<Token> tokens;

    Lexer(const Source& src, DiagnosticEngine& diags)
        : src(src), text(src.view()), diags(diags) {
        tokens.reserve(text.size() / 2 + 8);
    }

    [[nodiscard]] bool eof() const { return i >= text.size(); }

    [[nodiscard]] char ahead(std::size_t n = 0) const {
        return i + n < text.size() ? text[i + n] : '\0';
    }

    void push(TokenKind kind, std::size_t begin, std::size_t end) {
        tokens.push_back(Token{.kind = kind, .offset = begin, .length = end - begin});
    }

    void skip_spaces() {
        while (!eof() && is_space(text[i])) {
            ++i;
        }
    }

    bool try_line_comment() {
        if (ahead() != '/' || ahead(1) != '/') {
            return false;
        }

        i += 2;
        while (!eof() && text[i] != '\n') {
            ++i;
        }
        return true;
    }

    bool try_block_comment() {
        if (ahead() != '/' || ahead(1) != '*') {
            return false;
        }

        const std::size_t begin = i;
        i += 2;

        while (i + 1 < text.size()) {
            if (text[i] == '*' && text[i + 1] == '/') {
                i += 2;
                return true;
            }
            ++i;
        }

        diags.error(src, begin, "unterminated block comment");
        i = text.size();
        return true;
    }

    void scan_ident() {
        const std::size_t begin = i;
        ++i;
        while (!eof() && is_ident_continue(text[i])) {
            ++i;
        }
        push(keyword_kind(text.substr(begin, i - begin)), begin, i);
    }

    void consume_digits() {
        while (!eof() && (is_digit(text[i]) || text[i] == '_')) {
            ++i;
        }
    }

    void scan_number() {
        const std::size_t begin = i;
        consume_digits();

        const bool is_float = !eof() && text[i] == '.' && is_digit(ahead(1));
        if (is_float) {
            ++i;
            consume_digits();
            push(TokenKind::Float, begin, i);
            return;
        }

        push(TokenKind::Int, begin, i);
    }

    void scan_string() {
        const std::size_t begin = i;
        ++i;
        while (!eof() && text[i] != '"' && text[i] != '\n') {
            if (text[i] == '\\') {
                ++i;
                if (eof()) {
                    break;
                }
            }
            ++i;
        }
        if (eof() || text[i] != '"') {
            diags.error(src, begin, "unterminated string literal");
            i = text.size();
            return;
        }
        ++i;
        push(TokenKind::String, begin, i);
    }

    void scan_char() {
        const std::size_t begin = i;
        ++i;
        if (eof() || text[i] == '\n') {
            diags.error(src, begin, "unterminated char literal");
            return;
        }
        if (text[i] == '\\') {
            ++i;
            if (!eof()) {
                ++i;
            }
        } else {
            ++i;
        }
        if (eof() || text[i] != '\'') {
            diags.error(src, begin, "unterminated char literal");
            while (!eof() && text[i] != '\n' && text[i] != '\'') {
                ++i;
            }
            if (!eof() && text[i] == '\'') {
                ++i;
            }
            return;
        }
        ++i;
        push(TokenKind::Char, begin, i);
    }

    void scan_punct() {
        if (ahead() == '-' && ahead(1) == '>') {
            push(TokenKind::Arrow, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '=' && ahead(1) == '>') {
            push(TokenKind::FatArrow, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == ':' && ahead(1) == ':') {
            push(TokenKind::ColonColon, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '=' && ahead(1) == '=') {
            push(TokenKind::EqEq, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '!' && ahead(1) == '=') {
            push(TokenKind::BangEq, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '<' && ahead(1) == '=') {
            push(TokenKind::Le, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '>' && ahead(1) == '=') {
            push(TokenKind::Ge, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '&' && ahead(1) == '&') {
            push(TokenKind::AmpAmp, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '|' && ahead(1) == '|') {
            push(TokenKind::PipePipe, i, i + 2);
            i += 2;
            return;
        }
        if (ahead() == '.' && ahead(1) == '.') {
            push(TokenKind::DotDot, i, i + 2);
            i += 2;
            return;
        }

        const TokenKind kind = single_punct(text[i]);
        if (kind == TokenKind::Eof) {
            diags.error(src, i, std::string("unexpected character '") + text[i] + "'");
            ++i;
            return;
        }

        push(kind, i, i + 1);
        ++i;
    }

    std::vector<Token> run() {
        while (!eof()) {
            const char c = text[i];

            if (is_space(c)) {
                skip_spaces();
                continue;
            }
            if (try_line_comment() || try_block_comment()) {
                continue;
            }
            if (is_ident_start(c)) {
                scan_ident();
                continue;
            }
            if (is_digit(c)) {
                scan_number();
                continue;
            }
            if (c == '"') {
                scan_string();
                continue;
            }
            if (c == '\'') {
                scan_char();
                continue;
            }

            scan_punct();
        }

        push(TokenKind::Eof, text.size(), text.size());
        return std::move(tokens);
    }
};

}  // namespace

std::vector<Token> lex(const Source& src, DiagnosticEngine& diags) {
    return Lexer{src, diags}.run();
}

}  // namespace qpc
