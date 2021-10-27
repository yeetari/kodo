#include <Lexer.hh>

#include <CharStream.hh>
#include <Diagnostic.hh>

#include <cctype>
#include <cstring>

Token Lexer::next_token() {
    while (std::isspace(m_stream.peek()) != 0) {
        m_stream.next();
    }
    m_location = m_stream.location();
    if (!m_stream.has_next()) {
        return TokenKind::Eof;
    }

    char ch = m_stream.next();
    switch (ch) {
    case ':':
        return TokenKind::Colon;
    case ',':
        return TokenKind::Comma;
    case '=':
        if (m_stream.peek() == '>') {
            m_stream.next();
            return TokenKind::Arrow;
        }
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
        const char *start = m_stream.position_ptr() - 1;
        std::size_t length = 1;
        while (std::isalpha(ch = m_stream.peek()) != 0 || std::isdigit(ch) != 0 || ch == '_') {
            length++;
            m_stream.next();
        }
        std::string_view view(start, length);
        if (view == "fn") {
            return TokenKind::KeywordFn;
        }
        if (view == "let") {
            return TokenKind::KeywordLet;
        }
        if (view == "match") {
            return TokenKind::KeywordMatch;
        }
        if (view == "return") {
            return TokenKind::KeywordReturn;
        }
        if (view == "yield") {
            return TokenKind::KeywordYield;
        }
        return view;
    }
    Diagnostic(m_location, "unexpected '{}'", ch);
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
