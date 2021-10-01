#include <Lexer.hh>

#include <CharStream.hh>

#include <fmt/core.h>

#include <cctype>
#include <cstring>

Token Lexer::next_token() {
    while (std::isspace(m_stream.peek()) != 0) {
        m_stream.next();
    }
    if (!m_stream.has_next()) {
        return TokenKind::Eof;
    }

    char ch = m_stream.next();
    switch (ch) {
    case ',':
        return TokenKind::Comma;
    case '=':
        return TokenKind::Eq;
    case '{':
        return TokenKind::LeftBrace;
    case '(':
        return TokenKind::LeftParen;
    case '-':
        return TokenKind::Minus;
    case '+':
        return TokenKind::Plus;
    case '}':
        return TokenKind::RightBrace;
    case ')':
        return TokenKind::RightParen;
    case ';':
        return TokenKind::Semi;
    case '/':
        if (m_stream.peek() == '/') {
            m_stream.next();
            while (m_stream.peek() != '\n') {
                m_stream.next();
            }
            return next_token();
        }
        break;
    }
    if (std::isdigit(ch) != 0) {
        std::size_t number = ch - '0';
        while (std::isdigit(m_stream.peek()) != 0) {
            number = number * 10 + (m_stream.next() - '0');
        }
        return number;
    }
    if (std::isalpha(ch) != 0 || ch == '_') {
        std::string buf;
        buf += ch;
        while (std::isalpha(ch = m_stream.peek()) != 0 || std::isdigit(ch) != 0 || ch == '_') {
            buf += m_stream.next();
        }
        if (buf == "fn") {
            return TokenKind::KeywordFn;
        }
        if (buf == "let") {
            return TokenKind::KeywordLet;
        }
        if (buf == "return") {
            return TokenKind::KeywordReturn;
        }
        if (buf == "yield") {
            return TokenKind::KeywordYield;
        }
        // TODO: Don't strdup, make std::string disown memory somehow.
        const char *copy = strdup(buf.c_str());
        return std::string_view(copy, buf.length());
    }
    fmt::print("error: unexpected '{}'\n", ch);
    return TokenKind::Eof;
}

bool Lexer::has_next() {
    return peek().kind() != TokenKind::Eof;
}

Token Lexer::next() {
    if (m_peek_ready) {
        m_peek_ready = false;
        return std::move(m_peek_token);
    }
    return next_token();
}

const Token &Lexer::peek() {
    if (!m_peek_ready) {
        m_peek_ready = true;
        m_peek_token = next_token();
    }
    return m_peek_token;
}
