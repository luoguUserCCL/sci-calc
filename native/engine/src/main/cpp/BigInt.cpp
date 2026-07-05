// scicalc/BigInt.cpp — implementation
#include "scicalc/BigInt.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <random>

namespace scicalc {

// ---------------------------------------------------------------------------
// Construction / normalization
// ---------------------------------------------------------------------------

BigInt::BigInt(long long v) {
    if (v == 0) { sign_ = 1; return; }
    bool neg = v < 0;
    unsigned long long u = neg ? (unsigned long long)(-(v + 1)) + 1ULL
                               : (unsigned long long)v;
    while (u) { mag_.push_back((Limb)(u & 0xFFFFFFFFu)); u >>= 32; }
    sign_ = neg ? -1 : 1;
}

BigInt::BigInt(unsigned long long v) {
    sign_ = 1;
    while (v) { mag_.push_back((Limb)(v & 0xFFFFFFFFu)); v >>= 32; }
}

BigInt BigInt::fromU64(uint64_t v) { return BigInt((unsigned long long)v); }

void BigInt::normalize() {
    while (!mag_.empty() && mag_.back() == 0) mag_.pop_back();
    if (mag_.empty()) sign_ = 1;
}

BigInt::BigInt(const std::string& s, int base) : sign_(1) {
    if (s.empty()) throw std::invalid_argument("BigInt: empty string");
    size_t i = 0;
    if (s[0] == '+') i = 1;
    else if (s[0] == '-') { sign_ = -1; i = 1; }
    // optional 0x / 0b / 0o prefix when base==0 or matches
    if (base == 0 || base == 16) {
        if (i + 1 < s.size() && s[i] == '0' && (s[i+1] == 'x' || s[i+1] == 'X')) { base = 16; i += 2; }
    }
    if (base == 0 || base == 2) {
        if (i + 1 < s.size() && s[i] == '0' && (s[i+1] == 'b' || s[i+1] == 'B')) { base = 2; i += 2; }
    }
    if (base == 0) base = 10;
    if (base < 2 || base > 36) throw std::invalid_argument("BigInt: bad base");

    BigInt result;
    BigInt bbase((long long)base);
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\'') continue; // digit separator
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'Z') d = 10 + (c - 'A');
        else throw std::invalid_argument(std::string("BigInt: bad digit '") + c + "'");
        if (d >= base) throw std::invalid_argument("BigInt: digit >= base");
        result = result * bbase + BigInt((long long)d);
    }
    mag_ = std::move(result.mag_);
    normalize();
}

// ---------------------------------------------------------------------------
// Magnitude helpers
// ---------------------------------------------------------------------------

int BigInt::cmpMag(const std::vector<Limb>& a, const std::vector<Limb>& b) const {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0;)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

void BigInt::addMag(const std::vector<Limb>& a, const std::vector<Limb>& b,
                    std::vector<Limb>& out) const {
    size_t n = std::max(a.size(), b.size());
    out.assign(n, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t s = carry;
        if (i < a.size()) s += a[i];
        if (i < b.size()) s += b[i];
        out[i] = (Limb)(s & 0xFFFFFFFFu);
        carry = s >> 32;
    }
    if (carry) out.push_back((Limb)carry);
}

void BigInt::subMag(const std::vector<Limb>& a, const std::vector<Limb>& b,
                    std::vector<Limb>& out) const { // requires |a| >= |b|
    out.assign(a.size(), 0);
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t s = (int64_t)a[i] - borrow - (i < b.size() ? (int64_t)b[i] : 0);
        if (s < 0) { s += (int64_t)LIMB_BASE; borrow = 1; } else borrow = 0;
        out[i] = (Limb)s;
    }
    while (!out.empty() && out.back() == 0) out.pop_back();
}

void BigInt::mulMag(const std::vector<Limb>& a, const std::vector<Limb>& b,
                    std::vector<Limb>& out) const {
    if (a.empty() || b.empty()) { out.clear(); return; }
    out.assign(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        uint64_t ai = a[i];
        for (size_t j = 0; j < b.size(); ++j) {
            uint64_t cur = out[i + j] + ai * b[j] + carry;
            out[i + j] = (Limb)(cur & 0xFFFFFFFFu);
            carry = cur >> 32;
        }
        out[i + b.size()] += (Limb)carry;
    }
    while (!out.empty() && out.back() == 0) out.pop_back();
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

BigInt& BigInt::operator+=(const BigInt& o) {
    if (o.isZero()) return *this;
    if (isZero()) { *this = o; return *this; }
    std::vector<Limb> out;
    if (sign_ == o.sign_) {
        addMag(mag_, o.mag_, out);
        mag_ = std::move(out);
    } else {
        int c = cmpMag(mag_, o.mag_);
        if (c == 0) { mag_.clear(); sign_ = 1; }
        else if (c > 0) { subMag(mag_, o.mag_, out); mag_ = std::move(out); }
        else { subMag(o.mag_, mag_, out); mag_ = std::move(out); sign_ = o.sign_; }
    }
    normalize();
    return *this;
}

BigInt& BigInt::operator-=(const BigInt& o) {
    if (o.isZero()) return *this;
    if (isZero()) { *this = -o; return *this; }
    std::vector<Limb> out;
    if (sign_ != o.sign_) {
        addMag(mag_, o.mag_, out);
        mag_ = std::move(out);
    } else {
        int c = cmpMag(mag_, o.mag_);
        if (c == 0) { mag_.clear(); sign_ = 1; }
        else if (c > 0) { subMag(mag_, o.mag_, out); mag_ = std::move(out); }
        else { subMag(o.mag_, mag_, out); mag_ = std::move(out); sign_ = -sign_; }
    }
    normalize();
    return *this;
}

BigInt BigInt::operator-() const {
    BigInt r = *this;
    if (!r.isZero()) r.sign_ = -r.sign_;
    return r;
}

BigInt& BigInt::operator*=(const BigInt& o) {
    if (isZero() || o.isZero()) { mag_.clear(); sign_ = 1; return *this; }
    std::vector<Limb> out;
    mulMag(mag_, o.mag_, out);
    mag_ = std::move(out);
    sign_ = sign_ * o.sign_;
    normalize();
    return *this;
}

void BigInt::divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
    if (b.isZero()) throw std::invalid_argument("BigInt: division by zero");
    if (a.isZero()) { q.mag_.clear(); q.sign_ = 1; r.mag_.clear(); r.sign_ = 1; return; }
    int c = a.cmpMag(a.mag_, b.mag_);
    if (c < 0) { // |a| < |b|  -> q=0, r=a
        q.mag_.clear(); q.sign_ = 1;
        r = a;
        return;
    }
    if (c == 0) {
        q.mag_.assign(1, 1); q.sign_ = a.sign_ * b.sign_;
        r.mag_.clear(); r.sign_ = 1;
        return;
    }
    // Long division in base 2^32.
    const auto& na = a.mag_;
    const auto& nb = b.mag_;
    std::vector<Limb> rem(na.size(), 0);
    std::vector<Limb> quo(na.size(), 0);
    // Knuth Algorithm D (simplified, base 2^32). Normalize divisor.
    std::vector<Limb> u = na, v = nb;
    uint64_t d = LIMB_BASE / (uint64_t(v.back()) + 1);
    if (d != 1) {
        // u = u * d  (one extra limb)
        u.push_back(0);
        uint64_t carry = 0;
        for (size_t i = 0; i < u.size(); ++i) { uint64_t s = u[i]*d + carry; u[i] = (Limb)s; carry = s>>32; }
        // v = v * d
        carry = 0;
        for (size_t i = 0; i < v.size(); ++i) { uint64_t s = v[i]*d + carry; v[i] = (Limb)s; carry = s>>32; }
    } else {
        u.push_back(0);
    }
    size_t n = v.size();
    size_t m = u.size() - n - 1; // u has m+n+1 limbs (top may be 0)
    for (size_t j = m + 1; j-- > 0; ) {
        // estimate qhat
        uint64_t high = (uint64_t(u[j + n]) << 32) | u[j + n - 1];
        uint64_t qhat = high / v[n - 1];
        uint64_t rhat = high % v[n - 1];
        while (qhat >= LIMB_BASE ||
               (n >= 2 && qhat * v[n - 2] > ((rhat << 32) | u[j + n - 2]))) {
            --qhat; rhat += v[n - 1];
            if (rhat >= LIMB_BASE) break;
        }
        // u -= qhat * v shifted by j
        int64_t borrow = 0; uint64_t carry = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t prod = qhat * v[i] + carry;
            carry = prod >> 32;
            int64_t s = (int64_t)u[j + i] - (int64_t)(prod & 0xFFFFFFFFu) - borrow;
            if (s < 0) { s += (int64_t)LIMB_BASE; borrow = 1; } else borrow = 0;
            u[j + i] = (Limb)s;
        }
        int64_t s = (int64_t)u[j + n] - (int64_t)carry - borrow;
        if (s < 0) {
            // add back
            s += (int64_t)LIMB_BASE;
            u[j + n] = (Limb)s;
            uint64_t c2 = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t sum = u[j + i] + v[i] + c2;
                u[j + i] = (Limb)sum; c2 = sum >> 32;
            }
            u[j + n] += (Limb)c2;
            --qhat;
        } else {
            u[j + n] = (Limb)s;
        }
        quo[j] = (Limb)qhat;
    }
    // remainder = u / d  (low n limbs of u)
    std::vector<Limb> rrem(u.begin(), u.begin() + n);
    q.mag_ = std::move(quo);
    q.sign_ = a.sign_ * b.sign_;
    q.normalize();
    if (d != 1) {
        // divide rrem by d
        uint64_t rem2 = 0;
        for (size_t i = rrem.size(); i-- > 0;) {
            uint64_t cur = (rem2 << 32) | rrem[i];
            rrem[i] = (Limb)(cur / d);
            rem2 = cur % d;
        }
    }
    r.mag_ = std::move(rrem);
    r.sign_ = a.sign_; // remainder follows dividend sign
    r.normalize();
    // ensure 0 <= |r| < |b|
    (void)rem;
}

BigInt& BigInt::operator/=(const BigInt& o) {
    BigInt q, r; divmod(*this, o, q, r); *this = q; return *this;
}
BigInt& BigInt::operator%=(const BigInt& o) {
    BigInt q, r; divmod(*this, o, q, r); *this = r; return *this;
}

BigInt& BigInt::operator<<=(unsigned n) {
    if (isZero() || n == 0) return *this;
    unsigned limbs = n / 32; unsigned bits = n % 32;
    if (limbs) mag_.insert(mag_.begin(), limbs, 0);
    if (bits) {
        uint64_t carry = 0;
        for (auto& x : mag_) { uint64_t v = ((uint64_t)x << bits) | carry; x = (Limb)v; carry = v >> 32; }
        if (carry) mag_.push_back((Limb)carry);
    }
    normalize();
    return *this;
}
BigInt& BigInt::operator>>=(unsigned n) {
    if (isZero() || n == 0) return *this;
    unsigned limbs = n / 32; unsigned bits = n % 32;
    if (limbs >= mag_.size()) { mag_.clear(); sign_ = 1; return *this; }
    mag_.erase(mag_.begin(), mag_.begin() + limbs);
    if (bits) {
        uint64_t carry = 0;
        for (size_t i = mag_.size(); i-- > 0;) {
            uint64_t v = ((uint64_t)carry << 32) | mag_[i];
            mag_[i] = (Limb)(v >> bits);
            carry = v & ((1u << bits) - 1u);
        }
    }
    normalize();
    return *this;
}

bool getBitImpl(const std::vector<BigInt::Limb>& m, size_t i) {
    size_t li = i / 32;
    if (li >= m.size()) return false;
    return (m[li] >> (i % 32)) & 1u;
}

bool BigInt::getBit(size_t i) const { return getBitImpl(mag_, i); }
void BigInt::setBit(size_t i, bool v) {
    size_t li = i / 32;
    if (v) {
        if (li >= mag_.size()) mag_.resize(li + 1, 0);
        mag_[li] |= (1u << (i % 32));
    } else {
        if (li < mag_.size()) mag_[li] &= ~(1u << (i % 32));
        normalize();
    }
}

size_t BigInt::bitLength() const {
    if (mag_.empty()) return 0;
    Limb top = mag_.back();
    size_t b = 0;
    while (top) { ++b; top >>= 1; }
    return (mag_.size() - 1) * 32 + b;
}

bool BigInt::fitsLongLong() const {
    if (mag_.empty()) return true;
    if (mag_.size() > 2) return false;
    if (mag_.size() == 1) return true;
    // 2 limbs: must fit in 63 bits (signed)
    if (mag_[1] >> 31) return false;
    return true;
}

long long BigInt::toLongLong() const {
    uint64_t v = 0;
    if (!mag_.empty()) v = mag_[0];
    if (mag_.size() > 1) v |= (uint64_t)mag_[1] << 32;
    if (sign_ < 0) return -(int64_t)v;
    return (long long)v;
}

// ---------------------------------------------------------------------------
// to string
// ---------------------------------------------------------------------------

std::string BigInt::toString(int base) const {
    if (isZero()) return "0";
    if (base < 2 || base > 36) base = 10;
    std::string out;
    if (base == 16 || base == 8 || base == 2) return toBaseString(base, false);
    // generic: repeated divmod by base
    std::vector<Limb> cur = mag_;
    BigInt bbase((long long)base);
    // Convert via base-2^32 limbs repeatedly dividing by base (small)
    while (!cur.empty()) {
        uint64_t rem = 0;
        for (size_t i = cur.size(); i-- > 0;) {
            uint64_t cur2 = (rem << 32) | cur[i];
            cur[i] = (Limb)(cur2 / base);
            rem = cur2 % base;
        }
        while (!cur.empty() && cur.back() == 0) cur.pop_back();
        char d = (char)(rem < 10 ? '0' + rem : 'a' + (rem - 10));
        out.push_back(d);
    }
    if (sign_ < 0) out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}

std::string BigInt::toBaseString(int base, bool prefix) const {
    if (isZero()) return "0";
    // Use bit grouping for 2/8/16
    std::string bits;
    for (size_t i = bitLength(); i-- > 0;) bits.push_back(getBit(i) ? '1' : '0');
    std::string out;
    if (base == 2) {
        out = bits;
    } else {
        int g = (base == 8) ? 3 : 4;
        // pad
        while (bits.size() % g) bits = "0" + bits;
        const char* digs = "0123456789abcdef";
        for (size_t i = 0; i < bits.size(); i += g) {
            int v = 0; for (int k = 0; k < g; ++k) v = (v << 1) + (bits[i+k]-'0');
            out.push_back(digs[v]);
        }
        size_t p = out.find_first_not_of('0');
        if (p != std::string::npos) out.erase(0, p); else out = "0";
    }
    std::string pre;
    if (prefix) pre = (base == 2) ? "0b" : (base == 8) ? "0o" : "0x";
    return (sign_ < 0 ? "-" : "") + pre + out;
}

// ---------------------------------------------------------------------------
// Number theory
// ---------------------------------------------------------------------------

BigInt BigInt::gcd(BigInt a, BigInt b) {
    a.sign_ = 1; b.sign_ = 1;
    while (!b.isZero()) { BigInt q, r; divmod(a, b, q, r); a = b; b = r; }
    return a;
}
BigInt BigInt::lcm(const BigInt& a, const BigInt& b) {
    if (a.isZero() || b.isZero()) return BigInt(0);
    return abs(a * (b / gcd(a, b)));
}

BigInt BigInt::pow(const BigInt& base, const BigInt& exp) {
    if (exp.isNegative()) {
        if (base == BigInt(1)) return BigInt(1);
        if (base == BigInt(-1)) return exp.isOdd() ? BigInt(-1) : BigInt(1);
        return BigInt(0); // 1/base^n -> 0 for integers
    }
    BigInt result(1), b = base; BigInt e = exp;
    while (!e.isZero()) {
        if (e.isOdd()) result *= b;
        b *= b;
        e >>= 1; // floor division by 2 (e >= 0)
    }
    return result;
}

BigInt BigInt::powMod(const BigInt& b, const BigInt& e, const BigInt& m) {
    if (m.isZero()) throw std::invalid_argument("powMod: mod 0");
    if (m == BigInt(1)) return BigInt(0);
    BigInt result(1), base = b % m, ex = e;
    while (!ex.isZero()) {
        if (ex.isOdd()) result = (result * base) % m;
        base = (base * base) % m;
        ex >>= 1;
    }
    return result;
}

BigInt BigInt::factorial(unsigned long long n) {
    BigInt r(1);
    for (unsigned long long i = 2; i <= n; ++i) r *= BigInt((unsigned long long)i);
    return r;
}

BigInt BigInt::randomBits(unsigned bits) {
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom) {
        // fallback to mt19937 seeded from random_device
        std::random_device rd;
        std::mt19937 g(rd());
        BigInt r(0);
        for (unsigned i = 0; i < bits; ++i) { r <<= 1; if (g() & 1) r += BigInt(1); }
        return r;
    }
    std::vector<uint8_t> buf((bits + 7) / 8);
    urandom.read((char*)buf.data(), buf.size());
    BigInt r(0);
    for (uint8_t b : buf) { r <<= 8; r += BigInt((unsigned long long)b); }
    if (bits) r >>= (buf.size() * 8 - bits);
    return r;
}

bool operator==(const BigInt& a, const BigInt& b) {
    return a.signum() == b.signum() && a.cmpMag(a.mag_, b.mag_) == 0;
}
std::strong_ordering operator<=>(const BigInt& a, const BigInt& b) {
    int sa = a.signum(), sb = b.signum();
    if (sa != sb) return sa < sb ? std::strong_ordering::less : std::strong_ordering::greater;
    int c = a.cmpMag(a.mag_, b.mag_);
    if (sa < 0) c = -c;
    if (c < 0) return std::strong_ordering::less;
    if (c > 0) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}

std::ostream& operator<<(std::ostream& os, const BigInt& v) { return os << v.toString(); }

} // namespace scicalc
