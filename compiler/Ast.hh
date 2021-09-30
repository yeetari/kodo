#pragma once

#include <codegen/support/List.hh>
#include <codegen/support/ListNode.hh>

#include <cstddef>
#include <memory>
#include <string>

namespace ast {

class Visitor;

// TODO: Not all nodes are part of a list.
class Node : public ListNode {
public:
    virtual void accept(Visitor *visitor) const = 0;
};

enum class BinaryOp {
    Add,
    Sub,
};

class BinaryExpr : public Node {
    const BinaryOp m_op;
    const std::unique_ptr<const Node> m_lhs;
    const std::unique_ptr<const Node> m_rhs;

public:
    BinaryExpr(BinaryOp op, std::unique_ptr<const Node> &&lhs, std::unique_ptr<const Node> &&rhs)
        : m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}

    void accept(Visitor *visitor) const override;

    BinaryOp op() const { return m_op ;}
    const Node &lhs() const { return *m_lhs; }
    const Node &rhs() const { return *m_rhs; }
};

class Block : public Node {
    List<const Node> m_stmts;

public:
    void add_stmt(std::unique_ptr<const Node> &&stmt) { m_stmts.insert(m_stmts.end(), stmt.release()); }

    auto begin() const { return m_stmts.begin(); }
    auto end() const { return m_stmts.end(); }

    void accept(Visitor *visitor) const override;

    const List<const Node> &stmts() const { return m_stmts; }
};

class DeclStmt : public Node {
    const std::string m_name;
    const std::unique_ptr<const Node> m_value;

public:
    DeclStmt(std::string name, std::unique_ptr<const Node> &&value)
        : m_name(std::move(name)), m_value(std::move(value)) {}

    void accept(Visitor *visitor) const override;

    const std::string &name() const { return m_name; }
    const Node &value() const { return *m_value; }
};

class FunctionDecl : public Node {
    const std::string m_name;
    std::unique_ptr<const Block> m_block;

public:
    explicit FunctionDecl(std::string name) : m_name(std::move(name)) {}

    void accept(Visitor *visitor) const override;
    void set_block(std::unique_ptr<const Block> &&block) { m_block = std::move(block); }

    const std::string &name() const { return m_name; }
    const Block &block() const { return *m_block; }
};

class IntegerLiteral : public Node {
    const std::size_t m_value;

public:
    explicit IntegerLiteral(std::size_t value) : m_value(value) {}

    void accept(Visitor *visitor) const override;

    std::size_t value() const { return m_value; }
};

class ReturnStmt : public Node {
    const std::unique_ptr<const Node> m_value;

public:
    explicit ReturnStmt(std::unique_ptr<const Node> &&value) : m_value(std::move(value)) {}

    void accept(Visitor *visitor) const override;

    const Node &value() const { return *m_value; }
};

class Symbol : public Node {
    const std::string m_name;

public:
    explicit Symbol(std::string name) : m_name(std::move(name)) {}

    void accept(Visitor *visitor) const override;

    const std::string &name() const { return m_name; }
};

struct Visitor {
    virtual void visit(const BinaryExpr &binary_expr) = 0;
    virtual void visit(const Block &block) = 0;
    virtual void visit(const DeclStmt &decl_stmt) = 0;
    virtual void visit(const FunctionDecl &function_decl) = 0;
    virtual void visit(const IntegerLiteral &integer_literal) = 0;
    virtual void visit(const ReturnStmt &return_stmt) = 0;
    virtual void visit(const Symbol &symbol) = 0;
};

inline void BinaryExpr::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void Block::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void DeclStmt::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void FunctionDecl::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void IntegerLiteral::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void ReturnStmt::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void Symbol::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

} // namespace ast
