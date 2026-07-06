// scicalc/Engine.cpp
#include "scicalc/Engine.hpp"
#include "scicalc/Lexer.hpp"
#include "scicalc/Parser.hpp"
#include <sstream>

namespace scicalc {

// Flat text rendering of a symbolic AST (used for CLI output of symbolic results).
static std::string flatString(const Expr& e);

static std::string flatOp(BinOp op) {
    switch (op) {
        case BinOp::Add: return " + ";
        case BinOp::Sub: return " - ";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::Mod: return " mod ";
        case BinOp::Pow: return "^";
        case BinOp::SetIntersect: return " \xE2\x88\xA9 ";
        case BinOp::SetUnion: return " \xE2\x88\xAA ";
        case BinOp::SetDiff: return " \\ ";
        case BinOp::Eq: return " = ";
        case BinOp::Neq: return " \xE2\x89\xA0 ";
        case BinOp::Lt: return " < ";
        case BinOp::Gt: return " > ";
        case BinOp::Le: return " \xE2\x89\xA4 ";
        case BinOp::Ge: return " \xE2\x89\xA5 ";
        case BinOp::In: return " \xE2\x88\x88 ";
        case BinOp::Subset: return " \xE2\x8A\x86 ";
        case BinOp::RealSubset: return " \xE2\x8A\x8A ";
        case BinOp::Cong: return " \xE2\x89\xA1 "; // ≡
        case BinOp::And: return " \xE2\x88\xA7 ";
        case BinOp::Or: return " \xE2\x88\xA8 ";
        case BinOp::Assign: return " := ";
    }
    return "?";
}

static std::string flatString(const Expr& e) {
    switch (e.kind) {
        case Expr::Number: return e.num.toFraction();
        case Expr::Var:
            if (e.name == "pi") return "\xCF\x80";
            return e.name;
        case Expr::SetName:
            if (e.name == "Real") return "\xE2\x84\x9D";
            if (e.name == "Rational") return "\xE2\x84\x9A";
            if (e.name == "Integer") return "\xE2\x84\xA4";
            return e.name;
        case Expr::Unary: {
            const char* op = (e.unop == UnaryOp::Neg) ? "-" : (e.unop == UnaryOp::Not ? "\xC2\xAC" : "+");
            return std::string(op) + flatString(*e.lhs);
        }
        case Expr::Binary:
            if (e.binop == BinOp::Pow)
                return flatString(*e.lhs) + "^(" + flatString(*e.rhs) + ")";
            if (e.binop == BinOp::Mul && e.implicit)
                return flatString(*e.lhs) + flatString(*e.rhs);  // 隐式乘法不显示×
            return "(" + flatString(*e.lhs) + flatOp(e.binop) + flatString(*e.rhs) + ")";
        case Expr::Call: {
            // 同余: cong(a,b,m) -> a ≡ b (mod m)
            if (e.name == "cong" && e.args.size() == 3)
                return flatString(*e.args[0]) + " \xE2\x89\xA1 " + flatString(*e.args[1]) +
                       " (mod " + flatString(*e.args[2]) + ")";
            // 用户输入的分组括号
            if (e.name == "__group__" && e.args.size() == 1)
                return "(" + flatString(*e.args[0]) + ")";
            // 分数: frac(a,b) -> a/b
            if (e.name == "frac" && e.args.size() == 2)
                return flatString(*e.args[0]) + "/" + flatString(*e.args[1]);
            std::string s = e.name + "(";
            for (size_t i = 0; i < e.args.size(); ++i) { if (i) s += ", "; s += flatString(*e.args[i]); }
            return s + ")";
        }
        case Expr::AssignVar: return e.name + " := " + flatString(*e.lhs);
        case Expr::AssignFunc: {
            std::string s = e.name + "(";
            for (size_t i = 0; i < e.params.size(); ++i) { if (i) s += ", "; s += e.params[i]; }
            return s + ") := " + flatString(*e.lhs);
        }
        case Expr::SetEnum: {
            std::string s = "{";
            for (size_t i = 0; i < e.args.size(); ++i) { if (i) s += ", "; s += flatString(*e.args[i]); }
            return s + "}";
        }
        case Expr::Interval: return "[" + flatString(*e.lo) + ", " + flatString(*e.hi) + "]";
        case Expr::Iverson: return "\xE2\x84\xBA(" + flatString(*e.lhs) + ")";
    }
    return "?";
}

std::string Engine::formatRational(const BigRational& r) const {
    const auto& cfg = ev.config;
    // Integer base formatting for whole numbers when base != 10
    if (r.isInteger()) {
        BigInt n = r.num();
        if (cfg.numberBase != 10) {
            return n.toBaseString(cfg.numberBase, true);
        }
    }
    // Math output mode: prefer exact fraction.
    if (cfg.outputMode == OutputMode::Math) {
        if (r.isInteger()) return r.num().toString();
        return r.toFraction();
    }
    // Decimal output mode for a rational: expand to `precision` sig digits.
    bool exact, rec;
    std::string s = r.toDecimal(cfg.precision, exact, rec);
    // apply number format
    if (cfg.numFormat == EngineConfig::NumFormat::Scientific) {
        // convert to scientific; simplest: use BigFloat formatting
        return BigFloat::fromRational(r, cfg.precision).toString(cfg.precision);
    }
    if (cfg.numFormat == EngineConfig::NumFormat::FixedPoint) {
        // fixed digits after point
        BigInt p10(1), ten(10);
        for (int i = 0; i < cfg.fixedDigits; ++i) p10 *= ten;
        BigRational scaled = r * BigRational(p10);
        BigInt rounded = scaled.round();
        bool neg = rounded.isNegative();
        if (neg) rounded = -rounded;
        std::string digits = rounded.toString();
        while ((int)digits.size() <= cfg.fixedDigits) digits = "0" + digits;
        std::string intPart = digits.substr(0, digits.size() - cfg.fixedDigits);
        std::string fracPart = digits.substr(digits.size() - cfg.fixedDigits);
        return (neg ? "-" : "") + intPart + "." + fracPart;
    }
    if (cfg.numFormat == EngineConfig::NumFormat::FixedSignificant) {
        return BigFloat::fromRational(r, cfg.precision).toString(cfg.precision);
    }
    return s; // General / Float
}

std::string Engine::formatDecimal(const BigFloat& d) const {
    return d.toString(ev.config.precision);
}

std::string Engine::format(const Value& v) const {
    switch (v.type) {
        case Value::Rational: return formatRational(v.rat);
        case Value::Decimal: return formatDecimal(v.dec);
        case Value::Boolean: return v.boolean ? "true" : "false";
        case Value::Set: return v.set->toString();
        case Value::Symbolic: {
            if (!v.sym) return "?";
            return flatString(*v.sym);
        }
        case Value::Error: return "Error: " + v.err;
    }
    return "?";
}

EngineResult Engine::evaluate(const std::string& src) {
    EngineResult res;
    res.inputEcho = src;
    try {
        Lexer lex(src);
        auto toks = lex.tokenize();
        Parser p(std::move(toks));
        ExprPtr ast = p.parse();
        if (!p.fullyConsumed())
            throw std::runtime_error("unexpected trailing input after expression");
        Value v = ev.eval(*ast);
        res.value = v;
        res.output = format(v);
        res.renderTree = buildBox(v);
        res.ok = true;
    } catch (const std::exception& ex) {
        res.ok = false;
        res.error = ex.what();
    }
    return res;
}

} // namespace scicalc
