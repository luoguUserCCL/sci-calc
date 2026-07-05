// scicalc/Lexer.cpp
#include "scicalc/Lexer.hpp"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace scicalc {

static const std::unordered_map<std::string, TokKind> kKeywords = {
    {"and", TokKind::KW_and},
    {"or", TokKind::KW_or},
    {"not", TokKind::KW_not},
    {"in", TokKind::KW_in},
    {"subset", TokKind::KW_subset},
    {"realsubset", TokKind::KW_realsubset},
    {"cap", TokKind::KW_cap},
    {"cup", TokKind::KW_cup},
    {"cong", TokKind::KW_cong},
    {"mod", TokKind::KW_mod},
    {"real", TokKind::KW_Real},
    {"reals", TokKind::KW_Real},
    {"rational", TokKind::KW_Rational},
    {"rationals", TokKind::KW_Rational},
    {"quotient", TokKind::KW_Quotient},
    {"quotients", TokKind::KW_Quotient},
    {"integer", TokKind::KW_Integer},
    {"integers", TokKind::KW_Integer},
    {"zahlen", TokKind::KW_Zahlen},
};

bool Lexer::isKeyword(const std::string& lower) {
    return kKeywords.count(lower) > 0;
}

void Lexer::skipWs() {
    while (i_ < src_.size()) {
        char c = src_[i_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i_; continue; }
        // comments: # ... to end of line
        if (c == '#') { while (i_ < src_.size() && src_[i_] != '\n') ++i_; continue; }
        break;
    }
}

Token Lexer::readNumber() {
    // Supports: decimal, 0x/0b/0o prefixes, base#digits (e.g. 16#FF), fraction via '/'
    // is handled at parser level (division). Exponent e/E for decimals.
    Token t; t.pos = (int)i_;
    size_t start = i_;
    // detect base prefix
    if (i_ + 1 < src_.size() && src_[i_] == '0') {
        char p = src_[i_+1];
        if (p == 'x' || p == 'X') {
            i_ += 2;
            size_t s = i_;
            while (i_ < src_.size() && (std::isxdigit((unsigned char)src_[i_]) || src_[i_]=='.' )) ++i_;
            std::string num = src_.substr(s, i_ - s);
            t.text = src_.substr(start, i_ - start);
            t.kind = TokKind::Number;
            t.value = BigRational::fromBase(num, 16);
            return t;
        }
        if (p == 'b' || p == 'B') {
            i_ += 2;
            size_t s = i_;
            while (i_ < src_.size() && (src_[i_]=='0'||src_[i_]=='1'||src_[i_]=='.' )) ++i_;
            std::string num = src_.substr(s, i_ - s);
            t.text = src_.substr(start, i_ - start);
            t.kind = TokKind::Number;
            t.value = BigRational::fromBase(num, 2);
            return t;
        }
        if (p == 'o' || p == 'O') {
            i_ += 2;
            size_t s = i_;
            while (i_ < src_.size() && ((src_[i_]>='0'&&src_[i_]<='7')||src_[i_]=='.')) ++i_;
            std::string num = src_.substr(s, i_ - s);
            t.text = src_.substr(start, i_ - start);
            t.kind = TokKind::Number;
            t.value = BigRational::fromBase(num, 8);
            return t;
        }
    }
    // base#digits form, e.g. 16#FF or 2#1010
    if (i_ < src_.size() && std::isdigit((unsigned char)src_[i_])) {
        size_t bs = i_;
        while (i_ < src_.size() && std::isdigit((unsigned char)src_[i_])) ++i_;
        if (i_ < src_.size() && src_[i_] == '#') {
            std::string baseStr = src_.substr(bs, i_ - bs);
            int base = std::stoi(baseStr);
            ++i_; // skip #
            size_t s = i_;
            while (i_ < src_.size() && (std::isalnum((unsigned char)src_[i_]) || src_[i_]=='.' )) ++i_;
            std::string num = src_.substr(s, i_ - s);
            t.text = src_.substr(start, i_ - start);
            t.kind = TokKind::Number;
            t.value = BigRational::fromBase(num, base);
            return t;
        }
        i_ = bs; // not base# form, rewind
    }
    // plain decimal (with optional . and exponent)
    while (i_ < src_.size() && (std::isdigit((unsigned char)src_[i_]) || src_[i_]=='.' )) ++i_;
    if (i_ < src_.size() && (src_[i_]=='e' || src_[i_]=='E')) {
        size_t save = i_;
        ++i_;
        if (i_ < src_.size() && (src_[i_]=='+'||src_[i_]=='-')) ++i_;
        if (i_ < src_.size() && std::isdigit((unsigned char)src_[i_])) {
            while (i_ < src_.size() && std::isdigit((unsigned char)src_[i_])) ++i_;
        } else {
            i_ = save; // not an exponent
        }
    }
    std::string num = src_.substr(start, i_ - start);
    t.text = num;
    t.kind = TokKind::Number;
    t.value = BigRational::fromDecimal(num);
    return t;
}

Token Lexer::readIdent() {
    Token t; t.pos = (int)i_;
    size_t start = i_;
    while (i_ < src_.size() && (std::isalnum((unsigned char)src_[i_]) || src_[i_]=='_')) ++i_;
    std::string id = src_.substr(start, i_ - start);
    t.text = id;
    std::string lower = id;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
    auto it = kKeywords.find(lower);
    if (it != kKeywords.end()) {
        t.kind = it->second;
    } else {
        t.kind = TokKind::Ident;
    }
    return t;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    while (true) {
        skipWs();
        if (i_ >= src_.size()) break;
        char c = src_[i_];
        if (std::isdigit((unsigned char)c) ||
            (c == '.' && i_ + 1 < src_.size() && std::isdigit((unsigned char)src_[i_+1]))) {
            out.push_back(readNumber());
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            out.push_back(readIdent());
            continue;
        }
        Token t; t.pos = (int)i_; t.text = std::string(1, c);
        switch (c) {
            case '+': t.kind = TokKind::Plus; ++i_; out.push_back(t); continue;
            case '-': t.kind = TokKind::Minus; ++i_; out.push_back(t); continue;
            case '*': t.kind = TokKind::Star; ++i_; out.push_back(t); continue;
            case '/': t.kind = TokKind::Slash; ++i_; out.push_back(t); continue;
            case '%': t.kind = TokKind::Percent; ++i_; out.push_back(t); continue;
            case '^': t.kind = TokKind::Caret; ++i_; out.push_back(t); continue;
            case '\\': t.kind = TokKind::Backslash; ++i_; out.push_back(t); continue;
            case '(': t.kind = TokKind::LParen; ++i_; out.push_back(t); continue;
            case ')': t.kind = TokKind::RParen; ++i_; out.push_back(t); continue;
            case '[': t.kind = TokKind::LBracket; ++i_; out.push_back(t); continue;
            case ']': t.kind = TokKind::RBracket; ++i_; out.push_back(t); continue;
            case '{': t.kind = TokKind::LBrace; ++i_; out.push_back(t); continue;
            case '}': t.kind = TokKind::RBrace; ++i_; out.push_back(t); continue;
            case ',': t.kind = TokKind::Comma; ++i_; out.push_back(t); continue;
            case ':':
                if (i_ + 1 < src_.size() && src_[i_+1] == '=') {
                    t.kind = TokKind::Assign; t.text = ":="; i_ += 2; out.push_back(t); continue;
                }
                throw std::invalid_argument("Lexer: unexpected ':' (did you mean ':='?)");
            case '=': t.kind = TokKind::Eq; ++i_; out.push_back(t); continue;
            case '<':
                if (i_ + 1 < src_.size() && src_[i_+1] == '=') { t.kind = TokKind::Le; t.text = "<="; i_ += 2; }
                else { t.kind = TokKind::Lt; ++i_; }
                out.push_back(t); continue;
            case '>':
                if (i_ + 1 < src_.size() && src_[i_+1] == '=') { t.kind = TokKind::Ge; t.text = ">="; i_ += 2; }
                else { t.kind = TokKind::Gt; ++i_; }
                out.push_back(t); continue;
            case '!':
                if (i_ + 1 < src_.size() && src_[i_+1] == '=') { t.kind = TokKind::Neq; t.text = "!="; i_ += 2; out.push_back(t); continue; }
                t.kind = TokKind::Bang; ++i_; out.push_back(t); continue;
            default:
                throw std::invalid_argument(std::string("Lexer: unexpected character '") + c + "'");
        }
    }
    Token end; end.kind = TokKind::End; end.pos = (int)i_; out.push_back(end);
    return out;
}

} // namespace scicalc
