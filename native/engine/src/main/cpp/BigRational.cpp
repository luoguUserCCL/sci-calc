// scicalc/BigRational.cpp
#include "scicalc/BigRational.hpp"
#include <stdexcept>

namespace scicalc {

void BigRational::normalize() {
    if (den_.isZero()) throw std::invalid_argument("BigRational: zero denominator");
    if (den_.isNegative()) { num_ = -num_; den_ = -den_; }
    BigInt g = BigInt::gcd(BigInt::abs(num_), den_);
    if (g != BigInt(1) && !g.isZero()) {
        BigInt q, r;
        BigInt::divmod(num_, g, q, r); num_ = q;
        BigInt::divmod(den_, g, q, r); den_ = q;
    }
}

BigRational BigRational::reciprocal() const {
    if (isZero()) throw std::invalid_argument("BigRational: reciprocal of zero");
    return BigRational(den_, num_, NoNormalize{});
}

BigRational BigRational::operator+(const BigRational& o) const {
    // n1/d1 + n2/d2 = (n1*d2 + n2*d1)/(d1*d2)
    return BigRational(num_ * o.den_ + o.num_ * den_, den_ * o.den_);
}
BigRational BigRational::operator*(const BigRational& o) const {
    return BigRational(num_ * o.num_, den_ * o.den_);
}
BigRational BigRational::operator/(const BigRational& o) const {
    if (o.isZero()) throw std::invalid_argument("BigRational: division by zero");
    return BigRational(num_ * o.den_, den_ * o.num_);
}

BigRational BigRational::pow(const BigRational& base, const BigInt& exp) {
    if (exp.isNegative()) {
        if (base.isZero()) throw std::invalid_argument("0 to negative power");
        return pow(base.reciprocal(), -exp);
    }
    // exponentiation by squaring on numerator and denominator
    BigInt resultNum(1), resultDen(1);
    BigInt n = base.num_, d = base.den_, e = exp;
    while (!e.isZero()) {
        if (e.isOdd()) { resultNum *= n; resultDen *= d; }
        n = n * n;  // n^2
        d = d * d;
        e >>= 1;
    }
    return BigRational(resultNum, resultDen);
}

BigInt BigRational::floor() const {
    BigInt q, r;
    BigInt::divmod(num_, den_, q, r);
    if (!r.isZero() && r.isNegative()) q -= BigInt(1); // divmod remainder follows dividend sign
    return q;
}
BigInt BigRational::ceil() const {
    // ceil(x) = -floor(-x)
    return -((-(*this)).floor());
}
BigInt BigRational::trunc() const {
    BigInt q, r;
    BigInt::divmod(num_, den_, q, r);
    return q; // truncates toward zero (remainder has dividend sign)
}
BigInt BigRational::round() const {
    // round half away from zero: 2x floor, then see remainder*2 vs den
    BigInt twice = num_ * BigInt(2);
    BigInt q, r;
    BigInt::divmod(twice, den_, q, r);
    // q = trunc(2x) = 2*trunc(x) + adjustment. Compare |r| vs den/2.
    // Simpler: floor(x + 1/2) for positive; for negative use -floor(-x+1/2).
    // Use: round = floor(x + sign(x)/2)
    BigRational half(BigInt(1), BigInt(2));
    if (!isNegative()) return (*this + half).floor();
    return -(-(*this) + half).floor();
}

int BigRational::cmp(const BigRational& o) const {
    // compare num1*den2 vs num2*den1 (dens positive)
    BigInt l = num_ * o.den_;
    BigInt r = o.num_ * den_;
    if (l < r) return -1;
    if (l > r) return 1;
    return 0;
}

BigRational BigRational::fromDecimal(const std::string& s) {
    // handle optional sign, decimal point, exponent
    size_t i = 0;
    int sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) { if (s[i]=='-') sign=-1; ++i; }
    std::string intPart, fracPart;
    bool seenDot = false;
    for (; i < s.size(); ++i) {
        if (s[i] == '.') { seenDot = true; continue; }
        if (s[i] == 'e' || s[i] == 'E') break;
        if (s[i] >= '0' && s[i] <= '9') { if (seenDot) fracPart.push_back(s[i]); else intPart.push_back(s[i]); }
        else if (s[i] == '\'') continue;
        else throw std::invalid_argument(std::string("BigRational::fromDecimal bad char '") + s[i] + "'");
    }
    long long exp = 0;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        bool neg = false;
        if (i < s.size() && (s[i]=='+'||s[i]=='-')) { neg = s[i]=='-'; ++i; }
        long long e = 0;
        for (; i < s.size(); ++i) { if (s[i]>='0'&&s[i]<='9') e = e*10 + (s[i]-'0'); else break; }
        exp = neg ? -e : e;
    }
    // value = (intPart + fracPart/10^k) * 10^exp
    std::string digits = intPart + fracPart;
    if (digits.empty()) digits = "0";
    BigInt n(digits);
    if (sign < 0) n = -n;
    BigInt d(1);
    long long fracLen = (long long)fracPart.size();
    // apply 10^exp
    long long shift = exp - fracLen; // net power of 10
    BigInt ten(10);
    if (shift >= 0) {
        BigInt mul(1);
        for (long long k = 0; k < shift; ++k) mul *= ten;
        n *= mul;
    } else {
        BigInt mul(1);
        for (long long k = 0; k < -shift; ++k) mul *= ten;
        d *= mul;
    }
    return BigRational(n, d);
}

BigRational BigRational::fromBase(const std::string& s, int base) {
    // Parse in given base with optional fractional point.
    size_t i = 0; int sign = 1;
    if (i < s.size() && (s[i]=='+'||s[i]=='-')) { if (s[i]=='-') sign=-1; ++i; }
    // optional prefix
    if (base == 16 && i+1<s.size() && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')) i+=2;
    if (base == 2  && i+1<s.size() && s[i]=='0' && (s[i+1]=='b'||s[i+1]=='B')) i+=2;
    if (base == 8  && i+1<s.size() && s[i]=='0' && (s[i+1]=='o'||s[i+1]=='O')) i+=2;
    std::string intP, fracP; bool dot=false;
    auto digVal=[](char c)->int{
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='z')return 10+(c-'a');
        if(c>='A'&&c<='Z')return 10+(c-'A');
        return -1;
    };
    for(; i<s.size(); ++i){
        if(s[i]=='.'){dot=true;continue;}
        if(s[i]=='\'')continue;
        int v=digVal(s[i]);
        if(v<0||v>=base) throw std::invalid_argument("BigRational::fromBase bad digit");
        if(dot) fracP.push_back(s[i]); else intP.push_back(s[i]);
    }
    BigInt n(intP.empty()?"0":intP, base);
    if(sign<0) n=-n;
    BigInt d(1), bbase((long long)base);
    for(char c: fracP){ int v=digVal(c); n = n*bbase + BigInt((long long)v); d = d*bbase; }
    return BigRational(n,d);
}

std::string BigRational::toFraction() const {
    if (den_ == BigInt(1)) return num_.toString();
    return num_.toString() + "/" + den_.toString();
}

std::string BigRational::toMixed() const {
    if (isInteger() || den_ == BigInt(1)) return num_.toString();
    BigInt q = trunc();
    BigInt r = BigInt::abs(num_) % den_;
    if (q.isZero()) return toFraction();
    return q.toString() + " " + r.toString() + "/" + den_.toString();
}

std::string BigRational::toDecimal(unsigned digits, bool& exactOut, bool& recurring) const {
    exactOut = false; recurring = false;
    if (isZero()) { exactOut = true; return "0"; }
    // We compute num/den to `digits` significant figures using BigInt long division,
    // detecting termination (den has only factors 2 and 5) and recurring cycles.
    bool neg = isNegative();
    BigInt n = BigInt::abs(num_), d = den_;
    // Factor out 2s and 5s from d to know if terminating.
    BigInt dd = d;
    unsigned twos = 0, fives = 0;
    {
        BigInt two(2), five(5), q, r;
        while (true) { BigInt::divmod(dd, two, q, r); if (r.isZero()) { dd = q; ++twos; } else break; }
        while (true) { BigInt::divmod(dd, five, q, r); if (r.isZero()) { dd = q; ++fives; } else break; }
    }
    bool terminating = (dd == BigInt(1));
    // Determine number of fractional digits needed.
    // For terminating: max(twos, fives). For recurring: `digits` significant.
    // We'll produce `digits` significant figures overall.
    // Compute integer part.
    BigInt intPart, rem;
    BigInt::divmod(n, d, intPart, rem);
    std::string is = intPart.toString();
    // significant digits already in integer part
    unsigned sigInt = 0;
    for (char c : is) if (c != '-') ++sigInt;
    std::string result;
    if (neg) result = "-";
    result += is;
    if (rem.isZero()) { exactOut = true; return result; }
    result += '.';
    // produce fractional digits
    unsigned want = (sigInt >= digits) ? 0 : (digits - sigInt);
    // For terminating, we may produce fewer if it terminates earlier.
    BigInt ten(10);
    std::string frac;
    if (terminating) {
        // max(twos,fives) fractional digits, but cap by `want`+guard
        unsigned need = std::max(twos, fives);
        unsigned limit = need;
        if (want < limit && sigInt > 0) {
            // still show terminating full precision but at least `want`
        }
        // Produce up to limit digits.
        for (unsigned k = 0; k < limit; ++k) {
            rem *= ten;
            BigInt q, r; BigInt::divmod(rem, d, q, r);
            frac.push_back((char)('0' + q.toLongLong()));
            rem = r;
            if (rem.isZero()) break;
        }
        result += frac;
        exactOut = true;
        return result;
    }
    // recurring: produce `want` digits (or a few more), mark recurring.
    // Detect cycle by tracking remainders (Floyd or hash set) up to a bound.
    // Simpler: produce `want` digits then mark recurring true.
    unsigned produced = 0;
    // Try to detect a cycle within a reasonable bound using remainder tracking.
    std::vector<BigInt> seenRem;
    unsigned cycleStart = 0, cycleLen = 0;
    bool foundCycle = false;
    for (unsigned k = 0; k < want + 64 && !rem.isZero(); ++k) {
        // cycle detect
        for (size_t idx = 0; idx < seenRem.size(); ++idx) {
            if (seenRem[idx] == rem) { cycleStart = (unsigned)idx; cycleLen = k - (unsigned)idx; foundCycle = true; break; }
        }
        if (foundCycle) break;
        seenRem.push_back(rem);
        rem *= ten;
        BigInt q, r; BigInt::divmod(rem, d, q, r);
        frac.push_back((char)('0' + q.toLongLong()));
        rem = r;
        ++produced;
    }
    result += frac;
    recurring = !rem.isZero();
    if (foundCycle || recurring) {
        recurring = true;
        // annotate with ellipsis-style marker; caller may format further
        result += "...";
    }
    return result;
}

} // namespace scicalc
