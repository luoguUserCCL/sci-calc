// scicalc/Value.cpp
#include "scicalc/Value.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace scicalc {

SetValuePtr SetValue::makeNamed(const std::string& n) {
    auto s = std::make_shared<SetValue>(); s->kind = Named; s->name = n; return s;
}
SetValuePtr SetValue::makeFinite(std::vector<BigRational> e) {
    auto s = std::make_shared<SetValue>(); s->kind = Finite;
    // sort & unique
    std::sort(e.begin(), e.end(), [](const BigRational& a, const BigRational& b){ return a.cmp(b) < 0; });
    e.erase(std::unique(e.begin(), e.end(), [](const BigRational& a, const BigRational& b){ return a.cmp(b)==0; }), e.end());
    s->elems = std::move(e); return s;
}
SetValuePtr SetValue::makeInterval(const BigRational& lo, const BigRational& hi, bool loC, bool hiC) {
    auto s = std::make_shared<SetValue>(); s->kind = Interval;
    s->lo = lo; s->hi = hi; s->loClosed = loC; s->hiClosed = hiC; return s;
}
SetValuePtr SetValue::makeEmpty() { auto s = std::make_shared<SetValue>(); s->kind = Empty; return s; }
SetValuePtr SetValue::makeUnion(SetValuePtr a, SetValuePtr b) {
    auto s = std::make_shared<SetValue>(); s->kind = Union; s->a = std::move(a); s->b = std::move(b); return s;
}
SetValuePtr SetValue::makeIntersect(SetValuePtr a, SetValuePtr b) {
    auto s = std::make_shared<SetValue>(); s->kind = Intersect; s->a = std::move(a); s->b = std::move(b); return s;
}
SetValuePtr SetValue::makeDiff(SetValuePtr a, SetValuePtr b) {
    auto s = std::make_shared<SetValue>(); s->kind = Diff; s->a = std::move(a); s->b = std::move(b); return s;
}

static bool namedContains(const std::string& name, const BigRational& x) {
    if (name == "Real") {
        return true; // every rational is real
    }
    if (name == "Rational") {
        return true; // all our numeric values are rationals
    }
    if (name == "Integer") {
        return x.isInteger();
    }
    return false;
}

static bool intervalContains(const SetValue& s, const BigRational& x) {
    int cl = x.cmp(s.lo);
    int ch = x.cmp(s.hi);
    bool okLo = s.loClosed ? (cl >= 0) : (cl > 0);
    bool okHi = s.hiClosed ? (ch <= 0) : (ch < 0);
    return okLo && okHi;
}

bool SetValue::contains(const BigRational& x) const {
    switch (kind) {
        case Empty: return false;
        case Named: return namedContains(name, x);
        case Finite: {
            for (const auto& e : elems) if (e.cmp(x) == 0) return true;
            return false;
        }
        case Interval: return intervalContains(*this, x);
        case Union: return a->contains(x) || b->contains(x);
        case Intersect: return a->contains(x) && b->contains(x);
        case Diff: return a->contains(x) && !b->contains(x);
    }
    return false;
}

bool SetValue::isSubsetOf(const SetValue& o) const {
    // Finite subset of anything: check each element.
    if (kind == Finite) {
        for (const auto& e : elems) if (!o.contains(e)) return false;
        return true;
    }
    if (kind == Empty) return true;
    // Interval subset of named set: depends on named set
    if (kind == Interval && o.kind == Named) {
        if (o.name == "Real" || o.name == "Rational") return true;
        if (o.name == "Integer") {
            // interval subset of Z only if both endpoints integer and ... it's still an interval of reals
            return false; // an interval is a continuum, not subset of Z unless degenerate
        }
    }
    // Interval subset of Interval
    if (kind == Interval && o.kind == Interval) {
        int cl = lo.cmp(o.lo), ch = hi.cmp(o.hi);
        bool okLo = o.loClosed ? (cl >= 0) : (cl > 0);
        bool okHi = o.hiClosed ? (ch <= 0) : (ch < 0);
        // and endpoint strictness
        if (cl == 0 && o.loClosed && !loClosed) return false;
        if (ch == 0 && o.hiClosed && !hiClosed) return false;
        return okLo && okHi;
    }
    if (kind == Interval && o.kind == Finite) return false; // continuum not subset of finite
    // Named subset of Named
    if (kind == Named && o.kind == Named) {
        if (name == o.name) return true;
        if (name == "Integer" && (o.name == "Rational" || o.name == "Real")) return true;
        if (name == "Rational" && o.name == "Real") return true;
        return false;
    }
    // Named subset of interval: no (infinite not subset of finite)
    if (kind == Named) return false;
    // Compound
    if (kind == Union) return a->isSubsetOf(o) && b->isSubsetOf(o);
    if (kind == Intersect) {
        // A∩B subset O if A subset O or B subset O (sufficient, not necessary)
        return a->isSubsetOf(o) || b->isSubsetOf(o);
    }
    if (kind == Diff) {
        // A\B subset O if A subset O
        return a->isSubsetOf(o);
    }
    return false;
}

bool SetValue::isEqual(const SetValue& o) const {
    // Two sets equal iff each subset of the other (works for our finite/interval/named cases)
    return isSubsetOf(o) && o.isSubsetOf(*this);
}

std::string SetValue::toString() const {
    std::ostringstream os;
    switch (kind) {
        case Empty: os << "∅"; break;
        case Named:
            if (name == "Real") os << "ℝ";
            else if (name == "Rational") os << "ℚ";
            else if (name == "Integer") os << "ℤ";
            else os << name;
            break;
        case Finite: {
            os << "{";
            for (size_t i = 0; i < elems.size(); ++i) {
                if (i) os << ", ";
                os << elems[i].toFraction();
            }
            os << "}";
            break;
        }
        case Interval: {
            os << (loClosed ? "[" : "(") << lo.toFraction() << ", " << hi.toFraction() << (hiClosed ? "]" : ")");
            break;
        }
        case Union: os << "(" << a->toString() << " ∪ " << b->toString() << ")"; break;
        case Intersect: os << "(" << a->toString() << " ∩ " << b->toString() << ")"; break;
        case Diff: os << "(" << a->toString() << " \\ " << b->toString() << ")"; break;
    }
    return os.str();
}

BigRational Value::toRational() const {
    if (type == Rational) return rat;
    if (type == Decimal) return dec.toRational();
    if (type == Boolean) return BigRational(boolean ? 1 : 0);
    throw std::runtime_error("cannot convert value to rational");
}

BigFloat Value::toBigFloat(unsigned prec) const {
    if (type == Decimal) return dec;
    if (type == Rational) return BigFloat::fromRational(rat, prec);
    if (type == Boolean) return BigFloat(boolean ? 1 : 0);
    throw std::runtime_error("cannot convert value to big float");
}

} // namespace scicalc
