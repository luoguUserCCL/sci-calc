// scicalc/Ast.hpp — Abstract syntax tree for the sci-calc expression language.
#pragma once
#include "scicalc/BigRational.hpp"
#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace scicalc {

enum class BinOp {
    Add, Sub, Mul, Div, Mod, Pow,
    SetIntersect,   // cap (intersection)
    SetUnion,       // cup (union)
    SetDiff,        // backslash (set difference)
    Eq, Neq, Lt, Gt, Le, Ge,
    In,             // in
    Subset,         // subset (subseteq)
    RealSubset,     // realsubset (subsetneq)
    And, Or,
    Assign,         // :=
};

enum class UnaryOp { Pos, Neg, Not };

enum class SetIntervalKind {
    ClosedClosed,   // [a,b]
    OpenOpen,       // (a,b)
    ClosedOpen,     // [a,b)
    OpenClosed,     // (a,b]
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
    enum Kind {
        Number, Var, Unary, Binary, Call, AssignVar, AssignFunc,
        SetEnum,            // {a, b, c}
        Interval,           // [a,b] / (a,b] / ...
        Iverson,            // Iverson(P)
        SetName,            // Real / Rational / Integer predefined sets
    } kind;

    // Number
    BigRational num{};

    // Var / SetName
    std::string name;

    // Unary
    UnaryOp unop{UnaryOp::Neg};

    // Binary
    BinOp binop{BinOp::Add};
    ExprPtr lhs, rhs;

    // Call
    std::vector<ExprPtr> args;

    // AssignFunc: param names
    std::vector<std::string> params;

    // Interval
    SetIntervalKind intKind{SetIntervalKind::ClosedClosed};
    ExprPtr lo, hi;

    static ExprPtr makeNumber(const BigRational& r) {
        auto e = std::make_unique<Expr>(); e->kind = Number; e->num = r; return e;
    }
    static ExprPtr makeVar(const std::string& n) {
        auto e = std::make_unique<Expr>(); e->kind = Var; e->name = n; return e;
    }
    static ExprPtr makeUnary(UnaryOp op, ExprPtr a) {
        auto e = std::make_unique<Expr>(); e->kind = Unary; e->unop = op; e->lhs = std::move(a); return e;
    }
    static ExprPtr makeBinary(BinOp op, ExprPtr a, ExprPtr b) {
        auto e = std::make_unique<Expr>(); e->kind = Binary; e->binop = op;
        e->lhs = std::move(a); e->rhs = std::move(b); return e;
    }
    static ExprPtr makeCall(const std::string& n, std::vector<ExprPtr> a) {
        auto e = std::make_unique<Expr>(); e->kind = Call; e->name = n; e->args = std::move(a); return e;
    }
    static ExprPtr makeCall(const std::string& n, ExprPtr a) {
        std::vector<ExprPtr> v; v.push_back(std::move(a));
        auto e = std::make_unique<Expr>(); e->kind = Call; e->name = n; e->args = std::move(v); return e;
    }
    static ExprPtr makeCall(const std::string& n, ExprPtr a, ExprPtr b) {
        std::vector<ExprPtr> v; v.push_back(std::move(a)); v.push_back(std::move(b));
        auto e = std::make_unique<Expr>(); e->kind = Call; e->name = n; e->args = std::move(v); return e;
    }
    static ExprPtr makeAssignVar(const std::string& n, ExprPtr v) {
        auto e = std::make_unique<Expr>(); e->kind = AssignVar; e->name = n; e->lhs = std::move(v); return e;
    }
    static ExprPtr makeAssignFunc(const std::string& n, std::vector<std::string> p, ExprPtr body) {
        auto e = std::make_unique<Expr>(); e->kind = AssignFunc; e->name = n; e->params = std::move(p); e->lhs = std::move(body); return e;
    }
    static ExprPtr makeSetEnum(std::vector<ExprPtr> elems) {
        auto e = std::make_unique<Expr>(); e->kind = SetEnum; e->args = std::move(elems); return e;
    }
    static ExprPtr makeInterval(SetIntervalKind k, ExprPtr lo, ExprPtr hi) {
        auto e = std::make_unique<Expr>(); e->kind = Interval; e->intKind = k; e->lo = std::move(lo); e->hi = std::move(hi); return e;
    }
    static ExprPtr makeIverson(ExprPtr cond) {
        auto e = std::make_unique<Expr>(); e->kind = Iverson; e->lhs = std::move(cond); return e;
    }
    static ExprPtr makeSetName(const std::string& n) {
        auto e = std::make_unique<Expr>(); e->kind = SetName; e->name = n; return e;
    }

    /// Deep copy (Expr is non-copyable due to unique_ptr children).
    ExprPtr clone() const;
};

} // namespace scicalc
