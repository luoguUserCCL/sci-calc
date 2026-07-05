# sci-calc — Cross-platform High-Precision Scientific Calculator

A cross-platform scientific calculator with a custom high-precision math engine
(C++), a box-model formula rendering engine (no external LaTeX/MathJax/KaTeX),
a Dear ImGui GUI, a JNI bridge, and a Gradle (Groovy DSL) build producing
**four independent artifacts**: Linux / Windows / macOS native executables
(no JVM required) and a cross-platform Jar.

## Features

### Math engine (`native/engine`)
- **Exact arithmetic**: `BigInt` (arbitrary-precision integers, Knuth-D
  division, fast exponentiation, gcd/lcm, factorial, CSPRNG) and `BigRational`
  (exact fractions, floor/ceil/round, terminating/recurring decimal detection).
- **High-precision transcendentals**: `BigFloat` (decimal float) with
  sqrt/root (Newton), exp/ln/log (atanh series), sin/cos/tan (Taylor with
  proper factorial recurrence + 2π range reduction), asin/acos/atan, sinh/cosh/
  tanh, pow, π (Machin), e. No floating-point precision loss.
- **Two output modes**: *Math* (symbolic — fractions, √, π, sin kept symbolic,
  `sqrt(8)`→`2√2`) and *Decimal* (high-precision numeric).
- **Number bases**: 2/8/10/16 input & display (`0x`, `0b`, `0o`, `16#FF`).
- **Custom variables & functions** via `:=` (e.g. `f(n) := n*(n+1)`).

### Operator precedence (high → low)
1. `()`  2. function call  3. unary `+ - not`  4. `^` (right)
5. `* / %`  6. `+ -`  7. set `cap cup \`  8. relational `= < > <= >= != in subset realsubset` (chainable)
9. logical `and or`  10. `:=` (right)

### Function library
`abs |x|`, `floor ⌊x⌋`, `ceil ⌈x⌉`, `rand(min,max)` (CSPRNG),
`sum(i,a,b,expr) ∑`, `prod ∏`, `pow` (fast power), `gcd`, `lcm`,
`sqrt(a,b)/sqrt(b)`, `log(a,b)/log(b)`, `% mod`, full trig (+ inverse, hyperbolic),
`fact/n!`, `Iverson(P)`.

### Sets & Iverson bracket
- Enumerated `{1,2,3}`, intervals `[a,b] (a,b] (a,b) [a,b)`.
- Predefined: `Real ℝ`, `Rational/Quotient ℚ`, `Integer/Zahlen ℤ`.
- `cap ∩`, `cup ∪`, `\` (difference), `in ∈`, `subset ⊆`, `realsubset ⊊`.
- `Iverson(P)` renders as 𝕀(P) and evaluates to 1/0.

### Box-model formula rendering (`native/engine/Box.hpp`)
A backend-independent tree of boxes (Text, Row, Fraction, SupSub, Radical,
BigOp, Delimited, Function). The GUI's `BoxRenderer` walks it and draws with
ImGui draw lists. Verified by VLM: `sqrt(2)+sum(i,1,5,i^2)` renders as
√2 + Σ_{i=1}^{5} i².

### GUI (`native/app`, Dear ImGui + GLFW)
Math input area, formula render area (input echo + result), output, settings
(mode/base/precision/format), and History/Variables/Functions tabs. Loads a
Unicode font for math symbols.

## Build

```bash
source env.sh                 # configures JDK/Zig/MinGW/Gradle (auto-rebootstrap)
./gradlew buildAll            # Linux + Windows + macOS exes + Jar
./gradlew buildLinuxGui       # Linux ImGui GUI executable
```

Artifacts land in `build/distributions/`:
- `sci-calc-linux`, `sci-calc-windows.exe`, `sci-calc-macos`
- `jni/build/libs/sci-calc-1.0.0.jar` (with embedded `.so`/`.dll`/`.dylib`)

## Usage

```bash
./sci-calc "1/2 + 1/3"                 # = 5/6
./sci-calc "sqrt(2) * sqrt(8)"         # = (sqrt(2)*(2*sqrt(2)))
./sci-calc --decimal "pi"              # = 3.14159265358979323846264338328...
./sci-calc --base 16 "255 * 16"        # = 0xff0
./sci-calc "sum(i,1,100,i)"            # = 5050
./sci-calc "Iverson(2 < 3 < 10)"       # = 1
java -jar sci-calc-1.0.0.jar "100!"    # = 93326215443944...
```

## Architecture

```
native/engine   cpp-library   BigInt/BigRational/BigFloat/Lexer/Parser/Evaluator/Box
native/app      cpp-application   CLI REPL + (optional) ImGui GUI
jni             java + JNI   SciCalc.java + jni_bridge.cpp -> cross-platform Jar
third_party     ImGui, GLFW, GL headers, jni_md
scripts         base91.py (PAT decoder), bootstrap-tools.sh (toolchain self-heal)
```

## Toolchain (modern API, no `model { toolChains {} }`)
The `cpp-application`/`cpp-library` plugins auto-detect the host GCC. Cross
toolchains (MinGW-ucrt64, Zig) are registered in `sciCalcTools` (root
`build.gradle`) and invoked by Exec tasks that re-read paths at execution time
— surviving toolchain directory resets.
