#pragma once

#include <coel/ir/Type.hh>
#include <coel/support/List.hh>
#include <coel/support/ListNode.hh>

#include <cstddef>
#include <vector>

namespace hir {

class Function;
struct Visitor;

enum class TypeKind {
    Infer,
    Real,
};

class Type {
    union {
        const coel::ir::Type *m_real{nullptr};
    };
    TypeKind m_kind;

public:
    Type(TypeKind kind) : m_kind(kind) {}
    Type(const coel::ir::Type *real) : m_kind(TypeKind::Real), m_real(real) {}

    bool operator==(const Type &other) const {
        if (m_kind != other.m_kind) {
            return false;
        }
        if (m_kind == TypeKind::Infer) {
            return true;
        }
        COEL_ASSERT(is_real());
        return m_real == other.m_real;
    }

    bool is_infer() const { return m_kind == TypeKind::Infer; }
    bool is_real() const { return m_kind == TypeKind::Real; }
    const coel::ir::Type *real() const {
        COEL_ASSERT(is_real());
        return m_real;
    }
};

class Stmt : public coel::ListNode {
public:
    virtual void accept(Visitor *visitor) const = 0;
};

enum class ExprKind {
    // Binary
    Add,
    Sub,

    Argument,
    Block,
    Call,
    Constant,
    Match,
    Var,
};

using ExprId = std::size_t;

class Expr {
    union {
        struct {
            std::size_t index{0};
        } m_argument;
        struct {
            ExprId lhs{0};
            ExprId rhs{0};
        } m_binary;
        struct {
            coel::List<Stmt> *stmts{nullptr};
        } m_block;
        struct {
            const Function *callee{nullptr};
            const ExprId *args{nullptr};
        } m_call;
        struct {
            std::size_t value{0};
        } m_constant;
        struct {
            ExprId matchee{0};
            const std::pair<ExprId, ExprId> *arms{nullptr};
            std::size_t arm_count;
        } m_match;
    };
    Type m_type{TypeKind::Infer};
    ExprKind m_kind;

public:
    Expr(ExprKind kind, Type type) : m_kind(kind), m_type(type) {
        if (kind == ExprKind::Block) {
            m_block.stmts = new coel::List<Stmt>;
        }
    }
    Expr(ExprKind kind, Type type, std::size_t value) : m_kind(kind), m_type(type), m_argument{value} {}
    Expr(ExprKind op, ExprId lhs, ExprId rhs) : m_kind(op), m_binary{lhs, rhs} {}
    Expr(const Function *callee, Type type, const ExprId *args)
        : m_kind(ExprKind::Call), m_type(type), m_call{callee, args} {}
    Expr(ExprKind kind, std::size_t value) : m_kind(kind), m_constant{value} {}
    Expr(ExprId matchee, const std::pair<ExprId, ExprId> *arms, std::size_t arm_count)
        : m_kind(ExprKind::Match), m_match{matchee, arms, arm_count} {}
    Expr(const Expr &) = delete;
    Expr(Expr &&other) noexcept : m_type(other.m_type), m_kind(other.m_kind), m_binary(other.m_binary) {
        if (m_kind == ExprKind::Block) {
            other.m_block.stmts = nullptr;
        } else if (m_kind == ExprKind::Call) {
            other.m_call.args = nullptr;
        } else if (m_kind == ExprKind::Match) {
            other.m_match.arms = nullptr;
        }
    }
    ~Expr() {
        if (m_kind == ExprKind::Block) {
            delete m_block.stmts;
        } else if (m_kind == ExprKind::Call) {
            delete[] m_call.args;
        } else if (m_kind == ExprKind::Match) {
            delete[] m_match.arms;
        }
    }

    Expr &operator=(const Expr &) = delete;
    Expr &operator=(Expr &&) = delete;

    template <typename T, typename... Args>
    void append(Args &&...args) {
        COEL_ASSERT(m_kind == ExprKind::Block);
        m_block.stmts->emplace<T>(m_block.stmts->end(), std::forward<Args>(args)...);
    }
    void set_type(Type type) {
        // TODO: Should probably be asserts.
        if (m_kind == ExprKind::Argument || m_kind == ExprKind::Call) {
            return;
        }
        m_type = type;
    }

    const Type &type() const { return m_type; }
    ExprKind kind() const { return m_kind; }
    std::size_t argument_index() const { return m_argument.index; }
    ExprId binary_lhs() const { return m_binary.lhs; }
    ExprId binary_rhs() const { return m_binary.rhs; }
    const coel::List<Stmt> &block_stmts() const { return *m_block.stmts; }
    const Function *call_callee() const { return m_call.callee; }
    const ExprId *call_args() const { return m_call.args; }
    std::size_t constant_value() const { return m_constant.value; }
    ExprId match_matchee() const { return m_match.matchee; }
    const std::pair<ExprId, ExprId> *match_arms() const { return m_match.arms; }
    std::size_t match_arm_count() const { return m_match.arm_count; }
};

class DeclStmt final : public Stmt {
    const ExprId m_var;
    const ExprId m_value;

public:
    DeclStmt(ExprId var, ExprId value) : m_var(var), m_value(value) {}

    void accept(Visitor *visitor) const override;

    ExprId var() const { return m_var; }
    ExprId value() const { return m_value; }
};

class Function final : public coel::ListNode {
    const std::string m_name;
    const std::vector<ExprId> m_params;
    ExprId m_block{0};

public:
    Function(std::string &&name, std::vector<ExprId> &&params) : m_name(std::move(name)), m_params(std::move(params)) {}

    void accept(Visitor *visitor) const;
    void set_block(ExprId block) { m_block = block; }

    const std::string &name() const { return m_name; }
    const std::vector<ExprId> &params() const { return m_params; }
    ExprId block() const { return m_block; }
};

class ReturnStmt final : public Stmt {
    const ExprId m_value;

public:
    explicit ReturnStmt(ExprId value) : m_value(value) {}

    void accept(Visitor *visitor) const override;

    ExprId value() const { return m_value; }
};

class Root {
    coel::List<Function> m_functions;
    std::vector<Expr> m_exprs;

public:
    Function *append_function(std::string name, std::vector<ExprId> &&params) {
        return m_functions.emplace<Function>(m_functions.end(), std::move(name), std::move(params));
    }

    template <typename... Args>
    ExprId create_expr(Args &&...args) {
        m_exprs.emplace_back(std::forward<Args>(args)...);
        return m_exprs.size() - 1;
    }

    auto begin() const { return m_functions.begin(); }
    auto end() const { return m_functions.end(); }
    Expr &expr(ExprId id) { return m_exprs[id]; }
    const Expr &expr(ExprId id) const { return m_exprs[id]; }
    const Type &type(ExprId id) const { return m_exprs[id].type(); }
    std::size_t expr_count() const { return m_exprs.size(); }
};

struct Visitor {
    virtual void visit(const DeclStmt &decl_stmt) = 0;
    virtual void visit(const Function &function) = 0;
    virtual void visit(const ReturnStmt &return_stmt) = 0;
};

inline void DeclStmt::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void Function::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

inline void ReturnStmt::accept(Visitor *visitor) const {
    visitor->visit(*this);
}

} // namespace hir
