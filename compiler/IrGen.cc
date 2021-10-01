#include <IrGen.hh>

#include <Ast.hh>

#include <codegen/ir/Constant.hh>
#include <codegen/support/Stack.hh>

#include <string_view>
#include <unordered_map>

namespace {

class IrGenerator final : public ast::Visitor {
    ir::Unit m_unit;
    ir::Function *m_function{nullptr};
    ir::BasicBlock *m_block{nullptr};
    Stack<ir::Value *> m_expr_stack;

    // TODO: Scope class.
    std::unordered_map<std::string_view, ir::Value *> m_value_map;

public:
    void visit(const ast::BinaryExpr &binary_expr) override;
    void visit(const ast::Block &block) override;
    void visit(const ast::CallExpr &call_expr) override;
    void visit(const ast::DeclStmt &decl_stmt) override;
    void visit(const ast::FunctionDecl &function_decl) override;
    void visit(const ast::IntegerLiteral &integer_literal) override;
    void visit(const ast::ReturnStmt &return_stmt) override;
    void visit(const ast::Root &root) override;
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
    auto *rhs = m_expr_stack.pop();
    auto *lhs = m_expr_stack.pop();
    m_expr_stack.push(m_block->append<ir::BinaryInst>(binary_op(binary_expr.op()), lhs, rhs));
}

void IrGenerator::visit(const ast::Block &block) {
    m_block = m_function->append_block();
    for (const auto *stmt : block) {
        stmt->accept(this);
    }
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
    auto *call = m_block->append<ir::CallInst>(m_value_map.at(call_expr.callee().name()), std::move(args));
    m_expr_stack.push(call);
}

void IrGenerator::visit(const ast::DeclStmt &decl_stmt) {
    ASSERT(!m_value_map.contains(decl_stmt.name()));
    auto *stack_slot = m_function->append_stack_slot();
    decl_stmt.value().accept(this);
    m_block->append<ir::StoreInst>(stack_slot, m_expr_stack.pop());
    m_value_map.emplace(decl_stmt.name(), stack_slot);
}

void IrGenerator::visit(const ast::FunctionDecl &function_decl) {
    m_function = m_unit.append_function(function_decl.name(), function_decl.args().size());
    m_value_map.emplace(function_decl.name(), m_function);
    for (std::size_t i = 0; const auto &arg : function_decl.args()) {
        m_value_map.emplace(arg, m_function->argument(i++));
    }
    function_decl.block().accept(this);
}

void IrGenerator::visit(const ast::IntegerLiteral &integer_literal) {
    m_expr_stack.push(ir::Constant::get(integer_literal.value()));
}

void IrGenerator::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.value().accept(this);
    m_block->append<ir::RetInst>(m_expr_stack.pop());
}

void IrGenerator::visit(const ast::Root &root) {
    for (const auto *function : root) {
        function->accept(this);
    }
}

void IrGenerator::visit(const ast::Symbol &symbol) {
    auto *var = m_value_map.at(symbol.name());
    if (var->is<ir::StackSlot>()) {
        var = m_block->append<ir::LoadInst>(var);
    }
    m_expr_stack.push(var);
}

} // namespace

ir::Unit generate_ir(const ast::Root &root) {
    IrGenerator generator;
    root.accept(&generator);
    return std::move(generator.unit());
}
