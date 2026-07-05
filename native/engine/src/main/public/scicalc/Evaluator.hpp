// scicalc/Evaluator.hpp — AST evaluator producing Values.
#pragma once
#include "scicalc/Ast.hpp"
#include "scicalc/Value.hpp"
#include "scicalc/BigFloat.hpp"
#include <map>
#include <string>
#include <vector>
#include <functional>

namespace scicalc {

/// Output mode controls how transcendental/irrational results are represented.
enum class OutputMode { Math, Decimal };

struct EngineConfig {
    OutputMode outputMode = OutputMode::Math;
    enum class NumFormat { General, Scientific, FixedPoint, FixedSignificant, Float } numFormat = NumFormat::General;
    int numberBase = 10;          // display base for integers (2/8/10/16)
    unsigned precision = 50;      // significant digits for decimal mode
    unsigned fixedDigits = 6;     // digits after decimal point for FixedPoint
    bool showMixedNumbers = false;
};

struct UserFunc {
    std::vector<std::string> params;
    ExprPtr body;
};

class Evaluator {
public:
    Evaluator() = default;

    EngineConfig config;

    Value eval(const Expr& e);
    /// Evaluate with a one-shot local environment (for function calls / sum bounds).
    Value evalWith(const Expr& e, const std::vector<std::string>& names,
                   const std::vector<Value>& vals);

    // Environment access (for the management panel)
    bool hasVar(const std::string& n) const { return vars_.count(n) > 0; }
    Value getVar(const std::string& n) const;
    void setVar(const std::string& n, const Value& v) { vars_[n] = v; }
    bool delVar(const std::string& n) { return vars_.erase(n) > 0; }
    const std::map<std::string, Value>& vars() const { return vars_; }

    bool hasFunc(const std::string& n) const { return funcs_.count(n) > 0; }
    const UserFunc& getFunc(const std::string& n) const { return funcs_.at(n); }
    void setFunc(const std::string& n, UserFunc f) { funcs_[n] = std::move(f); }
    bool delFunc(const std::string& n) { return funcs_.erase(n) > 0; }
    const std::map<std::string, UserFunc>& funcs() const { return funcs_; }

private:
    std::map<std::string, Value> vars_;
    std::map<std::string, UserFunc> funcs_;

    Value evalBinary(BinOp op, const Value& a, const Value& b, const Expr& node);
    Value evalUnary(UnaryOp op, const Value& a);
    Value evalCall(const std::string& name, const std::vector<ExprPtr>& argExprs, const Expr& node);
    Value evalSetOp(BinOp op, const SetValuePtr& a, const SetValuePtr& b);
    Value evalRel(BinOp op, const Value& a, const Value& b);

    // numeric helpers
    Value powValues(const Value& base, const Value& exp);
    bool truthy(const Value& v);

    // built-in function dispatch
    Value builtin(const std::string& name, const std::vector<Value>& args, const Expr& node);
};

} // namespace scicalc
