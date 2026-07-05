# sci-calc — Project Initialization Plan

Cross-platform high-precision scientific calculator.
Core: C++ (engine + Dear ImGui GUI) · Bridge: JNI · Build: Gradle (Groovy DSL,
`cpp-application` / `cpp-library`) · Targets: Linux / Windows / macOS native + Jar.

---

## 1. New Gradle Toolchain API — Groovy DSL skeleton

The legacy Software-Model block:

```groovy
// ❌ DEPRECATED — legacy software-model toolchains
model {
    toolChains {
        gcc(Gcc) { ... }
    }
}
```

is replaced by the **modern Toolchain API**. With the native plugins
(`cpp-application`, `cpp-library`) we configure toolchains through the
`toolChains` container exposed on the native extension **and** declare
toolchain discovery through `toolchainManagement {}` in `settings.gradle`.

### `settings.gradle`

```groovy
pluginManagement {
    repositories { gradlePluginPortal(); mavenCentral() }
}

// --- New Toolchain API: central toolchain discovery/registration ---
toolchainManagement {
    // Custom toolchain repository that resolves cross-compilers
    // (MinGW-ucrt64, Zig) from a local tools/ prefix, with auto-rebootstrap.
    repositories {
        sciCalcTools(SciCalcToolchainRepository) {
            toolsDir = file("../tools")          // portable toolchains root
            autoRebootstrap = true                // fault tolerance (see §3)
        }
    }
}

rootProject.name = 'sci-calc'
include ':native:engine'     // cpp-library  — core math engine (libscicalc)
include ':native:gui'        // cpp-application — Dear ImGui GUI executable
include ':jni'               // java + cpp — JNI bridge + cross-platform Jar
```

### Root `build.gradle` (toolchain wiring)

```groovy
plugins {
    id 'cpp-application' apply false
    id 'cpp-library'     apply false
    id 'java'             apply false
}

// Modern Toolchain API: register each target toolchain as a named provider.
// No more `model { toolChains {} }`.
allprojects {
    // The cpp-* plugins expose a `toolChains` extension container; we register
    // native + cross toolchains here, each keyed by a target name.
    plugins.withId('cpp-application') {
        toolChains {
            // Linux host — GNU GCC (native)
            linuxGcc(GccToolChain) {
                path '/usr/bin'
                eachCompiler { it.executable = '/usr/bin/g++' }
            }
            // Windows — MinGW-w64 (ucrt64) cross
            windowsMingw(MinGWToolChain) {
                prefix 'x86_64-w64-mingw32ucrt-'
                path "${rootProject.toolsDir}/mingw-root/usr/bin"
            }
            // macOS — Zig as the C/C++ compiler (cross)
            macosZig(ClangToolChain) {
                eachCompiler { it.executable = "${rootProject.toolsDir}/zig-linux/zig cc" }
            }
        }
    }
}

// Target matrix: one variant per (platform) pair.
// Native executables link statically against the C++ runtime so they run
// without a JVM *and* without host C++ redist where possible.
ext.toolsDir = file("${rootDir}/../tools")  // shared with settings.gradle
```

> **Note on pragmatism:** Gradle's `cpp-application` cross-compile support is
> limited. To *guarantee* the four artifacts build reliably we additionally
> expose **`Exec`-based cross-compile tasks** (`buildWindowsExe`,
> `buildMacosExe`, `buildJar`) that invoke the registered toolchains directly.
> These tasks honor the same toolchain registry above, so the Toolchain API
> remains the single source of truth for *which* compiler to call.

---

## 2. Base91 Token decoder — security logic

The GitHub PAT is stored Base91-encoded. The decoder uses the **exact**
91-character alphabet mandated by the task (note: this alphabet intentionally
omits `'` and places `"` at the end — it is *not* the upstream basE91
alphabet, so a stock decoder would silently produce garbage).

**Alphabet** (91 chars):
```
ABCDEFGHIJKLMNOPQRSTUVWXYZ
abcdefghijklmnopqrstuvwxyz
0123456789
!#$%&()*+,./:;<=>?@[]^_`{|}~"
```

**Decoder algorithm** (implementation in `scripts/base91.py`, mirrored in C++
inside the JNI bridge for self-containment):

```cpp
// C++ mirror (self-contained, no deps) — used by tooling that must decode
// the PAT without Python available.
static const char* B91 =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789"
  "!#$%&()*+,./:;<=>?@[]^_`{|}~\"";
// build reverse map at startup; decode loop accumulates 13/14-bit groups.
```

**Security handling:**
1. The encoded token is never written to the repo. It lives only in the
   operator's invocation.
2. `scripts/decode-token.sh` reads the encoded string from a *non-versioned*
   file (`~/.gh-token-enc`) or env var `GH_PAT_B91`, decodes it, and writes
   the plaintext PAT to `~/.gh-token` with mode `0600`.
3. Git auth uses `credential.helper=store` pointed at `~/.git-credentials`
   (mode `0600`); the PAT is **never** embedded in remote URLs in committed
   files (which would leak via reflog/logs).
4. The plaintext PAT is never `echo`ed; verification prints only the
   `ghp_` prefix + last 4 chars.

---

## 3. Toolchain directory reset — fault tolerance

The sandbox warned that tool directories may be **reset at any time**.
Mitigations:

### 3.1 Single bootstrap script: `scripts/bootstrap-tools.sh`

Idempotent. Re-creates `/home/z/my-project/tools/` from scratch if missing.
Called automatically by `env.sh` and by the Gradle toolchain repository.

```
scripts/bootstrap-tools.sh
  ├── ensures JDK (Temurin 21 portable tar.gz)        -> tools/jdk-21.*
  ├── ensures Zig 0.16.0 (tar.xz)                     -> tools/zig-*
  ├── ensures MinGW-w64 ucrt64 (debs extracted w/ ar) -> tools/mingw-root/
  └── ensures Gradle 8.10.2                           -> tools/gradle-*
```

Each block:
- checks for the extracted directory; if present & passes `--version`, skip.
- else downloads (with `--retry` + resume `-C -`) and extracts.
- verifies by running `--version` / compiling a 1-line hello world.

### 3.2 `env.sh` auto-rebootstrap

`source env.sh` calls `bootstrap-tools.sh --check` which **only** re-runs the
missing pieces. So after a reset, re-sourcing `env.sh` self-heals.

### 3.3 Git = source of truth

Because tools may vanish, **all source code lives in git**, committed &
pushed after every module. A reset only costs tool re-download, never code
loss. Commit cadence: after every module/fix.

### 3.4 Build output isolation

Build artifacts go to `build/` (gitignored). The cross-compile `Exec` tasks
always re-derive compiler paths from `env.sh` at task-execution time (not
configuration time), so a mid-session reset is survived.

---

## Execution roadmap

| Phase | Deliverable |
|-------|-------------|
| A | Tools bootstrapped, repo pushed, env verified |
| B | `cpp-library` engine: bignum + lexer + AST parser (full precedence) + evaluator (decimal + symbolic) |
| C | Function library + set ops + Iverson bracket + custom vars/funcs (`:=`) |
| D | Box-model formula rendering engine (no external LaTeX) |
| E | Dear ImGui GUI (GLFW + Latin Modern Math + Noto Sans SC) |
| F | JNI bridge + Java wrapper + cross-platform Jar |
| G | Gradle cross-compile: Linux/Windows/macOS exes + Jar |
| H | Test GUI + CLI, publish GitHub Release with 4 assets |
