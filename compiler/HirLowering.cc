#include <HirLowering.hh>

#include <Hir.hh>

#include <coel/ir/Constant.hh>

#include <unordered_map>
#include <vector>

namespace {

class HirLowering final : public hir::Visitor {
    const hir::Root &m_root;
    coel::ir::Unit m_unit;
    coel::ir::Function *m_function{nullptr};
    coel::ir::BasicBlock *m_block{nullptr};
    std::unordered_map<const hir::Function *, coel::ir::Function *> m_function_map;
    std::unordered_map<hir::ExprId, coel::ir::Value *> m_vars;

    coel::ir::Value *lower_argument(std::size_t index);
    coel::ir::Value *lower_binary(hir::ExprKind op, hir::ExprId lhs_id, hir::ExprId rhs_id);
    coel::ir::Value *lower_block(const coel::List<hir::Stmt> &stmts);
    coel::ir::Value *lower_call(const hir::Function *callee, const hir::ExprId *arg_ids);
    coel::ir::Value *lower_constant(const hir::Type &type, std::size_t value);
    coel::ir::Value *lower_match(const hir::Type &type, hir::ExprId matchee_id,
                                 const std::pair<hir::ExprId, hir::ExprId> *arms, std::size_t arm_count);
    coel::ir::Value *lower_var(hir::ExprId id);
    coel::ir::Value *lower_expr(hir::ExprId id);

public:
    explicit HirLowering(const hir::Root &root) : m_root(root) {}

    void visit(const hir::DeclStmt &decl_stmt) override;
    void visit(const hir::Function &function) override;
    void visit(const hir::ReturnStmt &return_stmt) override;

    coel::ir::Unit &unit() { return m_unit; }
};

coel::ir::Value *HirLowering::lower_argument(std::size_t index) {
    return m_function->argument(index);
}

coel::ir::Value *HirLowering::lower_binary(hir::ExprKind op, hir::ExprId lhs_id, hir::ExprId rhs_id) {
    auto ir_op = [op] {
        switch (op) {
        case hir::ExprKind::Add:
            return coel::ir::BinaryOp::Add;
        case hir::ExprKind::Sub:
            return coel::ir::BinaryOp::Sub;
        default:
            COEL_ENSURE_NOT_REACHED();
        }
    }();
    auto *lhs = lower_expr(lhs_id);
    auto *rhs = lower_expr(rhs_id);
    return m_block->append<coel::ir::BinaryInst>(ir_op, lhs, rhs);
}

coel::ir::Value *HirLowering::lower_block(const coel::List<hir::Stmt> &stmts) {
    for (const auto *stmt : stmts) {
        stmt->accept(this);
    }
    return nullptr;
}

coel::ir::Value *HirLowering::lower_call(const hir::Function *callee, const hir::ExprId *arg_ids) {
    std::vector<coel::ir::Value *> args;
    for (std::size_t i = 0; i < callee->params().size(); i++) {
        args.push_back(lower_expr(arg_ids[i]));
    }
    return m_block->append<coel::ir::CallInst>(m_function_map.at(callee), std::move(args));
}

coel::ir::Value *HirLowering::lower_constant(const hir::Type &type, std::size_t value) {
    return coel::ir::Constant::get(type.real(), value);
}

coel::ir::Value *HirLowering::lower_match(const hir::Type &type, hir::ExprId matchee_id,
                                          const std::pair<hir::ExprId, hir::ExprId> *arms, std::size_t arm_count) {
    auto *matchee = lower_expr(matchee_id);
    auto *result_var = m_function->append_stack_slot(type.real());
    std::vector<coel::ir::BasicBlock *> blocks;
    for (std::size_t i = 0; i < arm_count; i++) {
        auto [lhs_id, rhs_id] = arms[i];
        auto *lhs = lower_expr(lhs_id);
        auto *compare = m_block->append<coel::ir::CompareInst>(coel::ir::CompareOp::Eq, matchee, lhs);
        auto *true_dst = m_function->append_block();
        auto *false_dst = m_function->append_block();
        m_block->append<coel::ir::CondBranchInst>(compare, true_dst, false_dst);
        blocks.push_back(true_dst);
        blocks.push_back(false_dst);
        m_block = true_dst;
        auto *rhs = lower_expr(rhs_id);
        m_block->append<coel::ir::StoreInst>(result_var, rhs);
        if (!m_block->has_terminator()) {
            blocks.push_back(m_block);
        }
        m_block = false_dst;
    }
    m_block = m_function->append_block();
    for (auto *block : blocks) {
        if (!block->has_terminator()) {
            block->append<coel::ir::BranchInst>(m_block);
        }
    }
    return m_block->append<coel::ir::LoadInst>(result_var);
}

coel::ir::Value *HirLowering::lower_var(hir::ExprId id) {
    return m_block->append<coel::ir::LoadInst>(m_vars.at(id));
}

coel::ir::Value *HirLowering::lower_expr(hir::ExprId id) {
    const auto &expr = m_root.expr(id);
    switch (expr.kind()) {
    case hir::ExprKind::Argument:
        return lower_argument(expr.argument_index());
    case hir::ExprKind::Add:
    case hir::ExprKind::Sub:
        return lower_binary(expr.kind(), expr.binary_lhs(), expr.binary_rhs());
    case hir::ExprKind::Block:
        return lower_block(expr.block_stmts());
    case hir::ExprKind::Call:
        return lower_call(expr.call_callee(), expr.call_args());
    case hir::ExprKind::Constant:
        return lower_constant(expr.type(), expr.constant_value());
    case hir::ExprKind::Match:
        return lower_match(expr.type(), expr.match_matchee(), expr.match_arms(), expr.match_arm_count());
    case hir::ExprKind::Var:
        return lower_var(id);
    }
}

void HirLowering::visit(const hir::DeclStmt &decl_stmt) {
    auto *stack_slot = m_function->append_stack_slot(m_root.type(decl_stmt.var()).real());
    auto *value = lower_expr(decl_stmt.value());
    m_block->append<coel::ir::StoreInst>(stack_slot, value);
    m_vars.emplace(decl_stmt.var(), stack_slot);
}

void HirLowering::visit(const hir::Function &function) {
    std::vector<const coel::ir::Type *> parameters(function.params().size());
    std::transform(function.params().begin(), function.params().end(), parameters.begin(), [this](hir::ExprId id) {
        return m_root.type(id).real();
    });
    m_function = m_unit.append_function(function.name(), m_root.type(function.block()).real(), parameters);
    m_block = m_function->append_block();
    m_function_map.emplace(&function, m_function);
    lower_expr(function.block());
}

void HirLowering::visit(const hir::ReturnStmt &return_stmt) {
    auto *value = lower_expr(return_stmt.value());
    m_block->append<coel::ir::RetInst>(value);
}

} // namespace

coel::ir::Unit lower_hir(const hir::Root &root) {
    HirLowering lowering(root);
    for (const auto *function : root) {
        function->accept(&lowering);
    }
    return std::move(lowering.unit());
}
