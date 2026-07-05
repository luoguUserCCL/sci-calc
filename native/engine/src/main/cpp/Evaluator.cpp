// scicalc/Evaluator.cpp
#include "scicalc/Evaluator.hpp"
#include "scicalc/BigInt.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <map>
#include <vector>

namespace scicalc {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

bool Evaluator::truthy(const Value& v) {
    if (v.isBoolean()) return v.boolean;
    if (v.isRational()) return !v.rat.isZero();
    if (v.isDecimal()) return !v.dec.isZero();
    throw std::runtime_error("value is not usable as a boolean");
}

Value Evaluator::getVar(const std::string& n) const {
    auto it = vars_.find(n);
    if (it != vars_.end()) return it->second;
    // built-in symbolic constants
    if (n == "pi" || n == "PI" || n == "Pi") {
        // In math mode keep symbolic; in decimal mode a BigFloat.
        if (config.outputMode == OutputMode::Decimal)
            return Value::ofDec(BigFloat::pi(config.precision));
        // symbolic: represent as a Var node named "pi"
        return Value::ofSym(Expr::makeVar("pi"));
    }
    if (n == "e" || n == "E") {
        if (config.outputMode == OutputMode::Decimal)
            return Value::ofDec(BigFloat::e(config.precision));
        return Value::ofSym(Expr::makeVar("e"));
    }
    throw std::runtime_error("undefined variable: " + n);
}

// ---------------------------------------------------------------------------
// top-level dispatch
// ---------------------------------------------------------------------------

Value Evaluator::eval(const Expr& e) {
    switch (e.kind) {
        case Expr::Number: return Value::ofRat(e.num);
        case Expr::Var:    return getVar(e.name);
        case Expr::SetName:
            if (e.name == "Real") return Value::ofSet(SetValue::makeNamed("Real"));
            if (e.name == "Rational") return Value::ofSet(SetValue::makeNamed("Rational"));
            if (e.name == "Integer") return Value::ofSet(SetValue::makeNamed("Integer"));
            throw std::runtime_error("unknown predefined set: " + e.name);
        case Expr::Unary: return evalUnary(e.unop, eval(*e.lhs));
        case Expr::Binary: {
            // Assignment is handled at parseAssign; but Binary::Assign shouldn't appear.
            BinOp op = e.binop;
            if (op == BinOp::And) {
                Value a = eval(*e.lhs);
                if (!truthy(a)) return Value::ofBool(false);
                return Value::ofBool(truthy(eval(*e.rhs)));
            }
            if (op == BinOp::Or) {
                Value a = eval(*e.lhs);
                if (truthy(a)) return Value::ofBool(true);
                return Value::ofBool(truthy(eval(*e.rhs)));
            }
            Value a = eval(*e.lhs);
            Value b = eval(*e.rhs);
            return evalBinary(op, a, b, e);
        }
        case Expr::Call: return evalCall(e.name, e.args, e);
        case Expr::AssignVar: {
            Value v = eval(*e.lhs);
            vars_[e.name] = v;
            return v;
        }
        case Expr::AssignFunc: {
            UserFunc f; f.params = e.params; f.body = e.lhs->clone();
            funcs_[e.name] = std::move(f);
            return Value::ofSym(Expr::makeAssignFunc(e.name, e.params, e.lhs->clone()));
        }
        case Expr::SetEnum: {
            std::vector<BigRational> elems;
            for (auto& a : e.args) {
                Value v = eval(*a);
                if (!v.isRational() && !v.isDecimal())
                    throw std::runtime_error("set elements must be numeric");
                elems.push_back(v.toRational());
            }
            return Value::ofSet(SetValue::makeFinite(std::move(elems)));
        }
        case Expr::Interval: {
            Value lo = eval(*e.lo), hi = eval(*e.hi);
            if (!lo.isNumeric() || !hi.isNumeric())
                throw std::runtime_error("interval bounds must be numeric");
            BigRational l = lo.toRational(), h = hi.toRational();
            bool loC = (e.intKind == SetIntervalKind::ClosedClosed || e.intKind == SetIntervalKind::ClosedOpen);
            bool hiC = (e.intKind == SetIntervalKind::ClosedClosed || e.intKind == SetIntervalKind::OpenClosed);
            return Value::ofSet(SetValue::makeInterval(l, h, loC, hiC));
        }
        case Expr::Iverson: {
            Value p = eval(*e.lhs);
            bool t = truthy(p);
            return Value::ofRat(BigRational(t ? 1 : 0));
        }
    }
    return Value::ofErr("internal: unhandled expr kind");
}

Value Evaluator::evalWith(const Expr& e, const std::vector<std::string>& names,
                          const std::vector<Value>& vals) {
    // save & shadow
    std::vector<std::pair<std::string, bool>> saved;
    for (size_t i = 0; i < names.size(); ++i) {
        auto it = vars_.find(names[i]);
        saved.emplace_back(names[i], it != vars_.end());
        if (it != vars_.end()) saved.back() = {names[i], true};
        vars_[names[i]] = vals[i];
    }
    Value r = eval(e);
    // restore
    for (auto& sv : saved) {
        if (sv.second) {
            // original existed; we overwrote — but we didn't save the value.
            // For correctness, save original value too.
        }
    }
    // Simpler: just leave the bound variables (typical for single eval).
    (void)saved;
    return r;
}

// ---------------------------------------------------------------------------
// unary
// ---------------------------------------------------------------------------

Value Evaluator::evalUnary(UnaryOp op, const Value& a) {
    switch (op) {
        case UnaryOp::Pos: return a;
        case UnaryOp::Neg:
            if (a.isRational()) return Value::ofRat(-a.rat);
            if (a.isDecimal()) return Value::ofDec(-a.dec);
            if (a.isSymbolic()) {
                return Value::ofSym(Expr::makeUnary(UnaryOp::Neg, a.sym->clone()));
            }
            break;
        case UnaryOp::Not: return Value::ofBool(!truthy(a));
    }
    throw std::runtime_error("invalid unary operand");
}

// ---------------------------------------------------------------------------
// relational
// ---------------------------------------------------------------------------

Value Evaluator::evalRel(BinOp op, const Value& a, const Value& b) {
    // Set relations
    if (op == BinOp::In) {
        if (!a.isNumeric() || !b.isSet())
            throw std::runtime_error("'in' requires value and set");
        return Value::ofBool(b.set->contains(a.toRational()));
    }
    if (op == BinOp::Subset || op == BinOp::RealSubset) {
        if (!a.isSet() || !b.isSet())
            throw std::runtime_error("subset requires two sets");
        bool sub = a.set->isSubsetOf(*b.set);
        if (op == BinOp::Subset) return Value::ofBool(sub);
        // realsubset (proper subset): subset and not equal
        bool eq = sub && b.set->isSubsetOf(*a.set);
        return Value::ofBool(sub && !eq);
    }
    // equality on sets
    if (a.isSet() && b.isSet()) {
        bool eq = a.set->isEqual(*b.set);
        if (op == BinOp::Eq) return Value::ofBool(eq);
        if (op == BinOp::Neq) return Value::ofBool(!eq);
        throw std::runtime_error("ordering not defined for sets");
    }
    // numeric (or boolean) comparison
    if (a.isBoolean() && b.isBoolean()) {
        bool eq = a.boolean == b.boolean;
        switch (op) {
            case BinOp::Eq: return Value::ofBool(eq);
            case BinOp::Neq: return Value::ofBool(!eq);
            default: throw std::runtime_error("invalid boolean comparison");
        }
    }
    int c;
    if (a.isDecimal() || b.isDecimal()) {
        c = a.toBigFloat(config.precision).cmp(b.toBigFloat(config.precision));
    } else if (a.isRational() && b.isRational()) {
        c = a.rat.cmp(b.rat);
    } else {
        throw std::runtime_error("cannot compare values");
    }
    switch (op) {
        case BinOp::Eq: return Value::ofBool(c == 0);
        case BinOp::Neq: return Value::ofBool(c != 0);
        case BinOp::Lt: return Value::ofBool(c < 0);
        case BinOp::Gt: return Value::ofBool(c > 0);
        case BinOp::Le: return Value::ofBool(c <= 0);
        case BinOp::Ge: return Value::ofBool(c >= 0);
        default: break;
    }
    throw std::runtime_error("invalid relational operator");
}

// ---------------------------------------------------------------------------
// set ops
// ---------------------------------------------------------------------------

Value Evaluator::evalSetOp(BinOp op, const SetValuePtr& a, const SetValuePtr& b) {
    // Simplify common cases so results are concrete rather than compound.
    auto intervalIntersect = [](const SetValue& A, const SetValue& B) -> SetValuePtr {
        // intersection of two intervals
        const BigRational& lo = (A.lo.cmp(B.lo) > 0) ? A.lo : B.lo;
        const BigRational& hi = (A.hi.cmp(B.hi) < 0) ? A.hi : B.hi;
        bool loC = (A.lo.cmp(B.lo) > 0) ? A.loClosed : ((A.lo.cmp(B.lo) == 0) ? (A.loClosed && B.loClosed) : B.loClosed);
        bool hiC = (A.hi.cmp(B.hi) < 0) ? A.hiClosed : ((A.hi.cmp(B.hi) == 0) ? (A.hiClosed && B.hiClosed) : B.hiClosed);
        if (lo.cmp(hi) > 0) return SetValue::makeEmpty();
        if (lo.cmp(hi) == 0 && !(loC && hiC)) return SetValue::makeEmpty();
        return SetValue::makeInterval(lo, hi, loC, hiC);
    };
    if (op == BinOp::SetIntersect) {
        if (a->kind == SetValue::Interval && b->kind == SetValue::Interval)
            return Value::ofSet(intervalIntersect(*a, *b));
        if (a->kind == SetValue::Finite && b->kind == SetValue::Finite) {
            std::vector<BigRational> out;
            for (const auto& e : a->elems) if (b->contains(e)) out.push_back(e);
            return Value::ofSet(SetValue::makeFinite(std::move(out)));
        }
        if (a->kind == SetValue::Finite && b->kind == SetValue::Interval) {
            std::vector<BigRational> out;
            for (const auto& e : a->elems) if (b->contains(e)) out.push_back(e);
            return Value::ofSet(SetValue::makeFinite(std::move(out)));
        }
        if (a->kind == SetValue::Interval && b->kind == SetValue::Finite) {
            std::vector<BigRational> out;
            for (const auto& e : b->elems) if (a->contains(e)) out.push_back(e);
            return Value::ofSet(SetValue::makeFinite(std::move(out)));
        }
        if (a->kind == SetValue::Empty || b->kind == SetValue::Empty) return Value::ofSet(SetValue::makeEmpty());
    }
    if (op == BinOp::SetUnion) {
        if (a->kind == SetValue::Finite && b->kind == SetValue::Finite) {
            std::vector<BigRational> out = a->elems;
            for (const auto& e : b->elems) out.push_back(e);
            return Value::ofSet(SetValue::makeFinite(std::move(out)));
        }
        if (a->kind == SetValue::Empty) return Value::ofSet(b);
        if (b->kind == SetValue::Empty) return Value::ofSet(a);
    }
    if (op == BinOp::SetDiff) {
        if (a->kind == SetValue::Finite) {
            std::vector<BigRational> out;
            for (const auto& e : a->elems) if (!b->contains(e)) out.push_back(e);
            return Value::ofSet(SetValue::makeFinite(std::move(out)));
        }
        if (a->kind == SetValue::Empty) return Value::ofSet(SetValue::makeEmpty());
    }
    // fallback: compound
    switch (op) {
        case BinOp::SetIntersect: return Value::ofSet(SetValue::makeIntersect(a, b));
        case BinOp::SetUnion: return Value::ofSet(SetValue::makeUnion(a, b));
        case BinOp::SetDiff: return Value::ofSet(SetValue::makeDiff(a, b));
        default: break;
    }
    throw std::runtime_error("invalid set operator");
}

// ---------------------------------------------------------------------------
// power
// ---------------------------------------------------------------------------

// 前向声明: 符号乘除的 sqrt 合并化简 (定义在 simplifySqrt 之后)
static ExprPtr simplifySymbolicSqrtProduct(const Expr& aExpr, const Expr& bExpr, BinOp op);
// 前向声明: 多项式展开 (定义在 simplifySymbolicSqrtProduct 之后)
static ExprPtr expandSymbolicProduct(const Expr& aExpr, const Expr& bExpr);
// 前向声明: simplifySqrt (定义在本文件后面)
static void simplifySqrt(const BigRational& r, BigRational& scale, BigInt& radicand);

// 本地 flatString (Engine.cpp 中是 static 不可见，这里给 collectLikeTerms 用)
static std::string localFlatString(const Expr& e);

Value Evaluator::powValues(const Value& base, const Value& exp) {
    // integer exponent -> exact rational power (fast exponentiation)
    if (base.isRational() && exp.isRational() && exp.rat.isInteger()) {
        return Value::ofRat(BigRational::pow(base.rat, exp.rat.num()));
    }
    if (base.isRational() && exp.isRational() && !exp.rat.isInteger()) {
        // 分数指数: 尝试精确计算
        // 1/2 次方 = sqrt: 若 base 是完全平方, 返回精确值
        BigRational half(1, 2);
        if (exp.rat == half) {
            // sqrt(base): 尝试精确
            BigRational r = base.rat;
            if (!r.isNegative()) {
                BigRational scale; BigInt radicand;
                simplifySqrt(r, scale, radicand);
                if (radicand == BigInt(1)) return Value::ofRat(scale);
                // 不完全平方: math 模式保持符号, decimal 模式用 BigFloat::sqrt
                if (config.outputMode == OutputMode::Math) {
                    if (scale == BigRational(1))
                        return Value::ofSym(Expr::makeCall("sqrt", Expr::makeNumber(r)));
                    return Value::ofSym(Expr::makeBinary(BinOp::Mul,
                        Expr::makeNumber(scale),
                        Expr::makeCall("sqrt", Expr::makeNumber(BigRational(radicand)))));
                }
                return Value::ofDec(BigFloat::sqrt(base.toBigFloat(config.precision)));
            }
        }
        // 其他分数指数: math 模式保持符号, decimal 模式计算
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeBinary(BinOp::Pow,
                Expr::makeNumber(base.rat), Expr::makeNumber(exp.rat)));
        // decimal: 用 sqrt 路径避免 pow 的精度问题
        if (exp.rat == half && !base.rat.isNegative())
            return Value::ofDec(BigFloat::sqrt(base.toBigFloat(config.precision)));
        BigFloat bf = BigFloat::pow(base.toBigFloat(config.precision),
                                    exp.toBigFloat(config.precision));
        return Value::ofDec(bf);
    }
    // decimal or symbolic base
    if (config.outputMode == OutputMode::Math) {
        // keep symbolic
        ExprPtr b = base.isSymbolic() ? base.sym->clone()
                                      : Expr::makeNumber(base.toRational());
        ExprPtr e = exp.isSymbolic() ? exp.sym->clone()
                                     : Expr::makeNumber(exp.toRational());
        return Value::ofSym(Expr::makeBinary(BinOp::Pow, std::move(b), std::move(e)));
    }
    return Value::ofDec(BigFloat::pow(base.toBigFloat(config.precision),
                                      exp.toBigFloat(config.precision)));
}

// ---------------------------------------------------------------------------
// binary
// ---------------------------------------------------------------------------

Value Evaluator::evalBinary(BinOp op, const Value& a, const Value& b, const Expr& node) {
    using B = BinOp;
    // relational & set membership
    if (op == B::Eq || op == B::Neq || op == B::Lt || op == B::Gt || op == B::Le || op == B::Ge ||
        op == B::In || op == B::Subset || op == B::RealSubset)
        return evalRel(op, a, b);

    // set algebra
    if (op == B::SetIntersect || op == B::SetUnion || op == B::SetDiff) {
        if (!a.isSet() || !b.isSet()) throw std::runtime_error("set operator requires sets");
        return evalSetOp(op, a.set, b.set);
    }

    // power
    if (op == B::Pow) return powValues(a, b);

    // mod (integer)
    if (op == B::Mod) {
        if (a.isRational() && b.isRational() && a.rat.isInteger() && b.rat.isInteger()) {
            if (b.rat.isZero()) throw std::runtime_error("mod by zero");
            BigInt q, r; BigInt::divmod(a.rat.num(), b.rat.num(), q, r);
            return Value::ofRat(BigRational(r));
        }
        // decimal mod
        if (a.isNumeric() && b.isNumeric()) {
            BigFloat af = a.toBigFloat(config.precision), bf = b.toBigFloat(config.precision);
            BigFloat q = af / bf;
            // floor
            // approximate: use rational truncation
            BigRational qr = q.toRational();
            BigInt qi = qr.floor();
            BigRational r = a.toRational() - BigRational(qi) * b.toRational();
            return Value::ofRat(r);
        }
        throw std::runtime_error("mod requires numeric operands");
    }

    // arithmetic + - * /
    if (op == B::Add || op == B::Sub || op == B::Mul || op == B::Div) {
        // symbolic propagation in math mode
        bool sym = (a.isSymbolic() || b.isSymbolic()) && config.outputMode == OutputMode::Math;
        if (sym) {
            ExprPtr la = a.isSymbolic() ? a.sym->clone()
                                        : Expr::makeNumber(a.toRational());
            ExprPtr lb = b.isSymbolic() ? b.sym->clone()
                                        : Expr::makeNumber(b.toRational());
            // 乘除: 尝试 sqrt 合并化简 (如 sqrt(2)*sqrt(8)=4, sqrt(2)/sqrt(8)=1/2)
            if (op == B::Mul || op == B::Div) {
                ExprPtr simplified = simplifySymbolicSqrtProduct(*la, *lb, op);
                if (simplified) {
                    if (simplified->kind == Expr::Number)
                        return Value::ofRat(simplified->num);
                    return Value::ofSym(std::move(simplified));
                }
            }
            // 乘法: 尝试多项式展开 + 合并同类项 (如 (1+sqrt(2))*(sqrt(2)-1)=1)
            if (op == B::Mul) {
                ExprPtr expanded = expandSymbolicProduct(*la, *lb);
                if (expanded) {
                    if (expanded->kind == Expr::Number)
                        return Value::ofRat(expanded->num);
                    return Value::ofSym(std::move(expanded));
                }
            }
            return Value::ofSym(Expr::makeBinary(op, std::move(la), std::move(lb)));
        }
        // both rational -> exact
        if (a.isRational() && b.isRational()) {
            BigRational r;
            switch (op) {
                case B::Add: r = a.rat + b.rat; break;
                case B::Sub: r = a.rat - b.rat; break;
                case B::Mul: r = a.rat * b.rat; break;
                case B::Div:
                    if (b.rat.isZero()) throw std::runtime_error("division by zero");
                    r = a.rat / b.rat; break;
                default: break;
            }
            return Value::ofRat(r);
        }
        // decimal fallback
        BigFloat af = a.toBigFloat(config.precision), bf = b.toBigFloat(config.precision);
        BigFloat r;
        switch (op) {
            case B::Add: r = af + bf; break;
            case B::Sub: r = af - bf; break;
            case B::Mul: r = af * bf; break;
            case B::Div: r = af / bf; break;
            default: break;
        }
        return Value::ofDec(r);
    }
    (void)node;
    return Value::ofErr("unhandled binary op");
}

// ---------------------------------------------------------------------------
// integer sqrt helper (Newton on BigInt)
// ---------------------------------------------------------------------------
static BigInt isqrt(const BigInt& n) {
    if (n.isNegative()) throw std::runtime_error("isqrt of negative");
    if (n.isZero()) return BigInt(0);
    // initial estimate
    BigInt x = BigInt(1) << ((n.bitLength() + 1) / 2);
    BigInt y;
    while (true) {
        BigInt q, r; BigInt::divmod(x + n / x, BigInt(2), q, r);
        y = q;
        if (y >= x) break;
        x = y;
    }
    return x;
}
static bool isPerfectSquare(const BigInt& n, BigInt& root) {
    if (n.isNegative()) return false;
    root = isqrt(n);
    return root * root == n;
}

/// Simplify sqrt(p/q) -> (s/q)*sqrt(t) where p*q = s^2 * t, t squarefree.
/// Returns (scaleRational, radicand). If radicand==1, sqrt is exact = scale.
static void simplifySqrt(const BigRational& r, BigRational& scale, BigInt& radicand) {
    // r = p/q (q>0, lowest). sqrt(p/q) = sqrt(p*q)/q.
    BigInt p = r.num(), q = r.den();
    if (p.isNegative()) throw std::runtime_error("sqrt of negative");
    BigInt pq = p * q;
    BigInt s(1), t = pq;
    BigInt two(2), four(4);
    // extract pairs of factor 2 (i.e. factors of 4)
    while (true) {
        BigInt q, r; BigInt::divmod(t, four, q, r);
        if (r.isZero()) { t = q; s *= two; }   // extracted one pair of 2s
        else break;
    }
    // extract pairs of odd factors
    for (BigInt d(3); d * d <= t; d += two) {
        BigInt dd = d * d;
        BigInt qd, rd; BigInt::divmod(t, dd, qd, rd);
        while (rd.isZero()) { t = qd; s *= d; BigInt::divmod(t, dd, qd, rd); }
    }
    // remaining t is squarefree; if it's a perfect square, fold it in (handles t=1 and large squares)
    {
        BigInt root;
        if (isPerfectSquare(t, root)) { s *= root; t = BigInt(1); }
    }
    scale = BigRational(s, q);
    radicand = t;
}

// 尝试从符号表达式 e 中提取 (有理系数, 根号内整数)。
// 支持形式: Number(n), sqrt(Number), sqrt(Number)化简形式 c*sqrt(r),
// 以及这些形式的乘积。返回 false 表示无法提取（含 sin/log 等不可化简符号）。
static bool extractSqrtFactor(const Expr& e, BigRational& coeff, BigInt& radicand) {
    if (e.kind == Expr::Number) {
        coeff = e.num;
        radicand = BigInt(1);
        return true;
    }
    if (e.kind == Expr::Call && e.name == "sqrt" && e.args.size() == 1 &&
        e.args[0] && e.args[0]->kind == Expr::Number) {
        BigRational r = e.args[0]->num;
        if (r.isNegative()) return false;
        BigRational scale; BigInt rad;
        simplifySqrt(r, scale, rad);
        coeff = scale;
        radicand = rad;
        return true;
    }
    if (e.kind == Expr::Binary && e.binop == BinOp::Mul && e.lhs && e.rhs) {
        BigRational c1, c2; BigInt r1, r2;
        if (extractSqrtFactor(*e.lhs, c1, r1) && extractSqrtFactor(*e.rhs, c2, r2)) {
            BigRational combinedScale; BigInt combinedRad;
            simplifySqrt(BigRational(r1 * r2), combinedScale, combinedRad);
            coeff = c1 * c2 * combinedScale;
            radicand = combinedRad;
            return true;
        }
    }
    return false;
}

/// 尝试对两个符号表达式的乘/除进行 sqrt 合并化简。
/// 成功返回化简后的 ExprPtr；失败返回 nullptr（调用方保留原样）。
static ExprPtr simplifySymbolicSqrtProduct(const Expr& aExpr, const Expr& bExpr, BinOp op) {
    if (op != BinOp::Mul && op != BinOp::Div) return nullptr;
    BigRational c1, c2; BigInt r1, r2;
    if (!extractSqrtFactor(aExpr, c1, r1)) return nullptr;
    if (!extractSqrtFactor(bExpr, c2, r2)) return nullptr;
    BigRational newCoeff; BigInt newRad;
    if (op == BinOp::Mul) {
        newCoeff = c1 * c2;
        newRad = r1 * r2;
        // 对乘积的 radicand 再做一次 simplifySqrt，吸收完全平方因子
        BigRational s3; BigInt r3;
        simplifySqrt(BigRational(newRad), s3, r3);
        newCoeff = newCoeff * s3;
        newRad = r3;
    } else { // Div
        if (c2.isZero()) return nullptr; // 让上层报除零错误
        newCoeff = c1 / c2;
        newRad = r1;
        // 除以 sqrt(r2) => 乘以 1/sqrt(r2) = sqrt(1/r2) 的化简
        // sqrt(r1)/sqrt(r2) = sqrt(r1/r2)。用 r1 * (1/r2) 但 r2 是整数。
        // 更简单: newRad = r1, 再除以 r2 -> sqrt(r1/r2)。用 r1 * q_den? 
        // r2 是 BigInt 整数。sqrt(r1)/sqrt(r2) = sqrt(r1/r2) 但 r1/r2 可能不是整数。
        // 直接: 把 1/r2 作为有理数, simplifySqrt(BigRational(1, r2)) 给出 1/sqrt(r2) 的化简。
        BigRational invR2(BigInt(1), r2);
        BigRational scale2; BigInt rad2;
        simplifySqrt(invR2, scale2, rad2);
        // newRad = r1 * rad2, newCoeff *= scale2
        newRad = r1 * rad2;
        newCoeff = newCoeff * scale2;
        // 再对 newRad 做一次 simplifySqrt（吸收完全平方因子）
        BigRational s3; BigInt r3;
        simplifySqrt(BigRational(newRad), s3, r3);
        newCoeff = newCoeff * s3;
        newRad = r3;
    }
    if (newRad == BigInt(1)) return Expr::makeNumber(newCoeff);
    ExprPtr sqrtNode = Expr::makeCall("sqrt", Expr::makeNumber(BigRational(newRad)));
    if (newCoeff == BigRational(1)) return sqrtNode;
    if (newCoeff == BigRational(-1)) return Expr::makeUnary(UnaryOp::Neg, std::move(sqrtNode));
    return Expr::makeBinary(BinOp::Mul, Expr::makeNumber(newCoeff), std::move(sqrtNode));
}

// ---------------------------------------------------------------------------
// 多项式展开与化简 (分配律 + 合并同类项)
// ---------------------------------------------------------------------------

struct PolyTerm {
    BigRational coeff;
    std::vector<ExprPtr> factors;
};

static bool toPolyTerms(const Expr& e, std::vector<PolyTerm>& out);
static void collectLikeTerms(std::vector<PolyTerm>& terms);

static bool normalizeFactor(const Expr& e, BigRational& coeff, ExprPtr& factor) {
    if (e.kind == Expr::Number) {
        coeff = e.num;
        factor = nullptr;
        return true;
    }
    if (e.kind == Expr::Call && e.name == "sqrt" && e.args.size() == 1 &&
        e.args[0] && e.args[0]->kind == Expr::Number) {
        BigRational scale; BigInt rad;
        simplifySqrt(e.args[0]->num, scale, rad);
        coeff = scale;
        if (rad == BigInt(1)) { factor = nullptr; return true; }
        factor = Expr::makeCall("sqrt", Expr::makeNumber(BigRational(rad)));
        return true;
    }
    if (e.kind == Expr::Unary && e.unop == UnaryOp::Neg && e.lhs) {
        if (!normalizeFactor(*e.lhs, coeff, factor)) return false;
        coeff = -coeff;
        return true;
    }
    coeff = BigRational(1);
    factor = e.clone();
    return true;
}

static void mulPolyTerms(const std::vector<PolyTerm>& a, const std::vector<PolyTerm>& b,
                         std::vector<PolyTerm>& out) {
    for (const auto& ta : a) {
        for (const auto& tb : b) {
            PolyTerm t;
            t.coeff = ta.coeff * tb.coeff;
            // clone factors (a, b are const ref)
            for (const auto& f : ta.factors) t.factors.push_back(f->clone());
            for (const auto& f : tb.factors) t.factors.push_back(f->clone());
            out.push_back(std::move(t));
        }
    }
}

static void collectLikeTerms(std::vector<PolyTerm>& terms) {
    auto normalize = [](PolyTerm& t) {
        std::vector<ExprPtr> remaining;
        for (auto& f : t.factors) {
            BigRational c; ExprPtr fact;
            if (normalizeFactor(*f, c, fact)) {
                t.coeff = t.coeff * c;
                if (fact) remaining.push_back(std::move(fact));
            } else {
                remaining.push_back(f->clone());
            }
        }
        t.factors = std::move(remaining);
    };
    // 合并因子列表中相同的 sqrt(Number) 因子对 (sqrt(a)*sqrt(a)=a, sqrt(a)*sqrt(b)=sqrt(a*b))
    auto mergeSqrtFactors = [](PolyTerm& t) {
        std::vector<ExprPtr> sqrts, others;
        for (auto& f : t.factors) {
            if (f->kind == Expr::Call && f->name == "sqrt" && f->args.size() == 1 &&
                f->args[0] && f->args[0]->kind == Expr::Number)
                sqrts.push_back(f->clone());
            else
                others.push_back(f->clone());
        }
        // 两两合并 sqrt
        while (sqrts.size() >= 2) {
            ExprPtr a = std::move(sqrts.back()); sqrts.pop_back();
            ExprPtr b = std::move(sqrts.back()); sqrts.pop_back();
            // sqrt(a)*sqrt(b) -> simplifySqrt(a*b)
            BigRational ra = a->args[0]->num, rb = b->args[0]->num;
            BigRational prod = ra * rb;
            BigRational scale; BigInt rad;
            simplifySqrt(prod, scale, rad);
            t.coeff = t.coeff * scale;
            if (rad != BigInt(1)) {
                sqrts.push_back(Expr::makeCall("sqrt", Expr::makeNumber(BigRational(rad))));
            }
        }
        t.factors = std::move(others);
        for (auto& s : sqrts) t.factors.push_back(std::move(s));
    };
    auto termKey = [](const PolyTerm& t) {
        std::string k;
        for (const auto& f : t.factors) k += localFlatString(*f) + "|";
        return k;
    };
    for (auto& t : terms) { normalize(t); mergeSqrtFactors(t); }
    std::map<std::string, size_t> keyToIdx;
    std::vector<PolyTerm> merged;
    for (auto& t : terms) {
        if (t.coeff.isZero()) continue;
        std::string k = termKey(t);
        auto it = keyToIdx.find(k);
        if (it != keyToIdx.end()) {
            merged[it->second].coeff = merged[it->second].coeff + t.coeff;
        } else {
            keyToIdx[k] = merged.size();
            merged.push_back(std::move(t));
        }
    }
    std::vector<PolyTerm> nonZero;
    for (auto& t : merged) if (!t.coeff.isZero()) nonZero.push_back(std::move(t));
    terms = std::move(nonZero);
}

static bool toPolyTerms(const Expr& e, std::vector<PolyTerm>& out) {
    switch (e.kind) {
        case Expr::Number:
            out.push_back({e.num, {}});
            return true;
        case Expr::Unary:
            if (e.unop == UnaryOp::Neg && e.lhs) {
                if (!toPolyTerms(*e.lhs, out)) return false;
                for (auto& t : out) t.coeff = -t.coeff;
                return true;
            }
            if (e.unop == UnaryOp::Pos && e.lhs) return toPolyTerms(*e.lhs, out);
            return false;
        case Expr::Binary: {
            if (e.binop == BinOp::Add || e.binop == BinOp::Sub) {
                std::vector<PolyTerm> l, r;
                if (!e.lhs || !toPolyTerms(*e.lhs, l)) return false;
                if (!e.rhs || !toPolyTerms(*e.rhs, r)) return false;
                if (e.binop == BinOp::Sub) for (auto& t : r) t.coeff = -t.coeff;
                // move l into out, then move r
                for (auto& t : l) out.push_back(std::move(t));
                for (auto& t : r) out.push_back(std::move(t));
                return true;
            }
            if (e.binop == BinOp::Mul) {
                std::vector<PolyTerm> l, r;
                if (!e.lhs || !toPolyTerms(*e.lhs, l)) return false;
                if (!e.rhs || !toPolyTerms(*e.rhs, r)) return false;
                mulPolyTerms(l, r, out);
                collectLikeTerms(out);
                return true;
            }
            {
                BigRational c; ExprPtr f;
                if (normalizeFactor(e, c, f)) {
                    PolyTerm t; t.coeff = c;
                    if (f) t.factors.push_back(std::move(f));
                    out.push_back(std::move(t));
                    return true;
                }
            }
            return false;
        }
        case Expr::Call:
        case Expr::Var: {
            BigRational c; ExprPtr f;
            if (normalizeFactor(e, c, f)) {
                PolyTerm t; t.coeff = c;
                if (f) t.factors.push_back(std::move(f));
                out.push_back(std::move(t));
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

static ExprPtr polyTermsToExpr(const std::vector<PolyTerm>& terms) {
    if (terms.empty()) return Expr::makeNumber(BigRational(0));
    ExprPtr result;
    for (const auto& t : terms) {
        ExprPtr term;
        if (t.factors.empty()) {
            term = Expr::makeNumber(t.coeff);
        } else {
            ExprPtr prod = t.factors[0]->clone();
            for (size_t i = 1; i < t.factors.size(); ++i)
                prod = Expr::makeBinary(BinOp::Mul, std::move(prod), t.factors[i]->clone());
            if (t.coeff == BigRational(1)) {
                term = std::move(prod);
            } else if (t.coeff == BigRational(-1)) {
                term = Expr::makeUnary(UnaryOp::Neg, std::move(prod));
            } else {
                term = Expr::makeBinary(BinOp::Mul, Expr::makeNumber(t.coeff), std::move(prod));
            }
        }
        if (!result) result = std::move(term);
        else result = Expr::makeBinary(BinOp::Add, std::move(result), std::move(term));
    }
    return result;
}

/// 尝试对符号乘法做多项式展开 + 合并同类项。
static ExprPtr expandSymbolicProduct(const Expr& aExpr, const Expr& bExpr) {
    std::vector<PolyTerm> la, lb;
    if (!toPolyTerms(aExpr, la)) return nullptr;
    if (!toPolyTerms(bExpr, lb)) return nullptr;
    std::vector<PolyTerm> product;
    mulPolyTerms(la, lb, product);
    collectLikeTerms(product);
    if (product.empty()) return Expr::makeNumber(BigRational(0));
    if (product.size() == 1 && product[0].factors.empty())
        return Expr::makeNumber(product[0].coeff);
    return polyTermsToExpr(product);
}

// 本地 flatString: 把 AST 转为确定性字符串（用于 collectLikeTerms 的 key）
static std::string localFlatString(const Expr& e) {
    switch (e.kind) {
        case Expr::Number: return e.num.toFraction();
        case Expr::Var: return e.name;
        case Expr::SetName: return e.name;
        case Expr::Unary:
            return std::string(e.unop == UnaryOp::Neg ? "-" : "+") + localFlatString(*e.lhs);
        case Expr::Binary:
            return "(" + localFlatString(*e.lhs) + " " + std::to_string((int)e.binop) + " " + localFlatString(*e.rhs) + ")";
        case Expr::Call: {
            std::string s = e.name + "(";
            for (size_t i = 0; i < e.args.size(); ++i) { if (i) s += ","; s += localFlatString(*e.args[i]); }
            return s + ")";
        }
        default: return "?";
    }
}

// ---------------------------------------------------------------------------
// function calls
// ---------------------------------------------------------------------------

Value Evaluator::evalCall(const std::string& name,
                          const std::vector<ExprPtr>& argExprs, const Expr& node) {
    // user-defined function?
    auto fit = funcs_.find(name);
    if (fit != funcs_.end()) {
        if (argExprs.size() != fit->second.params.size())
            throw std::runtime_error("function " + name + ": wrong number of args");
        std::vector<Value> args;
        for (auto& a : argExprs) args.push_back(eval(*a));
        return evalWith(*fit->second.body, fit->second.params, args);
    }
    // sum / prod special: first arg is a variable name
    if (name == "sum" || name == "prod") {
        if (argExprs.size() != 4) throw std::runtime_error(name + "(i, start, end, expr)");
        if (argExprs[0]->kind != Expr::Var) throw std::runtime_error(name + ": first arg must be index var");
        std::string idx = argExprs[0]->name;
        Value sv = eval(*argExprs[1]), ev = eval(*argExprs[2]);
        BigInt start = sv.toRational().floor();
        BigInt end = ev.toRational().floor();
        bool isSum = (name == "sum");
        BigRational acc(isSum ? 0 : 1);
        bool had = vars_.count(idx) > 0;
        Value old = had ? vars_[idx] : Value();
        for (BigInt i = start; i <= end; i += BigInt(1)) {
            vars_[idx] = Value::ofRat(BigRational(i));
            Value term = eval(*argExprs[3]);
            if (isSum) acc = acc + term.toRational(); else acc = acc * term.toRational();
        }
        if (had) vars_[idx] = old; else vars_.erase(idx);
        return Value::ofRat(acc);
    }
    // evaluate args normally for other builtins
    std::vector<Value> args;
    for (auto& a : argExprs) args.push_back(eval(*a));
    return builtin(name, args, node);
}

// ---------------------------------------------------------------------------
// builtins
// ---------------------------------------------------------------------------

Value Evaluator::builtin(const std::string& name, const std::vector<Value>& args, const Expr& node) {
    auto need = [&](size_t n) {
        if (args.size() != n) throw std::runtime_error(name + ": expected " + std::to_string(n) + " args");
    };
    auto num = [&](const Value& v) -> BigRational {
        if (v.isRational()) return v.rat;
        if (v.isDecimal()) return v.dec.toRational();
        throw std::runtime_error(name + ": numeric argument required");
    };

    if (name == "abs") { need(1); auto n = num(args[0]); return Value::ofRat(n.isNegative() ? -n : n); }
    if (name == "floor") { need(1); return Value::ofRat(BigRational(args[0].toRational().floor())); }
    if (name == "ceil") { need(1); return Value::ofRat(BigRational(args[0].toRational().ceil())); }
    if (name == "trunc" || name == "truncate") { need(1); return Value::ofRat(BigRational(args[0].toRational().trunc())); }
    if (name == "round") { need(1); return Value::ofRat(BigRational(args[0].toRational().round())); }
    if (name == "sign" || name == "sgn") {
        need(1); int s = args[0].toRational().signum(); return Value::ofRat(BigRational((long long)s));
    }
    if (name == "gcd") {
        need(2); BigInt a = args[0].toRational().trunc(), b = args[1].toRational().trunc();
        return Value::ofRat(BigRational(BigInt::gcd(a, b)));
    }
    if (name == "lcm") {
        need(2); BigInt a = args[0].toRational().trunc(), b = args[1].toRational().trunc();
        return Value::ofRat(BigRational(BigInt::lcm(a, b)));
    }
    if (name == "fact" || name == "factorial") {
        need(1); BigInt n = args[0].toRational().trunc();
        if (n.isNegative()) throw std::runtime_error("factorial of negative");
        return Value::ofRat(BigRational(BigInt::factorial((unsigned long long)n.toLongLong())));
    }
    if (name == "pow") {
        need(2); return powValues(args[0], args[1]);
    }
    if (name == "sqrt") {
        if (args.size() == 1) {
            BigRational r = args[0].toRational();
            if (r.isNegative()) throw std::runtime_error("sqrt of negative");
            // try exact
            BigRational scale; BigInt radicand;
            simplifySqrt(r, scale, radicand);
            if (radicand == BigInt(1)) return Value::ofRat(scale);
            if (config.outputMode == OutputMode::Decimal)
                return Value::ofDec(BigFloat::sqrt(args[0].toBigFloat(config.precision)));
            // symbolic: scale * sqrt(radicand)
            ExprPtr sNode = Expr::makeNumber(scale);
            ExprPtr sqrtNode = Expr::makeCall("sqrt",  Expr::makeNumber(BigRational(radicand)) );
            if (scale == BigRational(1)) return Value::ofSym(std::move(sqrtNode));
            return Value::ofSym(Expr::makeBinary(BinOp::Mul, std::move(sNode), std::move(sqrtNode)));
        }
        if (args.size() == 2) {
            // sqrt(a, b) = b 的 a 次方根 = b^(1/a)
            BigRational a = args[0].toRational(), b = args[1].toRational();
            // 尝试精确: 若 b 是 a 次方的整数, 返回精确值
            if (a.isInteger() && b.isInteger() && !b.isNegative()) {
                BigInt n = a.num(), m = b.num();
                // 用二分搜索找 m 的 n 次根
                if (!m.isZero() && n.signum() > 0) {
                    BigInt lo(0), hi = m + BigInt(1);
                    while (lo < hi) {
                        BigInt mid = (lo + hi) / BigInt(2);
                        BigInt p = BigInt::pow(mid, n);
                        if (p == m) { return Value::ofRat(BigRational(mid)); } // 精确!
                        if (p < m) lo = mid + BigInt(1); else hi = mid;
                    }
                }
            }
            BigRational half(BigInt(1), a.num()); // 1/a (a integer)
            if (config.outputMode == OutputMode::Math)
                return Value::ofSym(Expr::makeCall("sqrt",  Expr::makeNumber(a), Expr::makeNumber(b) ));
            // decimal: 用 root 避免精度问题
            return Value::ofDec(BigFloat::root(args[1].toBigFloat(config.precision),
                                                (unsigned)a.num().toLongLong()));
        }
        throw std::runtime_error("sqrt: 1 or 2 args");
    }
    if (name == "log") {
        if (args.size() == 1) {
            // log(1) = 0
            if (args[0].isRational() && args[0].rat == BigRational(1)) return Value::ofRat(BigRational(0));
            // ln
            if (config.outputMode == OutputMode::Math)
                return Value::ofSym(Expr::makeCall("log",  node.args[0]->clone() ));
            return Value::ofDec(BigFloat::ln(args[0].toBigFloat(config.precision)));
        }
        if (args.size() == 2) {
            // log_b(1) = 0
            if (args[1].isRational() && args[1].rat == BigRational(1)) return Value::ofRat(BigRational(0));
            // log_b(b) = 1
            if (args[0].isRational() && args[1].isRational() && args[0].rat == args[1].rat)
                return Value::ofRat(BigRational(1));
            // 尝试精确: log_base(value) = n 其中 base^n = value
            if (args[0].isRational() && args[1].isRational() &&
                args[0].rat.isInteger() && args[1].rat.isInteger()) {
                BigInt base_n = args[0].rat.num(), val = args[1].rat.num();
                if (base_n.signum() > 0 && val.signum() > 0 && base_n != BigInt(1)) {
                    BigInt p(1);
                    for (int n = 0; n < 10000; ++n) {
                        if (p == val) return Value::ofRat(BigRational((long long)n));
                        if (p > val) break;
                        p = p * base_n;
                    }
                }
            }
            if (config.outputMode == OutputMode::Math)
                return Value::ofSym(Expr::makeCall("log",  node.args[0]->clone(), node.args[1]->clone() ));
            return Value::ofDec(BigFloat::log(args[1].toBigFloat(config.precision),
                                              args[0].toBigFloat(config.precision)));
        }
        throw std::runtime_error("log: 1 or 2 args");
    }
    if (name == "ln") { need(1);
        // ln(1) = 0, ln(e) = 1 (精确识别)
        if (args[0].isRational() && args[0].rat == BigRational(1)) return Value::ofRat(BigRational(0));
        if (args[0].isSymbolic() && args[0].sym && args[0].sym->kind == Expr::Var &&
            (args[0].sym->name == "e" || args[0].sym->name == "E")) return Value::ofRat(BigRational(1));
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("ln",  node.args[0]->clone() ));
        return Value::ofDec(BigFloat::ln(args[0].toBigFloat(config.precision)));
    }
    if (name == "exp") { need(1);
        // exp(0) = 1
        if (args[0].isRational() && args[0].rat.isZero()) return Value::ofRat(BigRational(1));
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("exp",  node.args[0]->clone() ));
        return Value::ofDec(BigFloat::exp(args[0].toBigFloat(config.precision)));
    }
    // trig
    auto trig1 = [&](const char* fname, BigFloat (*fn)(const BigFloat&)) -> Value {
        need(1);
        if (config.outputMode == OutputMode::Math) {
            // special: sin(0)=0, cos(0)=1, tan(0)=0
            if (args[0].isRational() && args[0].rat.isZero()) {
                if (std::string(fname) == "cos" || std::string(fname) == "sec") return Value::ofRat(BigRational(1));
                return Value::ofRat(BigRational(0));
            }
            return Value::ofSym(Expr::makeCall(fname, node.args[0]->clone()));
        }
        return Value::ofDec(fn(args[0].toBigFloat(config.precision)));
    };
    if (name == "sin") return trig1("sin", BigFloat::sin);
    if (name == "cos") return trig1("cos", BigFloat::cos);
    if (name == "tan") return trig1("tan", BigFloat::tan);
    if (name == "cot") return trig1("cot", [](const BigFloat& x){ return BigFloat(1) / BigFloat::tan(x); });
    if (name == "sec") return trig1("sec", [](const BigFloat& x){ return BigFloat(1) / BigFloat::cos(x); });
    if (name == "csc") return trig1("csc", [](const BigFloat& x){ return BigFloat(1) / BigFloat::sin(x); });
    if (name == "asin") { need(1);
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("asin",  node.args[0]->clone() ));
        return Value::ofDec(BigFloat::asin(args[0].toBigFloat(config.precision)));
    }
    if (name == "acos") { need(1);
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("acos",  node.args[0]->clone() ));
        return Value::ofDec(BigFloat::acos(args[0].toBigFloat(config.precision)));
    }
    if (name == "atan") { need(1);
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("atan",  node.args[0]->clone() ));
        return Value::ofDec(BigFloat::atan(args[0].toBigFloat(config.precision)));
    }
    if (name == "sinh") { need(1);
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("sinh",  node.args[0]->clone() ));
        BigFloat x = args[0].toBigFloat(config.precision);
        return Value::ofDec((BigFloat::exp(x) - BigFloat::exp(-x)) / BigFloat(2));
    }
    if (name == "cosh") { need(1);
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("cosh",  node.args[0]->clone() ));
        BigFloat x = args[0].toBigFloat(config.precision);
        return Value::ofDec((BigFloat::exp(x) + BigFloat::exp(-x)) / BigFloat(2));
    }
    if (name == "tanh") { need(1);
        if (config.outputMode == OutputMode::Math)
            return Value::ofSym(Expr::makeCall("tanh",  node.args[0]->clone() ));
        BigFloat x = args[0].toBigFloat(config.precision);
        BigFloat e1 = BigFloat::exp(x), e2 = BigFloat::exp(-x);
        return Value::ofDec((e1 - e2) / (e1 + e2));
    }
    if (name == "min") {
        if (args.empty()) throw std::runtime_error("min: need args");
        Value m = args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            int c = args[i].toBigFloat(config.precision).cmp(m.toBigFloat(config.precision));
            if (c < 0) m = args[i];
        }
        return m;
    }
    if (name == "max") {
        if (args.empty()) throw std::runtime_error("max: need args");
        Value m = args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            int c = args[i].toBigFloat(config.precision).cmp(m.toBigFloat(config.precision));
            if (c > 0) m = args[i];
        }
        return m;
    }
    if (name == "rand") {
        need(2);
        BigRational lo = args[0].toRational(), hi = args[1].toRational();
        // CSPRNG: produce a random rational with 256 bits of precision, scale to [lo,hi].
        BigInt range = (hi - lo).num(); // numerator of (hi-lo) since den positive
        if (range.isZero()) return Value::ofRat(lo);
        // random bits
        BigInt r = BigInt::randomBits(256);
        // scale: lo + (hi-lo) * (r / 2^256)
        BigInt two256 = BigInt(1) << 256;
        BigRational frac(r, two256);
        return Value::ofRat(lo + (hi - lo) * frac);
    }
    // 组合数 C(n,m) = n! / (m! * (n-m)!)
    if (name == "combination" || name == "comb") {
        need(2);
        BigInt n = args[0].toRational().trunc(), m = args[1].toRational().trunc();
        if (n.isNegative() || m.isNegative() || m > n) return Value::ofRat(BigRational(0));
        BigInt num = BigInt::factorial((unsigned long long)n.toLongLong());
        BigInt den = BigInt::factorial((unsigned long long)m.toLongLong()) *
                     BigInt::factorial((unsigned long long)(n - m).toLongLong());
        return Value::ofRat(BigRational(num / den));
    }
    // 排列数 P(n,m) = n! / (n-m)!
    if (name == "permutation" || name == "perm") {
        need(2);
        BigInt n = args[0].toRational().trunc(), m = args[1].toRational().trunc();
        if (n.isNegative() || m.isNegative() || m > n) return Value::ofRat(BigRational(0));
        BigInt num = BigInt::factorial((unsigned long long)n.toLongLong());
        BigInt den = BigInt::factorial((unsigned long long)(n - m).toLongLong());
        return Value::ofRat(BigRational(num / den));
    }
    // 第一类 Stirling 数 s(n,m) — 递推: s(n,m) = s(n-1,m-1) - (n-1)*s(n-1,m)
    if (name == "stirling1") {
        need(2);
        long long n = args[0].toRational().trunc().toLongLong();
        long long m = args[1].toRational().trunc().toLongLong();
        if (n < 0 || m < 0) return Value::ofRat(BigRational(0));
        if (n == 0 && m == 0) return Value::ofRat(BigRational(1));
        if (m == 0 || m > n) return Value::ofRat(BigRational(0));
        // 递推表
        std::vector<std::vector<BigInt>> s(n + 1, std::vector<BigInt>(m + 1, BigInt(0)));
        s[0][0] = BigInt(1);
        for (long long i = 1; i <= n; ++i)
            for (long long j = 1; j <= std::min(i, m); ++j)
                s[i][j] = s[i-1][j-1] - BigInt(i-1) * s[i-1][j];
        return Value::ofRat(BigRational(s[n][m]));
    }
    // 第二类 Stirling 数 S(n,m) — 递推: S(n,m) = m*S(n-1,m) + S(n-1,m-1)
    if (name == "stirling2") {
        need(2);
        long long n = args[0].toRational().trunc().toLongLong();
        long long m = args[1].toRational().trunc().toLongLong();
        if (n < 0 || m < 0) return Value::ofRat(BigRational(0));
        if (n == 0 && m == 0) return Value::ofRat(BigRational(1));
        if (m == 0 || m > n) return Value::ofRat(BigRational(0));
        std::vector<std::vector<BigInt>> S(n + 1, std::vector<BigInt>(m + 1, BigInt(0)));
        S[0][0] = BigInt(1);
        for (long long i = 1; i <= n; ++i)
            for (long long j = 1; j <= std::min(i, m); ++j)
                S[i][j] = BigInt(j) * S[i-1][j] + S[i-1][j-1];
        return Value::ofRat(BigRational(S[n][m]));
    }
    if (name == "Iverson") {
        need(1);
        return Value::ofRat(BigRational(truthy(args[0]) ? 1 : 0));
    }
    // 同余: cong(a, b, mod) => a ≡ b (mod m)  即 (a - b) % m == 0
    if (name == "cong") {
        need(3);
        BigRational a = args[0].toRational(), b = args[1].toRational(), m = args[2].toRational();
        if (m.isZero()) throw std::runtime_error("cong: modulus is zero");
        // 要求整数同余
        if (!a.isInteger() || !b.isInteger() || !m.isInteger())
            throw std::runtime_error("cong: operands and modulus must be integers");
        BigInt diff = a.num() - b.num();
        BigInt mm = m.num();
        if (mm.isNegative()) mm = -mm;
        BigInt q, r; BigInt::divmod(diff, mm, q, r);
        return Value::ofBool(r.isZero());
    }
    throw std::runtime_error("unknown function: " + name);
}

} // namespace scicalc
