#include <Token.hh>

#include <codegen/support/Assert.hh>

#include <cstdlib>

std::string_view Token::kind_string(TokenKind kind) {
    using namespace std::literals;
    switch (kind) {
    case TokenKind::Eof:
        return "eof"sv;
    case TokenKind::Eq:
        return "="sv;
    case TokenKind::Identifier:
        return "identifier"sv;
    case TokenKind::IntLit:
        return "number"sv;
    case TokenKind::KeywordFn:
        return "fn"sv;
    case TokenKind::KeywordLet:
        return "let"sv;
    case TokenKind::KeywordReturn:
        return "return"sv;
    case TokenKind::LeftBrace:
        return "{"sv;
    case TokenKind::LeftParen:
        return "("sv;
    case TokenKind::Minus:
        return "-"sv;
    case TokenKind::Plus:
        return "+"sv;
    case TokenKind::RightBrace:
        return "}"sv;
    case TokenKind::RightParen:
        return ")"sv;
    case TokenKind::Semi:
        return ";"sv;
    }
}

Token::~Token() {
    if (m_kind == TokenKind::Identifier) {
        std::free(const_cast<void *>(m_ptr_data));
    }
}

std::string_view Token::text() const {
    ASSERT(m_kind == TokenKind::Identifier);
    return {static_cast<const char *>(m_ptr_data), m_int_data};
}

std::string_view Token::to_string() const {
    switch (m_kind) {
    case TokenKind::Identifier:
        return text();
    default:
        return kind_string(m_kind);
    }
}
