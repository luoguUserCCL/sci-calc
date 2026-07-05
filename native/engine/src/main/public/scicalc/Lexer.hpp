// scicalc/Lexer.hpp — Tokenizer for the sci-calc language.
#pragma once
#include <string>
#include <vector>
#include <variant>
#include "scicalc/BigRational.hpp"

namespace scicalc {

enum class TokKind {
    Number, Ident, Keyword,
    Plus, Minus, Star, Slash, Percent, Caret, Backslash, Bang,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Comma,
    // multi-char
    Eq, Neq, Lt, Gt, Le, Ge,    // = != < > <= >=
    Assign,                      // :=
    // keywords (mapped from Ident when reserved)
    KW_and, KW_or, KW_not, KW_in, KW_subset, KW_realsubset,
    KW_cap, KW_cup,
    KW_Real, KW_Rational, KW_Quotient, KW_Integer, KW_Zahlen,
    End,
};

struct Token {
    TokKind kind;
    std::string text;        // raw text (for idents/keywords/numbers)
    BigRational value;       // for Number
    int pos = 0;             // source position
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : src_(src) {}
    std::vector<Token> tokenize();

    // Keywords that are reserved (cannot be variable names).
    static bool isKeyword(const std::string& lower);

private:
    std::string src_;
    size_t i_ = 0;

    Token readNumber();
    Token readIdent();
    void skipWs();
};

} // namespace scicalc
