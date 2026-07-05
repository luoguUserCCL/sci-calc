// scicalc/Box.cpp — box factory + AST->box construction.
#include "scicalc/Box.hpp"
#include "scicalc/Engine.hpp"
#include <sstream>

namespace scicalc {

BoxPtr Box::makeText(const std::string& s, TextStyle st) {
    auto b = std::make_unique<Box>(); b->kind = Text; b->text = s; b->style = st; return b;
}
BoxPtr Box::makeRow(std::vector<BoxPtr> children) {
    auto b = std::make_unique<Box>(); b->kind = Row; b->children = std::move(children); return b;
}
BoxPtr Box::makeFraction(BoxPtr n, BoxPtr d) {
    auto b = std::make_unique<Box>(); b->kind = Fraction; b->num = std::move(n); b->den = std::move(d); return b;
}
BoxPtr Box::makeSupSub(BoxPtr base, BoxPtr sup, BoxPtr sub) {
    auto b = std::make_unique<Box>(); b->kind = SupSub; b->base = std::move(base);
    b->sup = std::move(sup); b->sub = std::move(sub); return b;
}
BoxPtr Box::makeRadical(BoxPtr radicand, BoxPtr index) {
    auto b = std::make_unique<Box>(); b->kind = Radical; b->radicand = std::move(radicand); b->index = std::move(index); return b;
}
BoxPtr Box::makeBigOp(const std::string& sym, BoxPtr lo, BoxPtr hi, BoxPtr body) {
    auto b = std::make_unique<Box>(); b->kind = BigOp; b->opSymbol = sym;
    b->lower = std::move(lo); b->upper = std::move(hi); b->body = std::move(body); return b;
}
BoxPtr Box::makeDelimited(const std::string& l, const std::string& r, BoxPtr inner) {
    auto b = std::make_unique<Box>(); b->kind = Delimited; b->leftDelim = l; b->rightDelim = r;
    b->children.push_back(std::move(inner)); return b;
}
BoxPtr Box::makeFunction(const std::string& name, BoxPtr sub, BoxPtr arg) {
    auto b = std::make_unique<Box>(); b->kind = Function; b->funcName = name; b->funcSub = std::move(sub); b->funcArg = std::move(arg); return b;
}
BoxPtr Box::makePadded(BoxPtr inner, float, float) {
    auto b = std::make_unique<Box>(); b->kind = Padded; b->children.push_back(std::move(inner)); return b;
}

// --- pretty-print operators / keywords to symbols ---
static const char* opSymbol(BinOp op) {
    switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Sub: return "\xE2\x88\x92"; // − (U+2212)
        case BinOp::Mul: return "\xC3\x97";      // ×
        case BinOp::Div: return "\xC3\xB7";      // ÷
        case BinOp::Mod: return " mod ";
        case BinOp::Pow: return "^";
        case BinOp::SetIntersect: return "\xE2\x88\xA9"; // ∩
        case BinOp::SetUnion: return "\xE2\x88\xAA";     // ∪
        case BinOp::SetDiff: return "\\";
        case BinOp::Eq: return "=";
        case BinOp::Neq: return "\xE2\x89\xA0"; // ≠
        case BinOp::Lt: return "<";
        case BinOp::Gt: return ">";
        case BinOp::Le: return "\xE2\x89\xA4"; // ≤
        case BinOp::Ge: return "\xE2\x89\xA5"; // ≥
        case BinOp::In: return "\xE2\x88\x88"; // ∈
        case BinOp::Subset: return "\xE2\x8A\x86"; // ⊆
        case BinOp::RealSubset: return "\xE2\x8A\x8A"; // ⊊
        case BinOp::Cong: return "\xE2\x89\xA1"; // ≡
        case BinOp::And: return "\xE2\x88\xA7"; // ∧
        case BinOp::Or: return "\xE2\x88\xA8";  // ∨
        case BinOp::Assign: return ":=";
    }
    return "?";
}

static const char* setNameSym(const std::string& n) {
    if (n == "Real") return "\xE2\x84\x9D";       // ℝ
    if (n == "Rational") return "\xE2\x84\x9A";   // ℚ
    if (n == "Integer") return "\xE2\x84\xA4";    // ℤ
    return n.c_str();
}

BoxPtr numberBox(const BigRational& r) {
    // For input echo we use fraction form for non-integers, decimal for integers.
    return Box::makeText(r.toFraction(), Box::Number);
}

BoxPtr buildInputBox(const Expr& e) {
    switch (e.kind) {
        case Expr::Number: return numberBox(e.num);
        case Expr::Var: {
            if (e.name == "pi") return Box::makeText("\xCF\x80", Box::Symbol); // π
            if (e.name == "e") return Box::makeText("e", Box::Identifier);
            return Box::makeText(e.name, Box::Identifier);
        }
        case Expr::SetName: return Box::makeText(setNameSym(e.name), Box::Symbol);
        case Expr::Unary: {
            const char* op = (e.unop == UnaryOp::Neg) ? "\xE2\x88\x92" :
                             (e.unop == UnaryOp::Not) ? "\xC2\xAC" : "+"; // − ¬ +
            std::vector<BoxPtr> row;
            row.push_back(Box::makeText(op, Box::Operator));
            row.push_back(buildInputBox(*e.lhs));
            return Box::makeRow(std::move(row));
        }
        case Expr::Binary: {
            std::vector<BoxPtr> row;
            if (e.binop == BinOp::Pow) {
                BoxPtr base = buildInputBox(*e.lhs);
                BoxPtr exp = buildInputBox(*e.rhs);
                return Box::makeSupSub(std::move(base), std::move(exp), nullptr);
            }
            row.push_back(buildInputBox(*e.lhs));
            row.push_back(Box::makeText(opSymbol(e.binop), Box::Operator));
            row.push_back(buildInputBox(*e.rhs));
            return Box::makeRow(std::move(row));
        }
        case Expr::Call: {
            // special renderings
            const std::string& n = e.name;
            if (n == "abs" && e.args.size() == 1)
                return Box::makeDelimited("|", "|", buildInputBox(*e.args[0]));
            if (n == "floor" && e.args.size() == 1)
                return Box::makeDelimited("\xE2\x8C\x8A", "\xE2\x8C\x8B", buildInputBox(*e.args[0])); // ⌊ ⌋
            if (n == "ceil" && e.args.size() == 1)
                return Box::makeDelimited("\xE2\x8C\x88", "\xE2\x8C\x89", buildInputBox(*e.args[0])); // ⌈ ⌉
            if (n == "sqrt") {
                if (e.args.size() == 1) return Box::makeRadical(buildInputBox(*e.args[0]));
                if (e.args.size() == 2) return Box::makeRadical(buildInputBox(*e.args[1]), buildInputBox(*e.args[0]));
            }
            if ((n == "sum" || n == "prod") && e.args.size() == 4) {
                const char* sym = (n == "sum") ? "\xE2\x88\x91" : "\xE2\x88\x8F"; // ∑ ∏
                std::vector<BoxPtr> lo; lo.push_back(Box::makeText(e.args[0]->kind == Expr::Var ? e.args[0]->name : "i", Box::Identifier));
                lo.push_back(Box::makeText("=", Box::Operator)); lo.push_back(buildInputBox(*e.args[1]));
                return Box::makeBigOp(sym, Box::makeRow(std::move(lo)), buildInputBox(*e.args[2]), buildInputBox(*e.args[3]));
            }
            if (n == "log") {
                if (e.args.size() == 2)
                    return Box::makeFunction("log", buildInputBox(*e.args[0]), buildInputBox(*e.args[1]));
                return Box::makeFunction("ln", nullptr, buildInputBox(*e.args[0]));
            }
            if (n == "Iverson" && e.args.size() == 1) {
                std::vector<BoxPtr> row;
                row.push_back(Box::makeText("\xE2\x84\x9D", Box::Symbol)); // 𝕀-ish; use ℝ-styled? Use I
                row.push_back(Box::makeDelimited("(", ")", buildInputBox(*e.args[0])));
                return Box::makeRow(std::move(row));
            }
            // 同余: cong(a, b, m) -> a ≡ b (mod m)
            if (n == "cong" && e.args.size() == 3) {
                std::vector<BoxPtr> row;
                row.push_back(buildInputBox(*e.args[0]));
                row.push_back(Box::makeText(" \xE2\x89\xA1 ", Box::Operator)); // ≡
                row.push_back(buildInputBox(*e.args[1]));
                row.push_back(Box::makeText(" (mod ", Box::Normal));
                row.push_back(buildInputBox(*e.args[2]));
                row.push_back(Box::makeText(")", Box::Normal));
                return Box::makeRow(std::move(row));
            }
            // generic function: name(arg, ...)
            std::vector<BoxPtr> args;
            for (size_t i = 0; i < e.args.size(); ++i) {
                if (i) args.push_back(Box::makeText(", ", Box::Normal));
                args.push_back(buildInputBox(*e.args[i]));
            }
            return Box::makeFunction(n, nullptr, Box::makeRow(std::move(args)));
        }
        case Expr::AssignVar: {
            std::vector<BoxPtr> row;
            row.push_back(Box::makeText(e.name, Box::Identifier));
            row.push_back(Box::makeText(":=", Box::Operator));
            row.push_back(buildInputBox(*e.lhs));
            return Box::makeRow(std::move(row));
        }
        case Expr::AssignFunc: {
            std::vector<BoxPtr> sig;
            sig.push_back(Box::makeText(e.name, Box::Identifier));
            sig.push_back(Box::makeText("(", Box::Normal));
            for (size_t i = 0; i < e.params.size(); ++i) {
                if (i) sig.push_back(Box::makeText(", ", Box::Normal));
                sig.push_back(Box::makeText(e.params[i], Box::Identifier));
            }
            sig.push_back(Box::makeText(")", Box::Normal));
            sig.push_back(Box::makeText(":=", Box::Operator));
            sig.push_back(buildInputBox(*e.lhs));
            return Box::makeRow(std::move(sig));
        }
        case Expr::SetEnum: {
            std::vector<BoxPtr> row;
            row.push_back(Box::makeText("{", Box::Normal));
            for (size_t i = 0; i < e.args.size(); ++i) {
                if (i) row.push_back(Box::makeText(", ", Box::Normal));
                row.push_back(buildInputBox(*e.args[i]));
            }
            row.push_back(Box::makeText("}", Box::Normal));
            return Box::makeRow(std::move(row));
        }
        case Expr::Interval: {
            const char* l = (e.intKind == SetIntervalKind::ClosedClosed || e.intKind == SetIntervalKind::ClosedOpen) ? "[" : "(";
            const char* r = (e.intKind == SetIntervalKind::ClosedClosed || e.intKind == SetIntervalKind::OpenClosed) ? "]" : ")";
            std::vector<BoxPtr> row;
            row.push_back(Box::makeText(l, Box::Normal));
            row.push_back(buildInputBox(*e.lo));
            row.push_back(Box::makeText(", ", Box::Normal));
            row.push_back(buildInputBox(*e.hi));
            row.push_back(Box::makeText(r, Box::Normal));
            return Box::makeRow(std::move(row));
        }
        case Expr::Iverson: {
            std::vector<BoxPtr> row;
            row.push_back(Box::makeText("\xE2\x84\xBA", Box::Symbol)); // 𝕀 approx; we use a styled I
            row.push_back(Box::makeDelimited("(", ")", buildInputBox(*e.lhs)));
            return Box::makeRow(std::move(row));
        }
    }
    return Box::makeText("?", Box::Normal);
}

BoxPtr buildBoxFromExpr(const Expr& e) { return buildInputBox(e); }

BoxPtr buildBox(const Value& v) {
    switch (v.type) {
        case Value::Rational: return numberBox(v.rat);
        case Value::Decimal: return Box::makeText(v.dec.toString(), Box::Number);
        case Value::Boolean: return Box::makeText(v.boolean ? "true" : "false", Box::Keyword);
        case Value::Set: return Box::makeText(v.set->toString(), Box::Normal);
        case Value::Symbolic: return v.sym ? buildInputBox(*v.sym) : Box::makeText("?", Box::Normal);
        case Value::Error: return Box::makeText("error", Box::Normal);
    }
    return Box::makeText("?", Box::Normal);
}

} // namespace scicalc
