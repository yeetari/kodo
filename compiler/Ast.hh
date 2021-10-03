#pragma once

#include <codegen/support/List.hh>
#include <codegen/support/ListNode.hh>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ast {

class Symbol;
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

    BinaryOp op() const { return m_op; }
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
};

class CallExpr : public Node {
    const std::unique_ptr<const Symbol> m_callee;
    List<const Node> m_args;

public:
    explicit CallExpr(std::unique_ptr<const Symbol> &&callee) : m_callee(std::move(callee)) {}

    void accept(Visitor *visitor) const override;
    void add_arg(std::unique_ptr<const Node> &&arg) { m_args.insert(m_args.end(), arg.release()); }

    const Symbol &callee() const { return *m_callee; }
    const List<const Node> &args() const { return m_args; }
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
    std::vector<std::string> m_args;
    std::unique_ptr<const Block> m_block;

public:
    explicit FunctionDecl(std::string name) : m_name(std::move(name)) {}

    void accept(Visitor *visitor) const override;
    void add_arg(std::string name) { m_args.push_back(std::move(name)); }
    void set_block(std::unique_ptr<const Block> &&block) { m_block = std::move(block); }

    const std::string &name() const { return m_name; }
    const std::vector<std::string> &args() const { return m_args; }
    const Block &block() const { return *m_block; }
};

class IntegerLiteral : public Node {
    const std::size_t m_value;

public:
    explicit IntegerLiteral(std::size_t value) : m_value(value) {}

    void accept(Visitor *visitor) const override;

    std::size_t value() const { return m_value; }
};

class MatchArm {
    std::unique_ptr<const Node> m_lhs;
    std::unique_ptr<const Node> m_rhs;

public:
    MatchArm(std::unique_ptr<const Node> &&lhs, std::unique_ptr<const Node> &&rhs)
        : m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}

    const Node &lhs() const { return *m_lhs; }
    const Node &rhs() const { return *m_rhs; }
};

class MatchExpr : public Node {
    const std::unique_ptr<const Node> m_matchee;
    std::vector<MatchArm> m_arms;

public:
    explicit MatchExpr(std::unique_ptr<const Node> &&matchee) : m_matchee(std::move(matchee)) {}

    void accept(Visitor *visitor) const override;
    void add_arm(std::unique_ptr<const Node> &&lhs, std::unique_ptr<const Node> &&rhs) {
        m_arms.emplace_back(std::move(lhs), std::move(rhs));
    }

    const Node &matchee() const { return *m_matchee; }
    const std::vector<MatchArm> &arms() const { return m_arms; }
};

class ReturnStmt : public Node {
    const std::unique_ptr<const Node> m_value;

public:
    explicit ReturnStmt(std::unique_ptr<const Node> &&value) : m_value(std::move(value)) {}

    void accept(Visitor *visitor) const override;

    const Node &value() const { return *m_value; }
};

class Root : public Node {
    List<const FunctionDecl> m_functions;

public:
    void add_function(std::unique_ptr<const FunctionDecl> &&function) {
        m_functions.insert(m_functions.end(), function.release());
    }

    auto begin() const { return m_functions.begin(); }
    auto end() const { return m_functions.end(); }

    void accept(Visitor *visitor) const override;
};

class Symbol : public Node {
    const std::string m_name;

public:
    explicit Symbol(std::string name) : m_name(std::move(name)) {}

    void accept(Visitor *visitor) const override;

    const std::string &name() const { return m_name; }
};

class YieldStmt : public Node {
    const std::unique_ptr<const Node> m_value;

public:
    explicit YieldStmt(std::unique_ptr<const Node> &&value) : m_value(std::move(value)) {}

    void accept(Visitor *visitor) const override;

    const Node &value() const { return *m_value; }
};

struct Visitor {
    virtual void visit(const BinaryExpr &binary_expr) = 0;
    virtual void visit(const Block &block) = 0;
    virtual void visit(const CallExpr &call_expr) = 0;
    virtual void visit(const DeclStmt &decl_stmt) = 0;
    virtual void visit(const FunctionDecl &function_decl) = 0;
    virtual void visit(const IntegerLiteral &integer_literal) = 0;
    virtual void visit(const MatchExpr &match_expr) = 0;
    virtual void visit(const ReturnStmt &return_stmt) = 0;
    virtual void visit(const Root &root) = 0;
    virtual void visit(const Symbol &symbol) = 0;
    virtual void visit(const YieldStmt &yield_stmt) = 0;
};

inline void BinaryExpr::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void Block::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void CallExpr::accept(Visitor *visitor) const {
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

inline void MatchExpr::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void ReturnStmt::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void Root::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void Symbol::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void YieldStmt::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

} // namespace ast
