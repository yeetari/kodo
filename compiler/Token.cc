#include <Token.hh>

#include <coel/support/Assert.hh>
#include <fmt/format.h>

#include <cstdlib>

std::string_view Token::kind_string(TokenKind kind) {
    using namespace std::literals;
    switch (kind) {
    case TokenKind::Arrow:
        return "'=>'"sv;
    case TokenKind::Colon:
        return "':'"sv;
    case TokenKind::Comma:
        return "','"sv;
    case TokenKind::Eof:
        return "eof"sv;
    case TokenKind::Eq:
        return "'='"sv;
    case TokenKind::Identifier:
        return "identifier"sv;
    case TokenKind::IntLit:
        return "number"sv;
    case TokenKind::KeywordFn:
        return "'fn'"sv;
    case TokenKind::KeywordLet:
        return "'let'"sv;
    case TokenKind::KeywordMatch:
        return "'match'"sv;
    case TokenKind::KeywordReturn:
        return "'return'"sv;
    case TokenKind::KeywordYield:
        return "'yield'"sv;
    case TokenKind::LeftBrace:
        return "'{'"sv;
    case TokenKind::LeftParen:
        return "'('"sv;
    case TokenKind::Minus:
        return "'-'"sv;
    case TokenKind::Plus:
        return "'+'"sv;
    case TokenKind::RightBrace:
        return "'}'"sv;
    case TokenKind::RightParen:
        return "')'"sv;
    case TokenKind::Semi:
        return "';'"sv;
    }
}

std::string_view Token::text() const {
    COEL_ASSERT(m_kind == TokenKind::Identifier);
    return {static_cast<const char *>(m_ptr_data), m_int_data};
}

std::string Token::to_string() const {
    switch (m_kind) {
    case TokenKind::Identifier:
        return fmt::format("'{}'", text());
    case TokenKind::IntLit:
        return fmt::format("'{}'", number());
    default:
        return std::string(kind_string(m_kind));
    }
}
