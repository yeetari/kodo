#pragma once

#include <Hir.hh>

namespace ast {

class Root;

} // namespace ast

hir::Root lower_ast(const ast::Root &root);
