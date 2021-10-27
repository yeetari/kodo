#include <Parser.hh>

#include <Ast.hh>
#include <Diagnostic.hh>
#include <Lexer.hh>
#include <Token.hh>

#include <coel/support/Assert.hh>
#include <coel/support/Stack.hh>

namespace {

enum class Op {
    Add,
    Sub,
};

constexpr int precedence(Op op) {
    switch (op) {
    case Op::Add:
    case Op::Sub:
        return 1;
    }
}

constexpr int compare_op(Op op1, Op op2) {
    int p1 = precedence(op1);
    int p2 = precedence(op2);
    return p1 > p2 ? 1 : p1 < p2 ? -1 : 0;
}

std::unique_ptr<ast::Node> create_expr(const SourceLocation &location, Op op,
                                       coel::Stack<std::unique_ptr<ast::Node>> &operands) {
    auto rhs = operands.pop();
    auto lhs = operands.pop();
    switch (op) {
    case Op::Add:
        return std::make_unique<ast::BinaryExpr>(location, ast::BinaryOp::Add, std::move(lhs), std::move(rhs));
    case Op::Sub:
        return std::make_unique<ast::BinaryExpr>(location, ast::BinaryOp::Sub, std::move(lhs), std::move(rhs));
    }
}

} // namespace

std::optional<Token> Parser::consume(TokenKind kind) {
    if (m_lexer.peek().kind() == kind) {
        return m_lexer.next();
    }
    return std::nullopt;
}

Token Parser::expect(TokenKind kind) {
    auto next = m_lexer.next();
    if (next.kind() != kind) {
        Diagnostic(m_lexer.location(), "expected {} but got {}", Token::kind_string(kind), next.to_string());
    }
    return next;
}

std::unique_ptr<ast::CallExpr> Parser::parse_call_expr(const SourceLocation &location,
                                                       std::unique_ptr<ast::Symbol> &&name) {
    auto call_expr = std::make_unique<ast::CallExpr>(location, std::move(name));
    expect(TokenKind::LeftParen);
    while (m_lexer.peek().kind() != TokenKind::RightParen) {
        call_expr->add_arg(parse_expr());
        consume(TokenKind::Comma);
    }
    expect(TokenKind::RightParen);
    return call_expr;
}

std::unique_ptr<ast::MatchExpr> Parser::parse_match_expr() {
    expect(TokenKind::KeywordMatch);
    auto location = m_lexer.location();
    expect(TokenKind::LeftParen);
    auto match_expr = std::make_unique<ast::MatchExpr>(location, parse_expr());
    expect(TokenKind::RightParen);
    expect(TokenKind::LeftBrace);
    while (m_lexer.peek().kind() != TokenKind::RightBrace) {
        auto arm_lhs = parse_expr();
        expect(TokenKind::Arrow);
        auto arm_rhs = parse_expr();
        match_expr->add_arm(std::move(arm_lhs), std::move(arm_rhs));
        expect(TokenKind::Comma);
    }
    expect(TokenKind::RightBrace);
    return match_expr;
}

std::unique_ptr<ast::Node> Parser::parse_expr() {
    coel::Stack<std::unique_ptr<ast::Node>> operands;
    coel::Stack<Op> operators;
    bool keep_parsing = true;
    while (keep_parsing) {
        std::optional<Op> op1;
        switch (m_lexer.peek().kind()) {
        case TokenKind::Plus:
            op1 = Op::Add;
            break;
        case TokenKind::Minus:
            op1 = Op::Sub;
            break;
        default:
            break;
        }
        if (!op1) {
            switch (m_lexer.peek().kind()) {
            case TokenKind::Identifier: {
                std::string text(expect(TokenKind::Identifier).text());
                auto location = m_lexer.location();
                auto symbol = std::make_unique<ast::Symbol>(location, std::move(text));
                if (m_lexer.peek().kind() == TokenKind::LeftParen) {
                    operands.push(parse_call_expr(location, std::move(symbol)));
                } else {
                    operands.push(std::move(symbol));
                }
                break;
            }
            case TokenKind::IntLit: {
                auto number = expect(TokenKind::IntLit).number();
                operands.push(std::make_unique<ast::IntegerLiteral>(m_lexer.location(), number));
                break;
            }
            case TokenKind::KeywordMatch:
                operands.push(parse_match_expr());
                break;
            case TokenKind::LeftBrace: {
                operands.push(parse_block());
                break;
            }
            default:
                keep_parsing = false;
                break;
            }
            continue;
        }
        m_lexer.next();
        while (!operators.empty()) {
            auto op2 = operators.peek();
            if (compare_op(*op1, op2) >= 0) {
                break;
            }
            auto op = operators.pop();
            operands.push(create_expr(m_lexer.location(), op, operands));
        }
        operators.push(*op1);
    }
    while (!operators.empty()) {
        auto op = operators.pop();
        if (operands.size() < 2) {
            const auto &actual = m_lexer.peek();
            Diagnostic(m_lexer.location(), "expected expression before {} token", actual.to_string());
        }
        operands.push(create_expr(m_lexer.location(), op, operands));
    }
    COEL_ASSERT(operands.size() == 1);
    return operands.pop();
}

std::unique_ptr<ast::DeclStmt> Parser::parse_decl_stmt() {
    auto location = m_lexer.location();
    if (!consume(TokenKind::KeywordLet)) {
        return {};
    }
    auto name = expect(TokenKind::Identifier);
    expect(TokenKind::Eq);
    auto expr = parse_expr();
    expect(TokenKind::Semi);
    return std::make_unique<ast::DeclStmt>(location, std::string(name.text()), std::move(expr));
}

std::unique_ptr<ast::ReturnStmt> Parser::parse_return_stmt() {
    auto location = m_lexer.location();
    if (!consume(TokenKind::KeywordReturn)) {
        return {};
    }
    auto expr = parse_expr();
    expect(TokenKind::Semi);
    return std::make_unique<ast::ReturnStmt>(location, std::move(expr));
}

std::unique_ptr<ast::YieldStmt> Parser::parse_yield_stmt() {
    auto location = m_lexer.location();
    if (!consume(TokenKind::KeywordYield)) {
        return {};
    }
    auto expr = parse_expr();
    expect(TokenKind::Semi);
    return std::make_unique<ast::YieldStmt>(location, std::move(expr));
}

std::unique_ptr<ast::Node> Parser::parse_stmt() {
    if (auto decl_stmt = parse_decl_stmt()) {
        return decl_stmt;
    }
    if (auto return_stmt = parse_return_stmt()) {
        return return_stmt;
    }
    if (auto yield_stmt = parse_yield_stmt()) {
        return yield_stmt;
    }
    const auto &actual = m_lexer.peek();
    Diagnostic(m_lexer.location(), "expected a statement but got {}", actual.to_string());
    COEL_ENSURE_NOT_REACHED();
}

std::unique_ptr<ast::Block> Parser::parse_block() {
    auto block = std::make_unique<ast::Block>(m_lexer.location());
    expect(TokenKind::LeftBrace);
    while (m_lexer.has_next() && m_lexer.peek().kind() != TokenKind::RightBrace) {
        block->add_stmt(parse_stmt());
    }
    expect(TokenKind::RightBrace);
    return block;
}

std::unique_ptr<ast::Type> Parser::parse_type() {
    auto name = expect(TokenKind::Identifier);
    return std::make_unique<ast::BaseType>(std::string(name.text()));
}

std::unique_ptr<ast::Root> Parser::parse() {
    auto root = std::make_unique<ast::Root>();
    while (m_lexer.has_next()) {
        expect(TokenKind::KeywordFn);
        auto name = expect(TokenKind::Identifier);
        expect(TokenKind::LeftParen);
        auto function = std::make_unique<ast::FunctionDecl>(m_lexer.location(), std::string(name.text()));
        while (m_lexer.peek().kind() != TokenKind::RightParen) {
            expect(TokenKind::KeywordLet);
            auto location = m_lexer.location();
            auto arg_name = expect(TokenKind::Identifier);
            expect(TokenKind::Colon);
            function->add_arg({location, std::string(arg_name.text()), parse_type()});
            consume(TokenKind::Comma);
        }
        expect(TokenKind::RightParen);
        if (consume(TokenKind::Colon)) {
            function->set_return_type(parse_type());
        }
        function->set_block(parse_block());
        root->add_function(std::move(function));
    }
    return root;
}
