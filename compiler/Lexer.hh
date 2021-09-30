#pragma once

#include <Token.hh>

class CharStream;

class Lexer {
    CharStream &m_stream;
    Token m_peek_token{TokenKind::Eof};
    bool m_peek_ready{false};

    Token next_token();

public:
    explicit Lexer(CharStream &stream) : m_stream(stream) {}

    bool has_next();
    Token next();
    const Token &peek();
};
