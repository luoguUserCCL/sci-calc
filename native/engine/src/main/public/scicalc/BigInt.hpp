// scicalc/BigInt.hpp — Arbitrary-precision signed integer (exact arithmetic).
// Part of the sci-calc engine. No floating point is ever used here.
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <compare>
#include <iosfwd>

namespace scicalc {

/// Arbitrary-precision signed integer.
/// Magnitude stored as little-endian base-2^32 limbs; sign tracked separately.
/// Zero is always represented with an empty magnitude and sign == 0 (positive).
class BigInt {
public:
    using Limb = uint32_t;
    static constexpr uint64_t LIMB_BASE = uint64_t(1) << 32;

    BigInt() = default;                       // zero
    BigInt(long long v);
    BigInt(unsigned long long v);
    BigInt(int v) : BigInt(static_cast<long long>(v)) {}
    /// Parse from decimal (or base-2..36) string. Throws std::invalid_argument.
    explicit BigInt(const std::string& s, int base = 10);

    static BigInt fromU64(uint64_t v);
    static BigInt pow(const BigInt& base, const BigInt& exp);   ///< integer power (exp>=0)
    static BigInt powMod(const BigInt& b, const BigInt& e, const BigInt& m);
    static BigInt gcd(BigInt a, BigInt b);
    static BigInt lcm(const BigInt& a, const BigInt& b);
    static BigInt factorial(unsigned long long n);
    static BigInt randomBits(unsigned bits);   ///< CSPRNG (arc4random-style via /dev/urandom)
    /// Fast modular exponentiation is in powMod; pow() with integer exp uses
    /// exponentiation-by-squaring (fast power) internally.
    static BigInt abs(const BigInt& a) { BigInt r = a; r.sign_ = 1; return r; }

    bool isZero() const { return mag_.empty(); }
    bool isNegative() const { return sign_ < 0 && !mag_.empty(); }
    bool isOdd() const { return !mag_.empty() && (mag_[0] & 1u); }
    bool isEven() const { return !isOdd(); }
    int  signum() const { return isZero() ? 0 : sign_; }
    /// Number of bits (0 for zero).
    size_t bitLength() const;
    /// True if value fits in long long.
    bool fitsLongLong() const;

    long long toLongLong() const;
    std::string toString(int base = 10) const;
    /// Hex/oct/bin with optional prefix and grouping.
    std::string toBaseString(int base, bool prefix = true) const;

    BigInt operator-() const;
    BigInt& operator+=(const BigInt& o);
    BigInt& operator-=(const BigInt& o);
    BigInt& operator*=(const BigInt& o);
    BigInt& operator/=(const BigInt& o);
    BigInt& operator%=(const BigInt& o);
    BigInt& operator<<=(unsigned n);
    BigInt& operator>>=(unsigned n);

    friend BigInt operator+(BigInt a, const BigInt& b) { return a += b; }
    friend BigInt operator-(BigInt a, const BigInt& b) { return a -= b; }
    friend BigInt operator*(BigInt a, const BigInt& b) { return a *= b; }
    friend BigInt operator/(BigInt a, const BigInt& b) { return a /= b; }
    friend BigInt operator%(BigInt a, const BigInt& b) { return a %= b; }
    friend BigInt operator<<(BigInt a, unsigned n) { return a <<= n; }
    friend BigInt operator>>(BigInt a, unsigned n) { return a >>= n; }

    friend bool operator==(const BigInt&, const BigInt&);
    friend std::strong_ordering operator<=>(const BigInt&, const BigInt&);

    // Division returning quotient and remainder (remainder has sign of dividend).
    static void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r);

    // Bit access
    bool getBit(size_t i) const;
    void setBit(size_t i, bool v);

    const std::vector<Limb>& limbs() const { return mag_; }
    int rawSign() const { return sign_; }

private:
    std::vector<Limb> mag_;   // little-endian, no leading zero limbs
    int sign_ = 1;            // +1 or -1; for zero, mag_ empty, sign_ irrelevant

    void normalize();
    void addMag(const std::vector<Limb>& a, const std::vector<Limb>& b, std::vector<Limb>& out) const;
    void subMag(const std::vector<Limb>& a, const std::vector<Limb>& b, std::vector<Limb>& out) const; // a>=b
    int  cmpMag(const std::vector<Limb>& a, const std::vector<Limb>& b) const;
    void mulMag(const std::vector<Limb>& a, const std::vector<Limb>& b, std::vector<Limb>& out) const;
};

std::ostream& operator<<(std::ostream& os, const BigInt& v);

} // namespace scicalc
