#include <IrGen.hh>

#include <Ast.hh>

#include <codegen/ir/Constant.hh>
#include <codegen/support/Queue.hh>

#include <string_view>
#include <unordered_map>

namespace {

class IrGenerator final : public ast::Visitor {
    ir::Unit m_unit;
    ir::Function *m_function{nullptr};
    ir::BasicBlock *m_block{nullptr};
    Queue<ir::Value *> m_expr_stack;

    // TODO: Scope class.
    std::unordered_map<std::string_view, ir::StackSlot *> m_var_map;

public:
    void visit(const ast::BinaryExpr &binary_expr) override;
    void visit(const ast::Block &block) override;
    void visit(const ast::DeclStmt &decl_stmt) override;
    void visit(const ast::FunctionDecl &function_decl) override;
    void visit(const ast::IntegerLiteral &integer_literal) override;
    void visit(const ast::ReturnStmt &return_stmt) override;
    void visit(const ast::Symbol &symbol) override;

    ir::Unit &unit() { return m_unit; }
};

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
    m_expr_stack.push(
        m_block->append<ir::BinaryInst>(binary_op(binary_expr.op()), m_expr_stack.pop(), m_expr_stack.pop()));
}

void IrGenerator::visit(const ast::Block &block) {
    m_block = m_function->append_block();
    for (const auto *stmt : block) {
        stmt->accept(this);
    }
}

void IrGenerator::visit(const ast::DeclStmt &decl_stmt) {
    ASSERT(!m_var_map.contains(decl_stmt.name()));
    auto *stack_slot = m_function->append_stack_slot();
    decl_stmt.value().accept(this);
    m_block->append<ir::StoreInst>(stack_slot, m_expr_stack.pop());
    m_var_map.emplace(decl_stmt.name(), stack_slot);
}

void IrGenerator::visit(const ast::FunctionDecl &function_decl) {
    m_function = m_unit.append_function(function_decl.name(), 0);
    function_decl.block().accept(this);
}

void IrGenerator::visit(const ast::IntegerLiteral &integer_literal) {
    m_expr_stack.push(ir::Constant::get(integer_literal.value()));
}

void IrGenerator::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.value().accept(this);
    m_block->append<ir::RetInst>(m_expr_stack.pop());
}

void IrGenerator::visit(const ast::Symbol &symbol) {
    auto *load = m_block->append<ir::LoadInst>(m_var_map.at(symbol.name()));
    m_expr_stack.push(load);
}

} // namespace

ir::Unit generate_ir(const ast::FunctionDecl &function_decl) {
    IrGenerator generator;
    function_decl.accept(&generator);
    return std::move(generator.unit());
}
