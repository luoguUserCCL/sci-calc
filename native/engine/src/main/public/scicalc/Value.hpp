// scicalc/Value.hpp — Runtime value types for the evaluator.
#pragma once
#include "scicalc/BigRational.hpp"
#include "scicalc/BigFloat.hpp"
#include "scicalc/Ast.hpp"
#include <memory>
#include <string>
#include <vector>

namespace scicalc {

struct SetValue;
using SetValuePtr = std::shared_ptr<SetValue>;

/// A mathematical set value. Supports named sets (R/Q/Z), finite enumerated
/// sets, intervals, and compound union/intersection/difference.
struct SetValue {
    enum Kind { Named, Finite, Interval, Union, Intersect, Diff, Empty } kind;
    std::string name;                 // Named: "Real" | "Rational" | "Integer"
    std::vector<BigRational> elems;   // Finite (sorted, unique)
    BigRational lo, hi;               // Interval bounds
    bool loClosed = true, hiClosed = true;
    SetValuePtr a, b;                 // Union/Intersect/Diff operands

    static SetValuePtr makeNamed(const std::string& n);
    static SetValuePtr makeFinite(std::vector<BigRational> e);
    static SetValuePtr makeInterval(const BigRational& lo, const BigRational& hi,
                                    bool loC, bool hiC);
    static SetValuePtr makeEmpty();
    static SetValuePtr makeUnion(SetValuePtr a, SetValuePtr b);
    static SetValuePtr makeIntersect(SetValuePtr a, SetValuePtr b);
    static SetValuePtr makeDiff(SetValuePtr a, SetValuePtr b);

    bool contains(const BigRational& x) const;
    /// Is *this a subset of *o?
    bool isSubsetOf(const SetValue& o) const;
    /// Render to a textual form (LaTeX-ish semantics, no actual LaTeX).
    std::string toString() const;
    bool isEqual(const SetValue& o) const;
};

/// A runtime value.
struct Value {
    enum Type { Rational, Decimal, Boolean, Set, Symbolic, Error } type = Rational;
    BigRational rat;
    BigFloat dec;
    bool boolean = false;
    SetValuePtr set;
    std::shared_ptr<Expr> sym;   // unevaluated symbolic form (shared; Value is copyable)
    std::string err;

    static Value ofRat(const BigRational& r) { Value v; v.type = Rational; v.rat = r; return v; }
    static Value ofDec(const BigFloat& d) { Value v; v.type = Decimal; v.dec = d; return v; }
    static Value ofBool(bool b) { Value v; v.type = Boolean; v.boolean = b; return v; }
    static Value ofSet(SetValuePtr s) { Value v; v.type = Set; v.set = std::move(s); return v; }
    static Value ofSym(ExprPtr e) { Value v; v.type = Symbolic; v.sym = std::move(e); return v; }
    static Value ofErr(const std::string& m) { Value v; v.type = Error; v.err = m; return v; }

    bool isRational() const { return type == Rational; }
    bool isDecimal() const { return type == Decimal; }
    bool isBoolean() const { return type == Boolean; }
    bool isSet() const { return type == Set; }
    bool isSymbolic() const { return type == Symbolic; }
    bool isNumeric() const { return type == Rational || type == Decimal; }
    bool isError() const { return type == Error; }

    /// Convert to BigRational (Decimal is approximated; symbolic -> error).
    BigRational toRational() const;
    /// Convert to BigFloat at given precision (Rational is exact-converted).
    BigFloat toBigFloat(unsigned prec) const;
};

} // namespace scicalc
