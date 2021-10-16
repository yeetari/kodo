#pragma once

#include <coel/ir/Unit.hh>

namespace hir {

class Root;

} // namespace hir

coel::ir::Unit lower_hir(const hir::Root &root);
