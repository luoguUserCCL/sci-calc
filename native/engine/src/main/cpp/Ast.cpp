// scicalc/Ast.cpp — Expr::clone deep copy.
#include "scicalc/Ast.hpp"

namespace scicalc {

ExprPtr Expr::clone() const {
    auto e = std::make_unique<Expr>();
    e->kind = kind;
    e->num = num;
    e->name = name;
    e->unop = unop;
    e->binop = binop;
    if (lhs) e->lhs = lhs->clone();
    if (rhs) e->rhs = rhs->clone();
    e->args.reserve(args.size());
    for (auto& a : args) e->args.push_back(a ? a->clone() : nullptr);
    e->params = params;
    e->intKind = intKind;
    if (lo) e->lo = lo->clone();
    if (hi) e->hi = hi->clone();
    return e;
}

} // namespace scicalc
