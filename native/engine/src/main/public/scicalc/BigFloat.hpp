// scicalc/BigFloat.hpp — Arbitrary-precision decimal floating point.
// Used for decimal-mode evaluation of transcendental functions (sin, cos, log,
// exp, ...). Exact rationals (BigRational) are preferred wherever possible.
//
// Representation: value = mantissa * 10^exp, where mantissa is a BigInt with
// exactly `prec` significant decimal digits (or fewer for exact small values).
#pragma once
#include "scicalc/BigInt.hpp"
#include "scicalc/BigRational.hpp"
#include <string>

namespace scicalc {

class BigFloat {
public:
    /// Working precision in significant decimal digits.
    static unsigned& defaultPrecision();

    BigFloat() = default;                       // 0
    BigFloat(long long v);
    BigFloat(const BigInt& v) : mant_(v), exp_(0), prec_(defaultPrecision()) {}
    BigFloat(const BigInt& m, long long e) : mant_(m), exp_(e) {}
    static BigFloat fromRational(const BigRational& r, unsigned prec = 0);
    static BigFloat fromDecimal(const std::string& s, unsigned prec = 0);
    static BigFloat pi(unsigned prec = 0);
    static BigFloat e(unsigned prec = 0);

    bool isZero() const { return mant_.isZero(); }
    bool isNegative() const { return mant_.isNegative(); }
    int signum() const { return mant_.signum(); }
    const BigInt& mantissa() const { return mant_; }
    long long exponent() const { return exp_; }
    unsigned precision() const { return prec_; }

    BigFloat operator-() const { return BigFloat(-mant_, exp_, prec_); }
    BigFloat operator+(const BigFloat& o) const;
    BigFloat operator-(const BigFloat& o) const;
    BigFloat operator*(const BigFloat& o) const;
    BigFloat operator/(const BigFloat& o) const;
    bool operator<(const BigFloat& o) const;
    bool operator==(const BigFloat& o) const;

    /// Round to `prec` significant digits.
    BigFloat roundTo(unsigned prec) const;

    /// Square root (Newton's method).
    static BigFloat sqrt(const BigFloat& x);
    /// n-th root via Newton.
    static BigFloat root(const BigFloat& x, unsigned n);
    /// exp(x) via Taylor + argument reduction.
    static BigFloat exp(const BigFloat& x);
    /// ln(x) via atanh series.
    static BigFloat ln(const BigFloat& x);
    /// log base b of x = ln(x)/ln(b).
    static BigFloat log(const BigFloat& x, const BigFloat& base);
    /// sin, cos, tan via Taylor with range reduction.
    static BigFloat sin(const BigFloat& x);
    static BigFloat cos(const BigFloat& x);
    static BigFloat tan(const BigFloat& x);
    /// asin, acos, atan via series.
    static BigFloat asin(const BigFloat& x);
    static BigFloat acos(const BigFloat& x);
    static BigFloat atan(const BigFloat& x);
    /// power x^y = exp(y*ln(x)) for general y; exact path for integer y.
    static BigFloat pow(const BigFloat& x, const BigFloat& y);

    /// Decimal string with `prec` significant digits (or default).
    std::string toString(unsigned prec = 0) const;
    /// Convert to nearest BigRational (denominator a power of 10).
    BigRational toRational() const;

    int cmp(const BigFloat& o) const;

private:
    BigInt mant_;
    long long exp_ = 0;
    unsigned prec_ = 0;
    BigFloat(const BigInt& m, long long e, unsigned p) : mant_(m), exp_(e), prec_(p) {}
    void roundInPlace(unsigned prec);
};

} // namespace scicalc
