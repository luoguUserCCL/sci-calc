// scicalc/Box.hpp — Box-model formula rendering tree.
//
// The engine builds a backend-independent tree of boxes describing the visual
// structure of a formula (fractions, super/subscripts, radicals, big operators,
// delimiters, ...). The GUI renderer walks this tree and computes pixel layout
// using the active fonts (Latin Modern Math + Noto Sans SC). NO external LaTeX
// / MathJax / KaTeX is involved.
#pragma once
#include "scicalc/Ast.hpp"
#include "scicalc/Value.hpp"
#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace scicalc {

struct Box;
using BoxPtr = std::unique_ptr<Box>;

struct Box {
    enum Kind {
        Text,       // a run of text (glyph string) with a style
        Row,        // horizontal sequence
        RowStacked, // vertical sequence (rare)
        Fraction,   // num / den
        SupSub,     // base with optional sup/sub
        Radical,    // sqrt(radicand) or root(index, radicand)
        BigOp,      // ∑ ∏ ∫ with limits + body
        Delimited,  // |x|, ⌊x⌋, ⌈x⌉, (x), [x]
        Padded,     // spacing wrapper
        Function,   // named function like sin, log_a b
    } kind;

    // Text
    std::string text;
    enum TextStyle { Normal, Identifier, Number, Operator, Symbol, Keyword } style = Normal;

    // Row / RowStacked / children
    std::vector<BoxPtr> children;

    // Fraction
    BoxPtr num, den;

    // SupSub
    BoxPtr base, sup, sub;

    // Radical
    BoxPtr radicand, index;

    // BigOp
    std::string opSymbol;     // "∑" "∏" "∫"
    BoxPtr lower, upper, body;

    // Delimited
    std::string leftDelim, rightDelim;  // "|", "⌊", "⌈", "(", "[", "" (empty=none)

    // Function (log_a b, sin x)
    std::string funcName;
    BoxPtr funcSub;     // for log base
    BoxPtr funcArg;     // argument (may be a Row)

    // Factory helpers
    static BoxPtr makeText(const std::string& s, TextStyle st = Normal);
    static BoxPtr makeRow(std::vector<BoxPtr> children);
    static BoxPtr makeFraction(BoxPtr n, BoxPtr d);
    static BoxPtr makeSupSub(BoxPtr base, BoxPtr sup, BoxPtr sub);
    static BoxPtr makeRadical(BoxPtr radicand, BoxPtr index = nullptr);
    static BoxPtr makeBigOp(const std::string& sym, BoxPtr lo, BoxPtr hi, BoxPtr body);
    static BoxPtr makeDelimited(const std::string& l, const std::string& r, BoxPtr inner);
    static BoxPtr makeFunction(const std::string& name, BoxPtr sub, BoxPtr arg);
    static BoxPtr makePadded(BoxPtr inner, float left = 0, float right = 0);
};

/// Build a box tree from a Value or an AST (symbolic form).
BoxPtr buildBox(const Value& v);
BoxPtr buildBoxFromExpr(const Expr& e);
/// Build a box for the input expression (echo) — pretty-prints the source.
BoxPtr buildInputBox(const Expr& e);

} // namespace scicalc
