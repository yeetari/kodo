#include <AstLowering.hh>

#include <Ast.hh>
#include <Hir.hh>

#include <coel/ir/Types.hh>
#include <coel/support/Stack.hh>
#include <fmt/core.h>

#include <optional>
#include <string_view>
#include <unordered_map>

namespace {

enum class ScopeKind {
    Block,
    Function,
    Root,
};

class Scope {
    Scope *&m_current;
    Scope *const m_parent;
    const ScopeKind m_kind;
    std::unordered_map<std::string_view, hir::ExprId> m_symbol_map;

public:
    Scope(Scope *&current, ScopeKind kind) : m_current(current), m_parent(current), m_kind(kind) { current = this; }
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope() { m_current = m_parent; }

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    std::optional<hir::ExprId> find_symbol(std::string_view name) const;
    hir::ExprId lookup_symbol(std::string_view name) const;
    void put_symbol(std::string_view name, hir::ExprId id);

    const Scope *parent() const { return m_parent; }
    ScopeKind kind() const { return m_kind; }
};

class AstLowering final : public ast::Visitor {
    hir::Root m_root;
    hir::Function *m_function{nullptr};
    hir::ExprId m_block{0};
    coel::Stack<hir::ExprId> m_expr_stack;
    std::unordered_map<std::string_view, hir::Function *> m_function_map;
    Scope *m_scope{nullptr};

    hir::Type lower_type(const ast::Type &type);

public:
    void visit(const ast::BinaryExpr &binary_expr) override;
    void visit(const ast::Block &block) override;
    void visit(const ast::CallExpr &call_expr) override;
    void visit(const ast::DeclStmt &decl_stmt) override;
    void visit(const ast::FunctionDecl &function_decl) override;
    void visit(const ast::IntegerLiteral &integer_literal) override;
    void visit(const ast::MatchExpr &match_expr) override;
    void visit(const ast::ReturnStmt &return_stmt) override;
    void visit(const ast::Root &root) override;
    void visit(const ast::Symbol &symbol) override;
    void visit(const ast::YieldStmt &yield_stmt) override;

    hir::Root &root() { return m_root; }
};

std::optional<hir::ExprId> Scope::find_symbol(std::string_view name) const {
    for (const auto *scope = this; scope != nullptr; scope = scope->m_parent) {
        if (scope->m_symbol_map.contains(name)) {
            return scope->m_symbol_map.at(name);
        }
    }
    return std::nullopt;
}

hir::ExprId Scope::lookup_symbol(std::string_view name) const {
    if (auto symbol = find_symbol(name)) {
        return *symbol;
    }
    fmt::print("error: use of undeclared symbol {}\n", name);
    std::exit(1);
}

void Scope::put_symbol(std::string_view name, hir::ExprId id) {
    if (find_symbol(name)) {
        fmt::print("error: attempted redeclaration of symbol {}\n", name);
        std::exit(1);
    }
    m_symbol_map.emplace(name, id);
}

hir::Type AstLowering::lower_type(const ast::Type &type) {
    if (const auto *base_type = type.as<ast::BaseType>()) {
        const auto &name = base_type->name();
        if (name.starts_with('u')) {
            // TODO: substr is making a copy here, need stoi that works with string_view.
            return coel::ir::IntegerType::get(std::stoi(name.substr(1)));
        }
    }
    COEL_ENSURE_NOT_REACHED();
}

void AstLowering::visit(const ast::BinaryExpr &binary_expr) {
    auto binary_op = [](ast::BinaryOp op) {
        switch (op) {
        case ast::BinaryOp::Add:
            return hir::ExprKind::Add;
        case ast::BinaryOp::Sub:
            return hir::ExprKind::Sub;
        }
    };
    binary_expr.lhs().accept(this);
    binary_expr.rhs().accept(this);
    auto rhs = m_expr_stack.pop();
    auto lhs = m_expr_stack.pop();
    m_expr_stack.push(m_root.create_expr(binary_op(binary_expr.op()), lhs, rhs));
}

void AstLowering::visit(const ast::Block &block) {
    Scope scope(m_scope, ScopeKind::Block);
    for (const auto *stmt : block) {
        stmt->accept(this);
    }
}

void AstLowering::visit(const ast::CallExpr &call_expr) {
    auto *args = new hir::ExprId[call_expr.args().size()];
    for (std::size_t i = 0; const auto *arg : call_expr.args()) {
        arg->accept(this);
        args[i++] = m_expr_stack.pop();
    }
    auto *callee = m_function_map.at(call_expr.callee().name());
    m_expr_stack.push(m_root.create_expr(callee, m_root.type(callee->block()), args));
}

void AstLowering::visit(const ast::DeclStmt &decl_stmt) {
    decl_stmt.value().accept(this);
    COEL_ASSERT(!m_scope->find_symbol(decl_stmt.name()));
    COEL_ASSERT(m_expr_stack.size() == 1);
    auto var = m_root.create_expr(hir::ExprKind::Var, hir::TypeKind::Infer);
    m_root.expr(m_block).append<hir::DeclStmt>(var, m_expr_stack.pop());
    m_scope->put_symbol(decl_stmt.name(), var);
}

void AstLowering::visit(const ast::FunctionDecl &function_decl) {
    Scope scope(m_scope, ScopeKind::Function);
    std::vector<hir::ExprId> params;
    for (std::size_t i = 0; const auto &arg : function_decl.args()) {
        auto argument = m_root.create_expr(hir::ExprKind::Argument, lower_type(arg.type()), i++);
        m_scope->put_symbol(arg.name(), argument);
        params.push_back(argument);
    }
    m_function = m_root.append_function(function_decl.name(), std::move(params));
    m_block = m_root.create_expr(hir::ExprKind::Block, lower_type(function_decl.return_type()));
    m_function->set_block(m_block);
    m_function_map.emplace(function_decl.name(), m_function);
    function_decl.block().accept(this);
}

void AstLowering::visit(const ast::IntegerLiteral &integer_literal) {
    m_expr_stack.push(m_root.create_expr(hir::ExprKind::Constant, integer_literal.value()));
}

void AstLowering::visit(const ast::MatchExpr &match_expr) {
    match_expr.matchee().accept(this);
    auto matchee = m_expr_stack.pop();
    auto *arms = new std::pair<hir::ExprId, hir::ExprId>[match_expr.arms().size()];
    for (std::size_t i = 0; const auto &arm : match_expr.arms()) {
        arm.lhs().accept(this);
        auto lhs = m_expr_stack.pop();
        arm.rhs().accept(this);
        auto rhs = m_expr_stack.pop();
        arms[i++] = {lhs, rhs};
    }
    m_expr_stack.push(m_root.create_expr(matchee, arms, match_expr.arms().size()));
}

void AstLowering::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.value().accept(this);
    m_root.expr(m_block).append<hir::ReturnStmt>(m_expr_stack.pop());
}

void AstLowering::visit(const ast::Root &root) {
    Scope scope(m_scope, ScopeKind::Root);
    for (const auto *function : root) {
        function->accept(this);
    }
}

void AstLowering::visit(const ast::Symbol &symbol) {
    m_expr_stack.push(m_scope->lookup_symbol(symbol.name()));
}

void AstLowering::visit(const ast::YieldStmt &yield_stmt) {
    yield_stmt.value().accept(this);
    if (m_scope->parent()->kind() == ScopeKind::Function) {
        // Emit a return statement if yielding from a function.
        m_root.expr(m_block).append<hir::ReturnStmt>(m_expr_stack.pop());
    }
}

} // namespace

hir::Root lower_ast(const ast::Root &root) {
    AstLowering lowering;
    root.accept(&lowering);
    return std::move(lowering.root());
}
