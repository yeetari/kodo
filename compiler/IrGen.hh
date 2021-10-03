#pragma once

#include <coel/ir/Unit.hh>

namespace ast {

class Root;

} // namespace ast

coel::ir::Unit generate_ir(const ast::Root &root);
