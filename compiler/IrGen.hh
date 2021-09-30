#pragma once

#include <codegen/ir/Unit.hh>

namespace ast {

class FunctionDecl;

} // namespace ast

ir::Unit generate_ir(const ast::FunctionDecl &function_decl);
