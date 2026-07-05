// scicalc/Parser.cpp
#include "scicalc/Parser.hpp"
#include <stdexcept>

namespace scicalc {

bool Parser::accept(TokKind k) {
    if (cur().kind == k) { ++pos_; return true; }
    return false;
}
void Parser::expect(TokKind k, const char* what) {
    if (cur().kind != k)
        throw std::invalid_argument(std::string("Parse error: expected ") + what +
                                    " but got '" + cur().text + "'");
    ++pos_;
}

ExprPtr Parser::parse() { return parseAssign(); }

bool Parser::fullyConsumed() const { return atEnd(); }

// level 10: := (right-assoc, lowest)
ExprPtr Parser::parseAssign() {
    // Detect "ident := expr"  or  "ident(params) := expr".
    // A call `ident(...)` is only a function DEFINITION if `:=` follows the
    // matching `)`. We tentatively scan for that pattern; otherwise it's a call.
    if (cur().kind == TokKind::Ident) {
        size_t save = pos_;
        std::string name = cur().text;
        ++pos_;
        if (cur().kind == TokKind::LParen) {
            // find matching ')'
            size_t depth = 0;
            size_t j = pos_;
            bool matched = false;
            for (; j < toks_.size(); ++j) {
                if (toks_[j].kind == TokKind::LParen) ++depth;
                else if (toks_[j].kind == TokKind::RParen) { --depth; if (depth == 0) { matched = true; break; } }
                else if (toks_[j].kind == TokKind::End) break;
            }
            if (matched && j + 1 < toks_.size() && toks_[j+1].kind == TokKind::Assign) {
                // It's a function definition: name(params) := body
                ++pos_; // consume (
                std::vector<std::string> params;
                if (cur().kind != TokKind::RParen) {
                    while (true) {
                        if (cur().kind != TokKind::Ident)
                            throw std::invalid_argument("Parse error: function parameter must be identifier");
                        params.push_back(cur().text);
                        ++pos_;
                        if (accept(TokKind::Comma)) continue;
                        break;
                    }
                }
                expect(TokKind::RParen, "')'");
                expect(TokKind::Assign, "':='");
                ExprPtr body = parseAssign();
                return Expr::makeAssignFunc(name, std::move(params), std::move(body));
            }
            // not a function def — fall through (rewind fully so it parses as a call)
            pos_ = save;
        } else if (accept(TokKind::Assign)) {
            ExprPtr body = parseAssign();
            return Expr::makeAssignVar(name, std::move(body));
        } else {
            pos_ = save; // not an assignment
        }
    }
    return parseLogicOr();
}

// level 9: or
ExprPtr Parser::parseLogicOr() {
    ExprPtr left = parseLogicAnd();
    while (cur().kind == TokKind::KW_or) {
        ++pos_;
        ExprPtr right = parseLogicAnd();
        left = Expr::makeBinary(BinOp::Or, std::move(left), std::move(right));
    }
    return left;
}

// level 9: and
ExprPtr Parser::parseLogicAnd() {
    ExprPtr left = parseRelational();
    while (cur().kind == TokKind::KW_and) {
        ++pos_;
        ExprPtr right = parseRelational();
        left = Expr::makeBinary(BinOp::And, std::move(left), std::move(right));
    }
    return left;
}

// level 8: relational (chainable: a < b < c => (a<b) and (b<c))
ExprPtr Parser::parseRelational() {
    ExprPtr left = parseSetOps();
    // collect a chain of relational operators
    struct Op { BinOp b; ExprPtr node; };
    std::vector<Op> chain;
    chain.push_back({BinOp::Add, std::move(left)});
    while (true) {
        BinOp b;
        switch (cur().kind) {
            case TokKind::Eq: b = BinOp::Eq; break;
            case TokKind::Neq: b = BinOp::Neq; break;
            case TokKind::Lt: b = BinOp::Lt; break;
            case TokKind::Gt: b = BinOp::Gt; break;
            case TokKind::Le: b = BinOp::Le; break;
            case TokKind::Ge: b = BinOp::Ge; break;
            case TokKind::KW_in: b = BinOp::In; break;
            case TokKind::KW_subset: b = BinOp::Subset; break;
            case TokKind::KW_realsubset: b = BinOp::RealSubset; break;
            default: goto done;
        }
        ++pos_;
        ExprPtr rhs = parseSetOps();
        chain.push_back({b, std::move(rhs)});
    }
done:
    if (chain.size() == 1) return std::move(chain[0].node);
    // build chained conjunction
    ExprPtr result;
    for (size_t k = 1; k < chain.size(); ++k) {
        ExprPtr cmp = Expr::makeBinary(chain[k].b, chain[k-1].node->clone(),
                                       chain[k].node->clone());
        result = result ? Expr::makeBinary(BinOp::And, std::move(result), std::move(cmp)) : std::move(cmp);
    }
    return result;
}

// level 7: cap cup \ (left-assoc)
ExprPtr Parser::parseSetOps() {
    ExprPtr left = parseAddSub();
    while (true) {
        BinOp b;
        if (cur().kind == TokKind::KW_cap) b = BinOp::SetIntersect;
        else if (cur().kind == TokKind::KW_cup) b = BinOp::SetUnion;
        else if (cur().kind == TokKind::Backslash) b = BinOp::SetDiff;
        else break;
        ++pos_;
        ExprPtr right = parseAddSub();
        left = Expr::makeBinary(b, std::move(left), std::move(right));
    }
    return left;
}

// level 6: + - (left-assoc)
ExprPtr Parser::parseAddSub() {
    ExprPtr left = parseMulDiv();
    while (cur().kind == TokKind::Plus || cur().kind == TokKind::Minus) {
        BinOp b = (cur().kind == TokKind::Plus) ? BinOp::Add : BinOp::Sub;
        ++pos_;
        ExprPtr right = parseMulDiv();
        left = Expr::makeBinary(b, std::move(left), std::move(right));
    }
    return left;
}

// level 5: * / % (left-assoc)
ExprPtr Parser::parseMulDiv() {
    ExprPtr left = parsePower();
    while (cur().kind == TokKind::Star || cur().kind == TokKind::Slash || cur().kind == TokKind::Percent) {
        BinOp b;
        if (cur().kind == TokKind::Star) b = BinOp::Mul;
        else if (cur().kind == TokKind::Slash) b = BinOp::Div;
        else b = BinOp::Mod;
        ++pos_;
        ExprPtr right = parsePower();
        left = Expr::makeBinary(b, std::move(left), std::move(right));
    }
    return left;
}

// level 4: ^ (right-assoc)
ExprPtr Parser::parsePower() {
    ExprPtr base = parseUnary();
    if (cur().kind == TokKind::Caret) {
        ++pos_;
        ExprPtr exp = parsePower(); // right-assoc
        return Expr::makeBinary(BinOp::Pow, std::move(base), std::move(exp));
    }
    return base;
}

// level 3: unary + - not (right-assoc)
ExprPtr Parser::parseUnary() {
    if (cur().kind == TokKind::Plus) { ++pos_; return parseUnary(); }
    if (cur().kind == TokKind::Minus) { ++pos_; return Expr::makeUnary(UnaryOp::Neg, parseUnary()); }
    if (cur().kind == TokKind::KW_not) { ++pos_; return Expr::makeUnary(UnaryOp::Not, parseUnary()); }
    return parsePostfix();
}

// level 2: function call / postfix
ExprPtr Parser::parsePostfix() {
    ExprPtr node = parsePrimary();
    while (true) {
        if (cur().kind == TokKind::LParen) {
            // function call
            ++pos_; // (
            std::vector<ExprPtr> args;
            if (cur().kind != TokKind::RParen) {
                while (true) {
                    args.push_back(parseAssign());
                    if (accept(TokKind::Comma)) continue;
                    break;
                }
            }
            expect(TokKind::RParen, "')'");
            // node must be a Var to call
            if (node->kind != Expr::Var)
                throw std::invalid_argument("Parse error: call on non-identifier");
            std::string name = node->name;
            node = Expr::makeCall(name, std::move(args));
        } else if (cur().kind == TokKind::Bang) {
            // postfix factorial: n!
            ++pos_;
            if (node->kind == Expr::Var || node->kind == Expr::Number || node->kind == Expr::Call)
                node = Expr::makeCall("fact", std::move(node));
            else
                throw std::invalid_argument("Parse error: '!' applied to invalid operand");
        } else break;
    }
    return node;
}

// level 1: primary / parens / atoms
ExprPtr Parser::parsePrimary() {
    const Token& t = cur();
    switch (t.kind) {
        case TokKind::Number: ++pos_; return Expr::makeNumber(t.value);
        case TokKind::Ident: ++pos_; return Expr::makeVar(t.text);
        case TokKind::KW_Real: ++pos_; return Expr::makeSetName("Real");
        case TokKind::KW_Rational:
        case TokKind::KW_Quotient: ++pos_; return Expr::makeSetName("Rational");
        case TokKind::KW_Integer:
        case TokKind::KW_Zahlen: ++pos_; return Expr::makeSetName("Integer");
        case TokKind::LParen: {
            ++pos_; // (
            // could be grouping or open interval (a,b] / (a,b)
            ExprPtr first = parseAssign();
            if (accept(TokKind::Comma)) {
                ExprPtr second = parseAssign();
                // must be followed by ) or ]
                SetIntervalKind k = SetIntervalKind::OpenOpen;
                if (accept(TokKind::RParen)) k = SetIntervalKind::OpenOpen;
                else if (accept(TokKind::RBracket)) k = SetIntervalKind::OpenClosed;
                else throw std::invalid_argument("Parse error: expected ')' or ']' after interval");
                return Expr::makeInterval(k, std::move(first), std::move(second));
            }
            expect(TokKind::RParen, "')'");
            return first;
        }
        case TokKind::LBracket: {
            ++pos_; // [
            ExprPtr first = parseAssign();
            if (accept(TokKind::Comma)) {
                ExprPtr second = parseAssign();
                SetIntervalKind k = SetIntervalKind::ClosedOpen;
                if (accept(TokKind::RParen)) k = SetIntervalKind::ClosedOpen;
                else if (accept(TokKind::RBracket)) k = SetIntervalKind::ClosedClosed;
                else throw std::invalid_argument("Parse error: expected ')' or ']' after interval");
                return Expr::makeInterval(k, std::move(first), std::move(second));
            }
            expect(TokKind::RBracket, "']'");
            return first; // [ expr ] grouping (rare) 
        }
        case TokKind::LBrace: {
            ++pos_; // {
            std::vector<ExprPtr> elems;
            if (cur().kind != TokKind::RBrace) {
                while (true) {
                    elems.push_back(parseAssign());
                    if (accept(TokKind::Comma)) continue;
                    break;
                }
            }
            expect(TokKind::RBrace, "'}'");
            return Expr::makeSetEnum(std::move(elems));
        }
        default:
            throw std::invalid_argument(std::string("Parse error: unexpected token '") + t.text + "'");
    }
}

} // namespace scicalc
