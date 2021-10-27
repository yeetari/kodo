#pragma once

#include <CharStream.hh>
#include <SourceLocation.hh>
#include <Token.hh>

#include <utility>

class Lexer {
    CharStream m_stream;
    SourceLocation m_location{0, 0, {}};
    Token m_peek_token{TokenKind::Eof};
    bool m_peek_ready{false};

    Token next_token();

public:
    explicit Lexer(CharStream &&stream) : m_stream(std::move(stream)) {}

    bool has_next();
    Token next();
    const Token &peek();

    const SourceLocation &location() const { return m_location; }
};
