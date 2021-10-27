#include <Analysis.hh>

#include <Diagnostic.hh>
#include <Hir.hh>

#include <coel/ir/Types.hh>
#include <coel/support/Stack.hh>
#include <fmt/format.h>

#include <cmath>
#include <vector>

namespace {

enum class ConstraintKind {
    Equals,
    ImplicitlyCastable,
    IntegerWidth,
};

class Constraint {
    union {
        struct {
            const hir::Type *type{nullptr};
        } m_equals;
        struct {
            hir::ExprId expr{0};
        } m_implicitly_castable;
        struct {
            std::size_t bit_width{0};
        } m_integer_width;
    };
    ConstraintKind m_kind;

public:
    Constraint(const hir::Type &type) : m_kind(ConstraintKind::Equals), m_equals{&type} {}
    Constraint(ConstraintKind kind, std::size_t bit_width) : m_kind(kind), m_integer_width{bit_width} {}

    ConstraintKind kind() const { return m_kind; }
    const hir::Type &equals_type() const { return *m_equals.type; }
    hir::ExprId implicitly_castable_expr() const { return m_implicitly_castable.expr; }
    std::size_t integer_width_bit_width() const { return m_integer_width.bit_width; }
};

class Constrainer final : public hir::Visitor {
    hir::Root &m_root;
    const hir::Function *m_function{nullptr};
    std::vector<coel::Stack<Constraint>> m_constraints;

    void analyse_binary(hir::ExprId id, hir::ExprId lhs_id, hir::ExprId rhs_id);
    void analyse_block(const coel::List<hir::Stmt> &stmts);
    void analyse_call(hir::ExprId id, const hir::Function *callee, const hir::ExprId *arg_ids);
    void analyse_constant(hir::ExprId id, std::size_t value);
    void analyse_match(hir::ExprId id, hir::ExprId matchee_id, const std::pair<hir::ExprId, hir::ExprId> *arms,
                       std::size_t arm_count);
    void analyse_var(hir::ExprId id);
    void analyse_expr(hir::ExprId id);

public:
    explicit Constrainer(hir::Root &root) : m_root(root) { m_constraints.resize(root.expr_count()); }

    void visit(const hir::DeclStmt &decl_stmt) override;
    void visit(const hir::Function &function) override;
    void visit(const hir::ReturnStmt &return_stmt) override;

    std::vector<coel::Stack<Constraint>> &constraints() { return m_constraints; }
};

class Unifier final : public hir::Visitor {
    hir::Root &m_root;
    std::vector<coel::Stack<Constraint>> &m_constraints;

    void analyse_binary(hir::ExprId lhs_id, hir::ExprId rhs_id);
    void analyse_block(const coel::List<hir::Stmt> &stmts);
    void analyse_call(const hir::Function *callee, const hir::ExprId *arg_ids);
    void analyse_match(hir::ExprId matchee_id, const std::pair<hir::ExprId, hir::ExprId> *arms, std::size_t arm_count);
    void analyse_expr(hir::ExprId id);

public:
    Unifier(hir::Root &root, std::vector<coel::Stack<Constraint>> &constraints)
        : m_root(root), m_constraints(constraints) {}

    void visit(const hir::DeclStmt &decl_stmt) override;
    void visit(const hir::Function &function) override;
    void visit(const hir::ReturnStmt &return_stmt) override;
};

std::string type_string(const coel::ir::Type *type) {
    if (const auto *bool_type = type->as<coel::ir::BoolType>()) {
        return "bool";
    }
    return fmt::format("u{}", type->as_non_null<coel::ir::IntegerType>()->bit_width());
}

std::string type_string(const hir::Type &type) {
    if (type.is_infer()) {
        return "?";
    }
    return type_string(type.real());
}

void Constrainer::analyse_binary(hir::ExprId id, hir::ExprId lhs_id, hir::ExprId rhs_id) {
    analyse_expr(lhs_id);
    analyse_expr(rhs_id);
    m_constraints[lhs_id].emplace(ConstraintKind::ImplicitlyCastable, id);
    m_constraints[rhs_id].emplace(ConstraintKind::ImplicitlyCastable, id);
}

void Constrainer::analyse_block(const coel::List<hir::Stmt> &stmts) {
    for (const auto *stmt : stmts) {
        stmt->accept(this);
    }
}

void Constrainer::analyse_call(hir::ExprId id, const hir::Function *callee, const hir::ExprId *arg_ids) {
    m_constraints[id].emplace(m_root.type(callee->block()));
    for (std::size_t i = 0; i < callee->params().size(); i++) {
        hir::ExprId arg_id = arg_ids[i];
        analyse_expr(arg_id);
        m_constraints[arg_id].emplace(ConstraintKind::ImplicitlyCastable, callee->params()[i]);
    }
}

void Constrainer::analyse_constant(hir::ExprId id, std::size_t value) {
    auto bit_width = static_cast<std::size_t>(std::ceil(std::log2(std::max(value, 1ul))));
    m_constraints[id].emplace(ConstraintKind::IntegerWidth, bit_width);
}

void Constrainer::analyse_match(hir::ExprId id, hir::ExprId matchee_id, const std::pair<hir::ExprId, hir::ExprId> *arms,
                                std::size_t arm_count) {
    analyse_expr(matchee_id);
    for (std::size_t i = 0; i < arm_count; i++) {
        analyse_expr(arms[i].first);
        analyse_expr(arms[i].second);
        m_constraints[matchee_id].emplace(ConstraintKind::ImplicitlyCastable, arms[i].first);
        m_constraints[arms[i].first].emplace(ConstraintKind::ImplicitlyCastable, matchee_id);
        m_constraints[arms[i].second].emplace(ConstraintKind::ImplicitlyCastable, id);
    }
}

void Constrainer::analyse_var(hir::ExprId) {
    // TODO: Explicit type, i.e. let foo: i32
}

void Constrainer::analyse_expr(hir::ExprId id) {
    auto &expr = m_root.expr(id);
    switch (expr.kind()) {
    case hir::ExprKind::Argument:
        break;
    case hir::ExprKind::Add:
    case hir::ExprKind::Sub:
        analyse_binary(id, expr.binary_lhs(), expr.binary_rhs());
        break;
    case hir::ExprKind::Block:
        analyse_block(expr.block_stmts());
        break;
    case hir::ExprKind::Call:
        analyse_call(id, expr.call_callee(), expr.call_args());
        break;
    case hir::ExprKind::Constant:
        analyse_constant(id, expr.constant_value());
        break;
    case hir::ExprKind::Match:
        analyse_match(id, expr.match_matchee(), expr.match_arms(), expr.match_arm_count());
        break;
    case hir::ExprKind::Var:
        analyse_var(id);
        break;
    }
}

void Constrainer::visit(const hir::DeclStmt &decl_stmt) {
    analyse_expr(decl_stmt.value());
    m_constraints[decl_stmt.value()].emplace(ConstraintKind::ImplicitlyCastable, decl_stmt.var());
}

void Constrainer::visit(const hir::Function &function) {
    m_function = &function;
    for (hir::ExprId param : function.params()) {
        m_constraints[param].emplace(m_root.type(param));
    }
    analyse_expr(function.block());
}

void Constrainer::visit(const hir::ReturnStmt &return_stmt) {
    analyse_expr(return_stmt.value());
    m_constraints[return_stmt.value()].emplace(ConstraintKind::ImplicitlyCastable, m_function->block());
}

void Unifier::analyse_binary(hir::ExprId lhs_id, hir::ExprId rhs_id) {
    analyse_expr(lhs_id);
    analyse_expr(rhs_id);
}

void Unifier::analyse_block(const coel::List<hir::Stmt> &stmts) {
    for (const auto *stmt : stmts) {
        stmt->accept(this);
    }
}

void Unifier::analyse_call(const hir::Function *callee, const hir::ExprId *arg_ids) {
    for (std::size_t i = 0; i < callee->params().size(); i++) {
        analyse_expr(arg_ids[i]);
    }
}

void Unifier::analyse_match(hir::ExprId matchee_id, const std::pair<hir::ExprId, hir::ExprId> *arms,
                            std::size_t arm_count) {
    analyse_expr(matchee_id);
    for (std::size_t i = 0; i < arm_count; i++) {
        analyse_expr(arms[i].first);
        analyse_expr(arms[i].second);
    }
}

void Unifier::analyse_expr(hir::ExprId id) {
    auto &expr = m_root.expr(id);
    if (expr.kind() == hir::ExprKind::Block) {
        COEL_ASSERT(m_constraints[id].empty());
        analyse_block(expr.block_stmts());
        return;
    }
    if (expr.kind() == hir::ExprKind::Var && m_constraints[id].empty()) {
        return;
    }

    std::vector<Constraint> visited_constraints;
    do {
        auto c1 = m_constraints[id].pop();
        switch (c1.kind()) {
        case ConstraintKind::Equals: {
            expr.set_type(c1.equals_type());
            for (const auto &c2 : visited_constraints) {
                switch (c2.kind()) {
                case ConstraintKind::ImplicitlyCastable: {
                    const auto &cast_to = m_root.type(c2.implicitly_castable_expr());
                    if (expr.type() != cast_to) {
                        Diagnostic diagnostic(expr.location(), "cannot implicitly cast from {} to {}",
                                              type_string(expr.type()), type_string(cast_to));
                        auto &constraining_expr = m_root.expr(c2.implicitly_castable_expr());
                        diagnostic.add_note(constraining_expr.location(), "constrained here");
                    }
                    break;
                }
                default:
                    COEL_ENSURE_NOT_REACHED();
                }
            }
            break;
        }
        case ConstraintKind::ImplicitlyCastable: {
            // TODO: Check visited_constraints.
            if (!expr.type().is_real()) {
                expr.set_type(m_root.type(c1.implicitly_castable_expr()));
            }
            break;
        }
        case ConstraintKind::IntegerWidth:
            expr.set_type(coel::ir::IntegerType::get(c1.integer_width_bit_width()));
            for (const auto &c2 : visited_constraints) {
                switch (c2.kind()) {
                case ConstraintKind::ImplicitlyCastable: {
                    const auto &cast_to_hir = m_root.type(c2.implicitly_castable_expr());
                    if (cast_to_hir.is_infer()) {
                        break;
                    }
                    const auto *cast_to = cast_to_hir.real()->as<coel::ir::IntegerType>();
                    if (cast_to == nullptr) {
                        COEL_ENSURE_NOT_REACHED("Integer type needs to be castable to a non-integer");
                    }
                    if (cast_to->bit_width() < c1.integer_width_bit_width()) {
                        Diagnostic diagnostic(expr.location(), "implicit truncation from {} to {} is not allowed",
                                              expr.kind() == hir::ExprKind::Constant
                                                  ? fmt::format("the literal '{}' (u{})", expr.constant_value(),
                                                                c1.integer_width_bit_width())
                                                  : fmt::format("a u{}", c1.integer_width_bit_width()),
                                              type_string(cast_to));
                        auto &constraining_expr = m_root.expr(c2.implicitly_castable_expr());
                        if (constraining_expr.kind() == hir::ExprKind::Argument) {
                            diagnostic.add_note(constraining_expr.location(), "parameter declared as {} here",
                                                type_string(cast_to));
                        }
                    }
                    expr.set_type(cast_to);
                    break;
                }
                default:
                    COEL_ENSURE_NOT_REACHED();
                }
            }
            break;
        default:
            COEL_ENSURE_NOT_REACHED();
        }
        visited_constraints.push_back(c1);
    } while (!m_constraints[id].empty());

    switch (expr.kind()) {
    case hir::ExprKind::Add:
    case hir::ExprKind::Sub:
        analyse_binary(expr.binary_lhs(), expr.binary_rhs());
        break;
    case hir::ExprKind::Call:
        analyse_call(expr.call_callee(), expr.call_args());
        break;
    case hir::ExprKind::Match:
        analyse_match(expr.match_matchee(), expr.match_arms(), expr.match_arm_count());
        break;
    default:
        break;
    }
}

void Unifier::visit(const hir::DeclStmt &decl_stmt) {
    analyse_expr(decl_stmt.var());
    analyse_expr(decl_stmt.value());
    auto &var = m_root.expr(decl_stmt.var());
    if (var.type().is_infer()) {
        var.set_type(m_root.type(decl_stmt.value()));
    }
}

void Unifier::visit(const hir::Function &function) {
    analyse_expr(function.block());
}

void Unifier::visit(const hir::ReturnStmt &return_stmt) {
    analyse_expr(return_stmt.value());
}

} // namespace

void analyse_hir(hir::Root &root) {
    Constrainer constrainer(root);
    for (auto *function : root) {
        function->accept(&constrainer);
    }
    Unifier unifier(root, constrainer.constraints());
    for (auto *function : root) {
        function->accept(&unifier);
    }
}
