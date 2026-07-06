// scicalc/Parser.hpp — Recursive-descent parser implementing the full
// operator-precedence table mandated by the task.
#pragma once
#include "scicalc/Lexer.hpp"
#include "scicalc/Ast.hpp"
#include <vector>

namespace scicalc {

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : toks_(std::move(toks)) {}
    /// Parse a full expression (may be an assignment/definition).
    ExprPtr parse();

    /// True if the remaining input (after parsing) is just End.
    bool fullyConsumed() const;

private:
    std::vector<Token> toks_;
    size_t pos_ = 0;

    const Token& peek(int o = 0) const { return toks_[std::min(pos_ + o, toks_.size() - 1)]; }
    const Token& cur() const { return toks_[pos_]; }
    bool atEnd() const { return cur().kind == TokKind::End; }
    bool accept(TokKind k);
    void expect(TokKind k, const char* what);

    // precedence layers (low -> high)
    ExprPtr parseAssign();        // :=                  (right, level 10)
    ExprPtr parseLogicOr();       // or                  (left, level 9)
    ExprPtr parseLogicAnd();      // and                 (left, level 9)
    ExprPtr parseRelational();    // = != < > <= >= in subset realsubset cong (chainable, level 8)
    ExprPtr parseRelationalRest(ExprPtr first);  // cong 回退后的普通关系链
    ExprPtr parseSetOps();        // cap cup \           (left, level 7)
    ExprPtr parseAddSub();        // + -                 (left, level 6)
    ExprPtr parseMulDiv();        // * / % + 隐式乘法  (left, level 5)
    bool isImplicitMul();         // 判断当前是否为隐式乘法上下文
    ExprPtr parsePower();         // ^                   (right, level 4)
    ExprPtr parseUnary();         // unary + - not       (right, level 3)
    ExprPtr parsePostfix();       // function call       (level 2)
    ExprPtr parsePrimary();       // parens / atoms      (level 1)

    ExprPtr parseIntervalOrGroup(); // [ ] ( ) with two exprs -> interval
    ExprPtr parseSetEnumOrEmpty();  // { ... }
};

} // namespace scicalc
