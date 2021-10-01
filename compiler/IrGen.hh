#pragma once

#include <codegen/ir/Unit.hh>

namespace ast {

class Root;

} // namespace ast

ir::Unit generate_ir(const ast::Root &root);
