// scicalc/BigFloat.cpp
#include "scicalc/BigFloat.hpp"
#include <algorithm>
#include <stdexcept>

namespace scicalc {

unsigned& BigFloat::defaultPrecision() {
    static unsigned p = 50;
    return p;
}

BigFloat::BigFloat(long long v) : mant_(v), exp_(0) {
    prec_ = defaultPrecision();
}

static unsigned digits10Of(const BigInt& b) {
    if (b.isZero()) return 1;
    std::string s = BigInt::abs(b).toString();
    return (unsigned)s.size();
}

void BigFloat::roundInPlace(unsigned prec) {
    if (mant_.isZero()) { prec_ = prec; return; }
    unsigned d = digits10Of(mant_);
    if (d <= prec) { prec_ = prec; return; }
    long long shift = (long long)d - (long long)prec;
    // divide by 10^shift with rounding (half away from zero)
    BigInt ten(10), div(1);
    for (long long i = 0; i < shift; ++i) div *= ten;
    BigInt q, r;
    BigInt::divmod(BigInt::abs(mant_), div, q, r);
    // round half up on absolute
    BigInt two(2);
    BigInt halfRem = div / two;
    if (r >= halfRem) q += BigInt(1);
    BigInt nm = q;
    if (mant_.isNegative()) nm = -nm;
    mant_ = nm;
    exp_ += shift;
    prec_ = prec;
    // strip trailing zeros to keep canonical
    while (!mant_.isZero()) {
        BigInt qq, rr; BigInt::divmod(mant_, ten, qq, rr);
        if (rr.isZero()) { mant_ = qq; ++exp_; } else break;
    }
}

BigFloat BigFloat::roundTo(unsigned prec) const {
    BigFloat r = *this;
    r.roundInPlace(prec);
    return r;
}

BigFloat BigFloat::fromRational(const BigRational& r, unsigned prec) {
    if (prec == 0) prec = defaultPrecision();
    if (r.isZero()) return BigFloat();
    // Compute r.num / r.den to `prec` significant digits.
    BigInt n = r.num(), d = r.den();
    bool neg = n.isNegative();
    if (neg) n = -n;
    // Estimate magnitude: number of integer digits.
    // We scale n by 10^k so that n/d has about `prec` digits, then divide.
    // First, get a rough integer-part length.
    BigInt intPart, rem;
    BigInt::divmod(n, d, intPart, rem);
    std::string ip = intPart.toString();
    unsigned intDigits = (unsigned)ip.size();
    unsigned fracDigitsNeeded;
    long long k;
    if (intDigits >= prec) {
        // value is large; scale down to prec digits
        k = 0;
        fracDigitsNeeded = 0;
    } else {
        fracDigitsNeeded = prec - intDigits;
        k = (long long)fracDigitsNeeded;
    }
    BigInt ten(10), scale(1);
    for (long long i = 0; i < k; ++i) scale *= ten;
    BigInt scaledN = n * scale;
    BigInt q, r2;
    BigInt::divmod(scaledN, d, q, r2);
    // round
    BigInt half = d / BigInt(2);
    if (r2 >= half) q += BigInt(1);
    if (neg) q = -q;
    BigFloat result;
    result.mant_ = q;
    result.exp_ = -k;
    result.prec_ = prec;
    // strip trailing zeros
    while (!result.mant_.isZero()) {
        BigInt qq, rr; BigInt::divmod(result.mant_, ten, qq, rr);
        if (rr.isZero()) { result.mant_ = qq; ++result.exp_; } else break;
    }
    return result;
}

BigFloat BigFloat::fromDecimal(const std::string& s, unsigned prec) {
    if (prec == 0) prec = defaultPrecision();
    BigRational r = BigRational::fromDecimal(s);
    return fromRational(r, prec);
}

static BigInt pow10(long long e) {
    if (e < 0) throw std::invalid_argument("pow10: negative");
    BigInt r(1), ten(10);
    for (long long i = 0; i < e; ++i) r *= ten;
    return r;
}

// Align two floats to the same exponent (returns the max exponent; scales mantissas).
static void align(const BigFloat& a, const BigFloat& b, BigInt& ma, BigInt& mb, long long& commonExp) {
    long long ea = a.exponent(), eb = b.exponent();
    commonExp = std::min(ea, eb);
    BigInt ten(10);
    ma = a.mantissa();
    mb = b.mantissa();
    long long da = ea - commonExp;
    long long db = eb - commonExp;
    for (long long i = 0; i < da; ++i) ma *= ten;
    for (long long i = 0; i < db; ++i) mb *= ten;
}

BigFloat BigFloat::operator+(const BigFloat& o) const {
    BigInt ma, mb; long long ce;
    align(*this, o, ma, mb, ce);
    BigFloat r;
    r.mant_ = ma + mb;
    r.exp_ = ce;
    r.prec_ = std::max(prec_, o.prec_);
    r.roundInPlace(std::max(prec_, o.prec_));
    return r;
}
BigFloat BigFloat::operator-(const BigFloat& o) const {
    return *this + (-o);
}
BigFloat BigFloat::operator*(const BigFloat& o) const {
    BigFloat r;
    r.mant_ = mant_ * o.mant_;
    r.exp_ = exp_ + o.exp_;
    r.prec_ = std::max(prec_, o.prec_);
    r.roundInPlace(std::max(prec_, o.prec_));
    return r;
}
BigFloat BigFloat::operator/(const BigFloat& o) const {
    if (o.isZero()) throw std::invalid_argument("BigFloat: division by zero");
    unsigned prec = std::max(prec_, o.prec_);
    if (prec == 0) prec = defaultPrecision();
    // scale numerator to keep `prec` digits after division
    BigInt ten(10);
    BigInt scaledN = mant_;
    long long extra = 0;
    // Add guard digits
    unsigned guard = prec + 8;
    // scale so that |scaledN| has more digits than |o.mant_|
    unsigned dn = digits10Of(mant_);
    unsigned dd = digits10Of(o.mant_);
    long long need = (long long)dd + (long long)guard - (long long)dn;
    if (need < 0) need = 0;
    BigInt scale = pow10((unsigned)need);
    scaledN = mant_ * scale;
    extra = need;
    BigInt q, r;
    BigInt::divmod(scaledN, o.mant_, q, r);
    BigInt half = o.mant_ / BigInt(2);
    if (BigInt::abs(r) >= half) {
        if (r.isNegative() != o.mant_.isNegative()) q -= BigInt(1); else q += BigInt(1);
    }
    BigFloat res;
    res.mant_ = q;
    res.exp_ = exp_ - o.exp_ - extra;
    res.prec_ = prec;
    res.roundInPlace(prec);
    return res;
}

bool BigFloat::operator<(const BigFloat& o) const { return cmp(o) < 0; }
bool BigFloat::operator==(const BigFloat& o) const { return cmp(o) == 0; }

int BigFloat::cmp(const BigFloat& o) const {
    int sa = signum(), sb = o.signum();
    if (sa != sb) return sa < sb ? -1 : 1;
    if (sa == 0) return 0;
    // align and compare magnitudes
    BigInt ma, mb; long long ce;
    align(*this, o, ma, mb, ce);
    int c = (ma < mb) ? -1 : (ma > mb ? 1 : 0);
    return sa < 0 ? -c : c;
}

BigFloat BigFloat::sqrt(const BigFloat& x) {
    if (x.isNegative()) throw std::invalid_argument("sqrt of negative");
    if (x.isZero()) return BigFloat();
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    // Newton: y_{n+1} = (y_n + x/y_n)/2
    // Initial guess from exponent.
    BigFloat guess;
    {
        // approx = 10^(exp/2) * leading
        long long e = x.exponent();
        BigInt m = x.mantissa();
        // value ~ m * 10^e. sqrt ~ sqrt(m) * 10^(e/2).
        long long halfE = e / 2;
        // mantissa of guess: rough sqrt of |m| using long double estimate is avoided;
        // use 1 as initial and let Newton converge.
        guess.mant_ = BigInt(1);
        // scale guess to roughly the right magnitude
        long long est = halfE + (digits10Of(m) / 2);
        guess.exp_ = est;
        guess.prec_ = prec;
    }
    BigFloat two(2);
    BigFloat prev;
    unsigned guard = prec + 4;
    for (int iter = 0; iter < 200; ++iter) {
        BigFloat y = guess.roundTo(guard);
        BigFloat next = (y + x / y) / two;
        next.roundInPlace(guard);
        if (next.cmp(y) == 0) break;
        // convergence check
        BigFloat diff = next - y;
        if (!diff.isZero()) {
            BigFloat absDiff = diff.isNegative() ? -diff : diff;
            BigFloat absY = y.isNegative() ? -y : y;
            // if absDiff < absY * 10^-(guard)
            // simple: compare relative
            if (absDiff.cmp(absY.roundTo(guard)) < 0) {
                // check digits stabilized
            }
        }
        guess = next;
        (void)prev;
    }
    return guess.roundTo(prec);
}

BigFloat BigFloat::root(const BigFloat& x, unsigned n) {
    if (n == 0) throw std::invalid_argument("root: n=0");
    if (n == 1) return x;
    if (n == 2) return sqrt(x);
    if (x.isZero()) return BigFloat();
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    // Newton: y_{n+1} = ((n-1)*y + x/y^{n-1}) / n
    BigFloat guess = x;
    BigFloat nBf((long long)n), nMinus1((long long)(n - 1));
    unsigned guard = prec + 6;
    for (int iter = 0; iter < 300; ++iter) {
        BigFloat y = guess.roundTo(guard);
        BigFloat ypow(1);
        for (unsigned k = 0; k < n - 1; ++k) ypow = ypow * y;
        if (ypow.isZero()) break;
        BigFloat next = (nMinus1 * y + x / ypow) / nBf;
        next.roundInPlace(guard);
        if (next.cmp(y) == 0) { guess = next; break; }
        guess = next;
    }
    return guess.roundTo(prec);
}

BigFloat BigFloat::exp(const BigFloat& x) {
    // exp(x) = exp(x/k)^k, reduce by k=2^m until |x/k| small, then Taylor.
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    unsigned guard = prec + 8;
    BigFloat two(2);
    // reduce
    int m = 0;
    BigFloat xr = x.roundTo(guard);
    // while |xr| > 0.5, halve
    BigFloat half = fromRational(BigRational(BigInt(1), BigInt(2)), guard);
    while (xr.isNegative() ? (-xr).cmp(half) > 0 : xr.cmp(half) > 0) {
        xr = (xr / two).roundTo(guard);
        ++m;
    }
    // Taylor: 1 + x + x^2/2! + ...
    BigFloat sum(1); sum.prec_ = guard;
    BigFloat term(1); term.prec_ = guard;
    BigFloat xrG = xr.roundTo(guard);
    for (unsigned k = 1; k < 500; ++k) {
        term = (term * xrG / BigFloat((long long)k)).roundTo(guard);
        BigFloat newSum = (sum + term).roundTo(guard);
        if (newSum.cmp(sum) == 0) break;
        sum = newSum;
        if (term.isZero()) break;
    }
    // square m times
    for (int i = 0; i < m; ++i) sum = (sum * sum).roundTo(guard);
    return sum.roundTo(prec);
}

BigFloat BigFloat::ln(const BigFloat& x) {
    if (x.isZero() || x.isNegative()) throw std::invalid_argument("ln of non-positive");
    if (x == BigFloat(1)) return BigFloat();
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    unsigned guard = prec + 12;

    // Pure atanh-series for ln(y) with y already reduced to ~[0.5, 2]. No constants.
    auto lnNear1 = [&](const BigFloat& yIn) -> BigFloat {
        BigFloat one(1), two(2);
        BigFloat y = yIn.roundTo(guard);
        long long p2 = 0;
        BigFloat half = fromRational(BigRational(BigInt(1), BigInt(2)), guard);
        while (y.cmp(two) > 0) { y = (y / two).roundTo(guard); ++p2; }
        while (y.cmp(half) < 0) { y = (y * two).roundTo(guard); --p2; }
        BigFloat t = ((y - one) / (y + one)).roundTo(guard);
        BigFloat t2 = (t * t).roundTo(guard);
        BigFloat sum = t.roundTo(guard);
        BigFloat term = t.roundTo(guard);
        for (long long k = 3; k < 2000; k += 2) {
            term = (term * t2).roundTo(guard);
            BigFloat inc = (term / BigFloat(k)).roundTo(guard);
            BigFloat ns = (sum + inc).roundTo(guard);
            if (ns.cmp(sum) == 0) break;
            sum = ns;
            if (inc.isZero()) break;
        }
        sum = (sum * two).roundTo(guard);
        // p2 contribution uses ln2 (computed by lnNear1 with y=2, p2 stays 0 there
        // because 2 is not > 2). To avoid recursion, compute ln2 inline here only
        // when p2 != 0.
        if (p2 != 0) {
            // ln2 = 2*atanh(1/3)
            BigFloat inv3 = (one / BigFloat(3)).roundTo(guard);
            BigFloat tt = inv3, t2b = (inv3 * inv3).roundTo(guard), s2 = inv3;
            for (long long k = 3; k < 2000; k += 2) {
                tt = (tt * t2b).roundTo(guard);
                BigFloat inc = (tt / BigFloat(k)).roundTo(guard);
                BigFloat ns = (s2 + inc).roundTo(guard);
                if (ns.cmp(s2) == 0) break;
                s2 = ns;
                if (inc.isZero()) break;
            }
            BigFloat ln2 = (s2 * two).roundTo(guard);
            sum = (sum + (BigFloat((long long)p2) * ln2).roundTo(guard)).roundTo(guard);
        }
        return sum;
    };

    // Separate decimal exponent: x = m * 10^e, ln(x) = ln(m) + e*ln(10).
    long long e = x.exponent();
    BigFloat m = BigFloat(x.mantissa(), 0, guard);
    BigFloat lnM = lnNear1(m);
    BigFloat result = lnM;
    if (e != 0) {
        // ln(10) = lnNear1(10) (reduces internally by halving)
        BigFloat ln10 = lnNear1(BigFloat(10));
        result = (result + (BigFloat((long long)e) * ln10).roundTo(guard)).roundTo(guard);
    }
    return result.roundTo(prec);
}

BigFloat BigFloat::log(const BigFloat& x, const BigFloat& base) {
    return ln(x) / ln(base);
}

BigFloat BigFloat::sin(const BigFloat& x) {
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    unsigned guard = prec + 12;
    // range reduce mod 2pi into [-pi, pi]
    BigFloat twoPi = (pi(guard) * BigFloat(2)).roundTo(guard);
    BigFloat piV = pi(guard);
    BigFloat xr = x.roundTo(guard);
    while (xr.isNegative() ? (-xr).cmp(piV) > 0 : xr.cmp(piV) > 0) {
        if (xr.isNegative()) xr = (xr + twoPi).roundTo(guard);
        else xr = (xr - twoPi).roundTo(guard);
    }
    // Taylor with correct factorial recurrence:
    //   term_0 = x;  term_k = term_{k-1} * x^2 / ((2k)(2k+1));  sign alternates.
    BigFloat x2 = (xr * xr).roundTo(guard);
    BigFloat term = xr.roundTo(guard);   // x^1 / 1!
    BigFloat sum = term;
    for (unsigned k = 1; k < 2000; ++k) {
        BigFloat denom = BigFloat((long long)(2 * k)) * BigFloat((long long)(2 * k + 1));
        term = (term * x2 / denom).roundTo(guard);
        BigFloat signedTerm = (k % 2 == 1) ? -term : term;
        BigFloat ns = (sum + signedTerm).roundTo(guard);
        if (ns.cmp(sum) == 0) break;
        sum = ns;
    }
    return sum.roundTo(prec);
}

BigFloat BigFloat::cos(const BigFloat& x) {
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    unsigned guard = prec + 12;
    BigFloat twoPi = (pi(guard) * BigFloat(2)).roundTo(guard);
    BigFloat piV = pi(guard);
    BigFloat xr = x.roundTo(guard);
    while (xr.isNegative() ? (-xr).cmp(piV) > 0 : xr.cmp(piV) > 0) {
        if (xr.isNegative()) xr = (xr + twoPi).roundTo(guard);
        else xr = (xr - twoPi).roundTo(guard);
    }
    // term_0 = 1; term_k = term_{k-1} * x^2 / ((2k-1)(2k)); sign alternates.
    BigFloat x2 = (xr * xr).roundTo(guard);
    BigFloat term(1); term.prec_ = guard;
    BigFloat sum(1); sum.prec_ = guard;
    for (unsigned k = 1; k < 2000; ++k) {
        BigFloat denom = BigFloat((long long)(2 * k - 1)) * BigFloat((long long)(2 * k));
        term = (term * x2 / denom).roundTo(guard);
        BigFloat signedTerm = (k % 2 == 1) ? -term : term;
        BigFloat ns = (sum + signedTerm).roundTo(guard);
        if (ns.cmp(sum) == 0) break;
        sum = ns;
    }
    return sum.roundTo(prec);
}

BigFloat BigFloat::tan(const BigFloat& x) {
    BigFloat c = cos(x);
    if (c.isZero()) throw std::domain_error("tan undefined (cos=0)");
    return sin(x) / c;
}

BigFloat BigFloat::asin(const BigFloat& x) {
    // asin(x) = x + x^3/6 + 3x^5/40 + ... (for |x|<1); use atan2-like fallback.
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    BigFloat one(1);
    if (x.cmp(one) > 0 || (-x).cmp(one) > 0)
        throw std::domain_error("asin domain");
    if (x == one) return (pi(prec) / BigFloat(2)).roundTo(prec);
    if (x == -one) return (-(pi(prec) / BigFloat(2))).roundTo(prec);
    // series: asin x = sum (2k choose k) x^{2k+1} / (4^k (2k+1))
    unsigned guard = prec + 10;
    BigFloat x2 = (x * x).roundTo(guard);
    BigFloat term = x.roundTo(guard);
    BigFloat sum = x.roundTo(guard);
    BigInt coef(1); // (2k choose k)/4^k
    for (unsigned k = 1; k < 1000; ++k) {
        // coef_{k} = coef_{k-1} * (2k-1)/(2k)
        BigInt num((long long)(2 * k - 1)), den((long long)(2 * k));
        coef = (coef * num);
        BigInt qq, rr; BigInt::divmod(coef, den, qq, rr); coef = qq;
        term = (term * x2).roundTo(guard);
        BigFloat inc = (term * BigFloat(coef) / BigFloat((long long)(2 * k + 1))).roundTo(guard);
        BigFloat newSum = (sum + inc).roundTo(guard);
        if (newSum.cmp(sum) == 0) break;
        sum = newSum;
        if (inc.isZero()) break;
    }
    return sum.roundTo(prec);
}
BigFloat BigFloat::acos(const BigFloat& x) {
    BigFloat halfPi = (pi(x.precision() ? x.precision() : defaultPrecision()) / BigFloat(2));
    return (halfPi - asin(x)).roundTo(x.precision() ? x.precision() : defaultPrecision());
}
BigFloat BigFloat::atan(const BigFloat& x) {
    // atan x = x - x^3/3 + x^5/7... ; for |x|>1 use atan(x)=pi/2 - atan(1/x)
    unsigned prec = x.precision(); if (prec == 0) prec = defaultPrecision();
    unsigned guard = prec + 10;
    BigFloat one(1);
    BigFloat xr = x;
    bool recip = false;
    if (x.isNegative() ? (-x).cmp(one) > 0 : x.cmp(one) > 0) {
        xr = (one / x).roundTo(guard);
        recip = true;
    }
    BigFloat x2 = (xr * xr).roundTo(guard);
    BigFloat term = xr.roundTo(guard);
    BigFloat sum = xr.roundTo(guard);
    bool neg = false;
    for (long long k = 3; k < 2000; k += 2) {
        term = (term * x2).roundTo(guard);
        BigFloat inc = (term / BigFloat(k)).roundTo(guard);
        neg = !neg;
        BigFloat signedInc = neg ? -inc : inc;
        BigFloat newSum = (sum + signedInc).roundTo(guard);
        if (newSum.cmp(sum) == 0) break;
        sum = newSum;
        if (inc.isZero()) break;
    }
    if (recip) sum = ((pi(guard) / BigFloat(2)) - sum).roundTo(guard);
    return sum.roundTo(prec);
}

BigFloat BigFloat::pow(const BigFloat& x, const BigFloat& y) {
    if (y.isZero()) return BigFloat(1);
    // integer y -> exact via BigRational
    if (y.exponent() == 0 && y.mantissa().fitsLongLong()) {
        // check if mantissa is a small integer
        long long yi = y.mantissa().toLongLong();
        if (y.mantissa() == BigInt(yi)) {
            BigRational base = x.toRational();
            return fromRational(BigRational::pow(base, BigInt(yi)), x.precision());
        }
    }
    if (x.isZero()) {
        if (y.isNegative()) throw std::domain_error("0^negative");
        return BigFloat();
    }
    return exp(y * ln(x)).roundTo(x.precision() ? x.precision() : defaultPrecision());
}

BigRational BigFloat::toRational() const {
    // value = mant * 10^exp
    if (exp_ >= 0) return BigRational(mant_ * pow10((unsigned)exp_));
    BigInt den = pow10((unsigned)(-exp_));
    return BigRational(mant_, den);
}

std::string BigFloat::toString(unsigned prec) const {
    if (prec == 0) prec = prec_ ? prec_ : defaultPrecision();
    if (isZero()) return "0";
    // produce decimal with `prec` significant digits
    BigFloat v = roundTo(prec);
    BigInt m = BigInt::abs(v.mant_);
    std::string digits = m.toString();
    bool neg = v.mant_.isNegative();
    long long e = v.exp_;
    // value = digits * 10^e, digits has d digits
    long long d = (long long)digits.size();
    long long decimalPos = d + e; // position of decimal point from left of digits
    std::string out;
    if (neg) out = "-";
    // Decide scientific vs plain based on magnitude
    if (decimalPos > prec + 6 || decimalPos < -5) {
        // scientific: d.dddde±xx
        out += digits[0];
        if (digits.size() > 1) { out += '.'; out += digits.substr(1, std::min((size_t)prec - 1, digits.size() - 1)); }
        long long exp10 = decimalPos - 1;
        out += 'e';
        if (exp10 >= 0) out += '+'; else { out += '-'; exp10 = -exp10; }
        out += std::to_string(exp10);
    } else if (decimalPos <= 0) {
        // 0.00ddd
        out += "0.";
        for (long long i = 0; i < -decimalPos; ++i) out += '0';
        out += digits;
    } else if (decimalPos >= d) {
        out += digits;
        for (long long i = 0; i < decimalPos - d; ++i) out += '0';
    } else {
        out += digits.substr(0, (size_t)decimalPos);
        out += '.';
        out += digits.substr((size_t)decimalPos);
    }
    return out;
}

// --- constants via Machin-like formulas ---
BigFloat BigFloat::pi(unsigned prec) {
    if (prec == 0) prec = defaultPrecision();
    // Machin: pi = 16*atan(1/5) - 4*atan(1/239)
    unsigned guard = prec + 12;
    auto atanInv = [&](long long n) -> BigFloat {
        // atan(1/n) = 1/n - 1/(3n^3) + 1/(5n^5) - ...
        BigFloat nBf((long long)n);
        nBf.prec_ = guard;
        BigFloat inv = (BigFloat(1) / nBf).roundTo(guard);
        BigFloat inv2 = (inv * inv).roundTo(guard);
        BigFloat term = inv;
        BigFloat sum = inv;
        bool neg = false;
        for (long long k = 3; k < 5000; k += 2) {
            term = (term * inv2).roundTo(guard);
            BigFloat inc = (term / BigFloat(k)).roundTo(guard);
            neg = !neg;
            BigFloat s = neg ? -inc : inc;
            BigFloat ns = (sum + s).roundTo(guard);
            if (ns.cmp(sum) == 0) break;
            sum = ns;
            if (inc.isZero()) break;
        }
        return sum;
    };
    BigFloat a1 = atanInv(5);
    BigFloat a2 = atanInv(239);
    BigFloat result = (a1 * BigFloat(16) - a2 * BigFloat(4)).roundTo(guard);
    return result.roundTo(prec);
}

BigFloat BigFloat::e(unsigned prec) {
    if (prec == 0) prec = defaultPrecision();
    unsigned guard = prec + 10;
    BigFloat sum(1); sum.prec_ = guard;
    BigFloat term(1); term.prec_ = guard;
    for (unsigned k = 1; k < 2000; ++k) {
        term = (term / BigFloat((long long)k)).roundTo(guard);
        BigFloat ns = (sum + term).roundTo(guard);
        if (ns.cmp(sum) == 0) break;
        sum = ns;
    }
    return sum.roundTo(prec);
}

} // namespace scicalc
