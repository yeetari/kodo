#pragma once

#include <SourceLocation.hh>

#include <coel/support/List.hh>
#include <coel/support/ListNode.hh>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ast {

class Symbol;
class Visitor;

// TODO: Not all nodes are part of a list.
class Node : public coel::ListNode {
    const SourceLocation m_location;

public:
    explicit Node(const SourceLocation &location) : m_location(location) {}

    virtual void accept(Visitor *visitor) const = 0;

    const SourceLocation &location() const { return m_location; }
};

class Type {
public:
    Type() = default;
    Type(const Type &) = delete;
    Type(Type &&) = delete;
    virtual ~Type() = default;

    Type &operator=(const Type &) = delete;
    Type &operator=(Type &&) = delete;

    template <typename T>
    const T *as() const {
        return dynamic_cast<const T *>(this);
    }
};

class BaseType : public Type {
    const std::string m_name;

public:
    explicit BaseType(std::string name) : m_name(std::move(name)) {}

    const std::string &name() const { return m_name; }
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
    BinaryExpr(const SourceLocation &location, BinaryOp op, std::unique_ptr<const Node> &&lhs,
               std::unique_ptr<const Node> &&rhs)
        : Node(location), m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}

    void accept(Visitor *visitor) const override;

    BinaryOp op() const { return m_op; }
    const Node &lhs() const { return *m_lhs; }
    const Node &rhs() const { return *m_rhs; }
};

class Block : public Node {
    coel::List<const Node> m_stmts;

public:
    explicit Block(const SourceLocation &location) : Node(location) {}

    void add_stmt(std::unique_ptr<const Node> &&stmt) { m_stmts.insert(m_stmts.end(), stmt.release()); }

    auto begin() const { return m_stmts.begin(); }
    auto end() const { return m_stmts.end(); }

    void accept(Visitor *visitor) const override;
};

class CallExpr : public Node {
    const std::unique_ptr<const Symbol> m_callee;
    coel::List<const Node> m_args;

public:
    CallExpr(const SourceLocation &location, std::unique_ptr<const Symbol> &&callee)
        : Node(location), m_callee(std::move(callee)) {}

    void accept(Visitor *visitor) const override;
    void add_arg(std::unique_ptr<const Node> &&arg) { m_args.insert(m_args.end(), arg.release()); }

    const Symbol &callee() const { return *m_callee; }
    const coel::List<const Node> &args() const { return m_args; }
};

class DeclStmt : public Node {
    const std::string m_name;
    const std::unique_ptr<const Node> m_value;

public:
    DeclStmt(const SourceLocation &location, std::string name, std::unique_ptr<const Node> &&value)
        : Node(location), m_name(std::move(name)), m_value(std::move(value)) {}

    void accept(Visitor *visitor) const override;

    const std::string &name() const { return m_name; }
    const Node &value() const { return *m_value; }
};

class FunctionArg {
    const SourceLocation m_location;
    std::string m_name;
    std::unique_ptr<const Type> m_type;

public:
    FunctionArg(const SourceLocation &location, std::string name, std::unique_ptr<const Type> &&type)
        : m_location(location), m_name(std::move(name)), m_type(std::move(type)) {}

    const SourceLocation &location() const { return m_location; }
    const std::string &name() const { return m_name; }
    const Type &type() const { return *m_type; }
};

class FunctionDecl : public Node {
    const std::string m_name;
    std::vector<FunctionArg> m_args;
    std::unique_ptr<const Block> m_block;
    std::unique_ptr<const Type> m_return_type;

public:
    FunctionDecl(const SourceLocation &location, std::string name) : Node(location), m_name(std::move(name)) {}

    void accept(Visitor *visitor) const override;
    void add_arg(FunctionArg &&arg) { m_args.push_back(std::move(arg)); }
    void set_block(std::unique_ptr<const Block> &&block) { m_block = std::move(block); }
    void set_return_type(std::unique_ptr<const Type> &&type) { m_return_type = std::move(type); }

    const std::string &name() const { return m_name; }
    const std::vector<FunctionArg> &args() const { return m_args; }
    const Block &block() const { return *m_block; }
    const Type &return_type() const { return *m_return_type; }
    bool has_return_type() const { return !!m_return_type; }
};

class IntegerLiteral : public Node {
    const std::size_t m_value;

public:
    IntegerLiteral(const SourceLocation &location, std::size_t value) : Node(location), m_value(value) {}

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
    MatchExpr(const SourceLocation &location, std::unique_ptr<const Node> &&matchee)
        : Node(location), m_matchee(std::move(matchee)) {}

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
    ReturnStmt(const SourceLocation &location, std::unique_ptr<const Node> &&value)
        : Node(location), m_value(std::move(value)) {}

    void accept(Visitor *visitor) const override;

    const Node &value() const { return *m_value; }
};

class Root : public Node {
    coel::List<const FunctionDecl> m_functions;

public:
    Root() : Node({0, 0, {}}) {}

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
    Symbol(const SourceLocation &location, std::string name) : Node(location), m_name(std::move(name)) {}

    void accept(Visitor *visitor) const override;

    const std::string &name() const { return m_name; }
};

class YieldStmt : public Node {
    const std::unique_ptr<const Node> m_value;

public:
    YieldStmt(const SourceLocation &location, std::unique_ptr<const Node> &&value)
        : Node(location), m_value(std::move(value)) {}

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
