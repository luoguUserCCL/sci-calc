// scicalc/BigRational.hpp — Exact rational number (numerator/denominator).
// Always stored in lowest terms with a positive denominator.
#pragma once
#include "scicalc/BigInt.hpp"
#include <string>

namespace scicalc {

class BigRational {
public:
    BigRational() = default;                       // 0/1
    BigRational(long long n) : num_(n), den_(1) {}
    BigRational(const BigInt& n) : num_(n), den_(BigInt(1)) {}
    BigRational(const BigInt& n, const BigInt& d) : num_(n), den_(d) { normalize(); }

    static BigRational fromDecimal(const std::string& s); // parse "1.5" / "1.5e3"
    static BigRational fromBase(const std::string& s, int base);

    const BigInt& num() const { return num_; }
    const BigInt& den() const { return den_; }

    bool isZero() const { return num_.isZero(); }
    bool isInteger() const { return den_ == BigInt(1); }
    bool isNegative() const { return num_.isNegative(); }
    int signum() const { return num_.signum(); }

    BigRational operator-() const { return BigRational(-num_, den_, NoNormalize{}); }
    BigRational operator+(const BigRational& o) const;
    BigRational operator-(const BigRational& o) const { return *this + (-o); }
    BigRational operator*(const BigRational& o) const;
    BigRational operator/(const BigRational& o) const;
    BigRational reciprocal() const;

    /// Integer power (exp may be negative -> reciprocal). Uses fast exponentiation.
    static BigRational pow(const BigRational& base, const BigInt& exp);

    /// Floor (towards -inf).
    BigInt floor() const;
    /// Ceiling (towards +inf).
    BigInt ceil() const;
    /// Truncation towards zero.
    BigInt trunc() const;
    /// Round to nearest integer (half away from zero).
    BigInt round() const;

    /// Compare with another rational.
    int cmp(const BigRational& o) const;

    /// Decimal string with up to `digits` significant digits; `exactOut` set true
    /// if the value has a terminating decimal representation that was fully printed.
    std::string toDecimal(unsigned digits, bool& exactOut, bool& recurring) const;
    /// Exact fraction string "n/d" (or "n" if integer).
    std::string toFraction() const;
    /// Mixed number "a b/c" when abs>1 and not integer (optional).
    std::string toMixed() const;

    bool operator==(const BigRational& o) const { return cmp(o) == 0; }
    bool operator!=(const BigRational& o) const { return cmp(o) != 0; }
    bool operator<(const BigRational& o) const { return cmp(o) < 0; }
    bool operator>(const BigRational& o) const { return cmp(o) > 0; }
    bool operator<=(const BigRational& o) const { return cmp(o) <= 0; }
    bool operator>=(const BigRational& o) const { return cmp(o) >= 0; }

private:
    BigInt num_, den_{1};
    struct NoNormalize {};
    BigRational(const BigInt& n, const BigInt& d, NoNormalize) : num_(n), den_(d) {}
    void normalize(); // gcd reduction + positive denominator
};

} // namespace scicalc
