#include <IrGen.hh>

#include <Ast.hh>

#include <coel/ir/Constant.hh>
#include <coel/support/Stack.hh>
#include <fmt/core.h>

#include <string_view>
#include <unordered_map>

using namespace coel;

namespace {

class Scope {
    const Scope *const m_parent;
    std::unordered_map<std::string_view, ir::Value *> m_symbol_map;

public:
    explicit Scope(const Scope *parent) : m_parent(parent) {}

    ir::Value *find_symbol(std::string_view name) const;
    ir::Value *lookup_symbol(std::string_view name) const;
    void put_symbol(std::string_view name, ir::Value *value);
};

class IrGenerator final : public ast::Visitor {
    ir::Unit m_unit;
    ir::Function *m_function{nullptr};
    ir::BasicBlock *m_block{nullptr};
    Stack<ir::Value *> m_expr_stack;
    Stack<Scope> m_scope_stack;

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

    ir::Unit &unit() { return m_unit; }
};

ir::Value *Scope::find_symbol(std::string_view name) const {
    for (const auto *scope = this; scope != nullptr; scope = scope->m_parent) {
        if (scope->m_symbol_map.contains(name)) {
            return scope->m_symbol_map.at(name);
        }
    }
    return nullptr;
}

ir::Value *Scope::lookup_symbol(std::string_view name) const {
    if (auto *symbol = find_symbol(name)) {
        return symbol;
    }
    fmt::print("error: use of undeclared symbol {}\n", name);
    std::exit(1);
}

void Scope::put_symbol(std::string_view name, ir::Value *value) {
    if (find_symbol(name) != nullptr) {
        fmt::print("error: attempted redeclaration of symbol {}\n", name);
        std::exit(1);
    }
    m_symbol_map.emplace(name, value);
}

void IrGenerator::visit(const ast::BinaryExpr &binary_expr) {
    binary_expr.lhs().accept(this);
    binary_expr.rhs().accept(this);
    auto binary_op = [](ast::BinaryOp op) {
        switch (op) {
        case ast::BinaryOp::Add:
            return ir::BinaryOp::Add;
        case ast::BinaryOp::Sub:
            return ir::BinaryOp::Sub;
        }
    };
    auto *rhs = m_expr_stack.pop();
    auto *lhs = m_expr_stack.pop();
    m_expr_stack.push(m_block->append<ir::BinaryInst>(binary_op(binary_expr.op()), lhs, rhs));
}

void IrGenerator::visit(const ast::Block &block) {
    m_scope_stack.emplace(m_scope_stack.peek());
    for (const auto *stmt : block) {
        stmt->accept(this);
    }
    m_scope_stack.pop();
}

void IrGenerator::visit(const ast::CallExpr &call_expr) {
    for (const auto *arg : call_expr.args()) {
        arg->accept(this);
    }
    std::vector<ir::Value *> args;
    args.reserve(call_expr.args().size());
    for (std::size_t i = 0; i < call_expr.args().size(); i++) {
        args.push_back(m_expr_stack.pop());
    }
    std::reverse(args.begin(), args.end());
    auto *callee = m_scope_stack.peek().lookup_symbol(call_expr.callee().name());
    auto *call = m_block->append<ir::CallInst>(callee, std::move(args));
    m_expr_stack.push(call);
}

void IrGenerator::visit(const ast::DeclStmt &decl_stmt) {
    COEL_ASSERT(m_scope_stack.peek().find_symbol(decl_stmt.name()) == nullptr);
    auto *stack_slot = m_function->append_stack_slot();
    decl_stmt.value().accept(this);
    COEL_ASSERT(m_expr_stack.size() == 1);
    m_block->append<ir::StoreInst>(stack_slot, m_expr_stack.pop());
    m_scope_stack.peek().put_symbol(decl_stmt.name(), stack_slot);
}

void IrGenerator::visit(const ast::FunctionDecl &function_decl) {
    m_function = m_unit.append_function(function_decl.name(), function_decl.args().size());
    m_block = m_function->append_block();
    m_scope_stack.peek().put_symbol(function_decl.name(), m_function);
    m_scope_stack.emplace(m_scope_stack.peek());
    for (std::size_t i = 0; const auto &arg : function_decl.args()) {
        m_scope_stack.peek().put_symbol(arg, m_function->argument(i++));
    }
    function_decl.block().accept(this);
    m_scope_stack.pop();
}

void IrGenerator::visit(const ast::IntegerLiteral &integer_literal) {
    m_expr_stack.push(ir::Constant::get(integer_literal.value()));
}

void IrGenerator::visit(const ast::MatchExpr &match_expr) {
    match_expr.matchee().accept(this);
    auto *matchee = m_expr_stack.pop();
    auto *result_var = m_function->append_stack_slot();
    std::vector<ir::BasicBlock *> blocks;
    for (const auto &arm : match_expr.arms()) {
        arm.lhs().accept(this);
        auto *rhs = m_expr_stack.pop();
        auto *compare = m_block->append<ir::CompareInst>(ir::CompareOp::Eq, matchee, rhs);
        auto *true_dst = m_function->append_block();
        auto *false_dst = m_function->append_block();
        m_block->append<ir::CondBranchInst>(compare, true_dst, false_dst);
        blocks.push_back(true_dst);
        blocks.push_back(false_dst);
        m_block = true_dst;
        arm.rhs().accept(this);
        m_block->append<ir::StoreInst>(result_var, m_expr_stack.pop());
        m_block = false_dst;
    }
    m_block = m_function->append_block();
    for (auto *block : blocks) {
        // TODO: Instruction::is_terminator() and BasicBlock::has_terminator()
        if (block->begin() != block->end() &&
            ((--block->end())->is<ir::BranchInst>() || (--block->end())->is<ir::CondBranchInst>() ||
             (--block->end())->is<ir::RetInst>())) {
            continue;
        }
        block->append<ir::BranchInst>(m_block);
    }
    auto *load = m_block->append<ir::LoadInst>(result_var);
    m_expr_stack.push(load);
}

void IrGenerator::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.value().accept(this);
    m_block->append<ir::RetInst>(m_expr_stack.pop());
}

void IrGenerator::visit(const ast::Root &root) {
    m_scope_stack.emplace(nullptr);
    for (const auto *function : root) {
        function->accept(this);
    }
}

void IrGenerator::visit(const ast::Symbol &symbol) {
    auto *var = m_scope_stack.peek().lookup_symbol(symbol.name());
    if (var->is<ir::StackSlot>()) {
        var = m_block->append<ir::LoadInst>(var);
    }
    m_expr_stack.push(var);
}

void IrGenerator::visit(const ast::YieldStmt &yield_stmt) {
    yield_stmt.value().accept(this);
    // TODO: Emit return if yielding from a function.
}

} // namespace

ir::Unit generate_ir(const ast::Root &root) {
    IrGenerator generator;
    root.accept(&generator);
    return std::move(generator.unit());
}
