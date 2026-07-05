// scicalc/Engine.hpp — Top-level facade: lexer + parser + evaluator + formatter.
#pragma once
#include "scicalc/Evaluator.hpp"
#include "scicalc/Box.hpp"
#include <string>
#include <memory>

namespace scicalc {

struct EngineResult {
    bool ok = false;
    std::string output;          // formatted result string
    std::string error;           // error message if !ok
    Value value;                 // the raw value
    BoxPtr renderTree;           // box-model tree for GUI rendering (may be null)
    std::string inputEcho;       // the parsed input (normalized)
};

class Engine {
public:
    Engine() { ev.config.precision = 50; }

    EngineConfig& config() { return ev.config; }
    const EngineConfig& config() const { return ev.config; }

    /// Evaluate a source expression. Returns a formatted result.
    EngineResult evaluate(const std::string& src);

    // Environment passthrough
    Evaluator& evaluator() { return ev; }
    const std::map<std::string, Value>& vars() const { return ev.vars(); }
    const std::map<std::string, UserFunc>& funcs() const { return ev.funcs(); }
    bool delVar(const std::string& n) { return ev.delVar(n); }
    bool delFunc(const std::string& n) { return ev.delFunc(n); }

    /// Format a Value according to current config (also used for sub-values).
    std::string format(const Value& v) const;

private:
    Evaluator ev;
    std::string formatRational(const BigRational& r) const;
    std::string formatDecimal(const BigFloat& d) const;
};

} // namespace scicalc
