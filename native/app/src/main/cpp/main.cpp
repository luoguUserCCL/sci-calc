// scicalc main — unified native entry point.
//   sci-calc                 -> interactive REPL
//   sci-calc "expr"          -> one-shot evaluate
//   sci-calc --base 16 "..." -> set number base
//   sci-calc --decimal "..." -> decimal output mode
//   sci-calc --math "..."    -> symbolic math output mode (default)
//   sci-calc --precision 80  -> set significant digits
//   sci-calc --fixed 6       -> fixed-point with 6 decimals
//   sci-calc --vars / --funcs / --list
//   sci-calc --gui           -> launch ImGui GUI (if built with GUI support)
//
// The binary is a single native executable with NO Java dependency.
#include "scicalc/Engine.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
  #include <io.h>
  #define SCICALC_ISATTY(fd) _isatty(fd)
#else
  #include <unistd.h>
  #define SCICALC_ISATTY(fd) isatty(fd)
#endif

#ifdef SCICALC_WITH_GUI
extern int runGui(int argc, char** argv, scicalc::Engine& engine);
#endif

using namespace scicalc;

static void printHelp() {
    std::cout <<
        "sci-calc — high-precision scientific calculator\n"
        "\n"
        "Usage:\n"
        "  sci-calc                  Interactive REPL (type :help inside)\n"
        "  sci-calc \"EXPR\"          Evaluate EXPR and print the result\n"
        "  sci-calc --gui            Launch the ImGui graphical interface\n"
        "\n"
        "Options:\n"
        "  --math        Symbolic math output (fractions, sqrt, pi) [default]\n"
        "  --decimal     Decimal output (high-precision float)\n"
        "  --base N      Integer display base (2, 8, 10, 16)\n"
        "  --precision N Significant digits (default 50)\n"
        "  --fixed N     Fixed-point with N digits after the decimal point\n"
        "  --scientific  Scientific notation\n"
        "  --vars        List defined variables\n"
        "  --funcs       List defined functions\n"
        "  --help, -h    Show this help\n"
        "\n"
        "Inside the REPL, prefix a line with ':' for commands (:help, :vars, :funcs,\n"
        ":math, :decimal, :base N, :precision N, :quit).\n";
}

static const char* styleNumber() { return ""; }
static const char* styleReset() { return ""; }

static void printResult(const EngineResult& r) {
    if (r.ok) {
        std::cout << "= " << r.output << "\n";
    } else {
        std::cout << "error: " << r.error << "\n";
    }
}

static void repl(Engine& eng) {
    bool tty = SCICALC_ISATTY(fileno(stdin));
    if (tty) std::cout << "sci-calc interactive REPL. Type :help for commands, :quit to exit.\n";
    std::string line;
    while (true) {
        if (tty) std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) { if (tty) std::cout << "\n"; break; }
        // trim
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        line = line.substr(a);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        if (line[0] == ':') {
            std::istringstream iss(line.substr(1));
            std::string cmd; iss >> cmd;
            if (cmd == "quit" || cmd == "q" || cmd == "exit") break;
            else if (cmd == "help" || cmd == "h" || cmd == "?") {
                std::cout << "Commands: :help :vars :funcs :math :decimal :base N :precision N :fixed N :scientific :del VAR :delfunc NAME :quit\n";
            } else if (cmd == "vars") {
                for (auto& [k, v] : eng.vars()) {
                    std::cout << "  " << k << " = " << eng.format(v) << "\n";
                }
            } else if (cmd == "funcs") {
                for (auto& [k, f] : eng.funcs()) {
                    std::cout << "  " << k << "(";
                    for (size_t i = 0; i < f.params.size(); ++i) { if (i) std::cout << ", "; std::cout << f.params[i]; }
                    std::cout << ")\n";
                }
            } else if (cmd == "math") { eng.config().outputMode = OutputMode::Math; std::cout << "  [math mode]\n"; }
            else if (cmd == "decimal") { eng.config().outputMode = OutputMode::Decimal; std::cout << "  [decimal mode]\n"; }
            else if (cmd == "scientific") { eng.config().numFormat = EngineConfig::NumFormat::Scientific; std::cout << "  [scientific format]\n"; }
            else if (cmd == "base") { int n; if (iss >> n) { eng.config().numberBase = n; std::cout << "  [base " << n << "]\n"; } }
            else if (cmd == "precision") { unsigned n; if (iss >> n) { eng.config().precision = n; std::cout << "  [precision " << n << "]\n"; } }
            else if (cmd == "fixed") { int n; if (iss >> n) { eng.config().numFormat = EngineConfig::NumFormat::FixedPoint; eng.config().fixedDigits = n; std::cout << "  [fixed " << n << "]\n"; } }
            else if (cmd == "del") { std::string n; if (iss >> n) { std::cout << (eng.delVar(n) ? "  deleted" : "  not found") << "\n"; } }
            else if (cmd == "delfunc") { std::string n; if (iss >> n) { std::cout << (eng.delFunc(n) ? "  deleted" : "  not found") << "\n"; } }
            else std::cout << "  unknown command: " << cmd << " (try :help)\n";
            continue;
        }
        EngineResult r = eng.evaluate(line);
        printResult(r);
    }
}

int main(int argc, char** argv) {
    Engine eng;
    std::vector<std::string> positional;
    bool wantGui = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printHelp(); return 0; }
        else if (a == "--math") eng.config().outputMode = OutputMode::Math;
        else if (a == "--decimal") eng.config().outputMode = OutputMode::Decimal;
        else if (a == "--scientific") eng.config().numFormat = EngineConfig::NumFormat::Scientific;
        else if (a == "--fixed") {
            if (i + 1 < argc) { eng.config().numFormat = EngineConfig::NumFormat::FixedPoint; eng.config().fixedDigits = std::atoi(argv[++i]); }
        }
        else if (a == "--base") { if (i + 1 < argc) eng.config().numberBase = std::atoi(argv[++i]); }
        else if (a == "--precision") { if (i + 1 < argc) { eng.config().precision = (unsigned)std::atoi(argv[++i]); BigFloat::defaultPrecision() = eng.config().precision; } }
        else if (a == "--vars") {
            for (auto& [k, v] : eng.vars()) std::cout << k << " = " << eng.format(v) << "\n";
            return 0;
        }
        else if (a == "--funcs") {
            for (auto& [k, f] : eng.funcs()) { std::cout << k << "("; for (size_t j=0;j<f.params.size();++j){ if(j) std::cout<<", "; std::cout<<f.params[j]; } std::cout << ")\n"; }
            return 0;
        }
        else if (a == "--gui") wantGui = true;
        else if (a == "--version") { std::cout << "sci-calc 1.0.0\n"; return 0; }
        else if (a.substr(0,2) == "--") { std::cerr << "unknown option: " << a << "\n"; }
        else positional.push_back(a);
    }
    BigFloat::defaultPrecision() = eng.config().precision;

#ifdef SCICALC_WITH_GUI
    if (wantGui) return runGui(argc, argv, eng);
#else
    if (wantGui) std::cerr << "GUI not built into this binary; using REPL.\n";
#endif

    if (!positional.empty()) {
        std::string expr = positional[0];
        EngineResult r = eng.evaluate(expr);
        printResult(r);
        return r.ok ? 0 : 1;
    }
    // No expression: REPL (handles both interactive and piped input;
    // ':' prefixed lines are commands, everything else is evaluated).
    repl(eng);
    return 0;
}
