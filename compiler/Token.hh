#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

enum class TokenKind {
    Arrow,
    Comma,
    Eof,
    Eq,
    Identifier,
    IntLit,
    KeywordFn,
    KeywordLet,
    KeywordMatch,
    KeywordReturn,
    KeywordYield,
    LeftBrace,
    LeftParen,
    Minus,
    Plus,
    RightBrace,
    RightParen,
    Semi,
};

class Token {
    std::size_t m_int_data{0};
    const void *m_ptr_data{nullptr};
    TokenKind m_kind;

public:
    static std::string_view kind_string(TokenKind kind);

    Token(TokenKind kind) : m_kind(kind) {}
    Token(std::string_view text) : m_kind(TokenKind::Identifier), m_int_data(text.length()), m_ptr_data(text.data()) {}
    Token(std::size_t number) : m_kind(TokenKind::IntLit), m_int_data(number) {}
    Token(const Token &) = delete;
    Token(Token &&other) noexcept
        : m_int_data(std::exchange(other.m_int_data, 0)), m_ptr_data(std::exchange(other.m_ptr_data, nullptr)),
          m_kind(other.m_kind) {}
    ~Token();

    Token &operator=(const Token &) = delete;
    Token &operator=(Token &&other) noexcept {
        m_int_data = std::exchange(other.m_int_data, 0);
        m_ptr_data = std::exchange(other.m_ptr_data, nullptr);
        m_kind = other.m_kind;
        return *this;
    }

    TokenKind kind() const { return m_kind; }
    std::size_t number() const { return m_int_data; }
    std::string_view text() const;
    std::string_view to_string() const;
};
