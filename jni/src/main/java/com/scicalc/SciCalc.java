package com.scicalc;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Locale;

/**
 * SciCalc — Java/JNI wrapper around the native C++ sci-calc engine.
 *
 * <p>The native library for the running platform is bundled inside the Jar
 * under {@code /native/<os>-<arch>/} and extracted to a temp file on first
 * load, so the Jar is fully self-contained and cross-platform.
 *
 * <p>Example:
 * <pre>
 *   try (SciCalc calc = new SciCalc()) {
 *       calc.setMathMode();
 *       System.out.println(calc.evaluate("1/2 + 1/3"));   // 5/6
 *       System.out.println(calc.evaluate("sqrt(2) * sqrt(8)"));
 *       System.out.println(calc.evaluate("sum(i,1,100,i)")); // 5050
 *   }
 * </pre>
 */
public class SciCalc implements AutoCloseable {

    static {
        loadNative();
    }

    private static void loadNative() {
        String os = osName();
        String arch = archName();
        String libName = libName();
        String resource = "/native/" + os + "-" + arch + "/" + libName;
        try (InputStream in = SciCalc.class.getResourceAsStream(resource)) {
            if (in == null)
                throw new UnsatisfiedLinkError("bundled native library not found: " + resource);
            Path tmp = Files.createTempFile("scicalc-" + os + "-" + arch, libExt());
            tmp.toFile().deleteOnExit();
            Files.copy(in, tmp, StandardCopyOption.REPLACE_EXISTING);
            System.load(tmp.toAbsolutePath().toString());
        } catch (IOException e) {
            throw new UnsatisfiedLinkError("failed to extract native library: " + e.getMessage());
        }
    }

    private static String osName() {
        String s = System.getProperty("os.name").toLowerCase(Locale.ROOT);
        if (s.contains("linux")) return "linux";
        if (s.contains("windows")) return "windows";
        if (s.contains("mac") || s.contains("darwin")) return "macos";
        return "unknown";
    }
    private static String archName() {
        String a = System.getProperty("os.arch").toLowerCase(Locale.ROOT);
        if (a.contains("x86_64") || a.contains("amd64")) return "x86_64";
        if (a.contains("aarch64") || a.contains("arm64")) return "aarch64";
        return a;
    }
    private static String libName() {
        // Platform-conventional file name (libscicalc.so / libscicalc.dylib / scicalc.dll).
        String os = osName();
        String base = os.equals("windows") ? "scicalc" : "libscicalc";
        return base + libExt();
    }
    private static String libBase() { return "scicalc"; }
    private static String libExt() {
        String os = osName();
        if (os.equals("windows")) return ".dll";
        if (os.equals("macos")) return ".dylib";
        return ".so";
    }

    private final long handle;

    public SciCalc() { this.handle = nativeCreate(); }
    @Override public void close() { if (handle != 0) nativeDestroy(handle); }

    /** Evaluate an expression and return the formatted result (or "error: ..."). */
    public String evaluate(String expr) { return nativeEvaluate(handle, expr); }

    public void setMathMode() { nativeSetMathMode(handle); }
    public void setDecimalMode() { nativeSetDecimalMode(handle); }
    public void setBase(int base) { nativeSetBase(handle, base); }
    public void setPrecision(int digits) { nativeSetPrecision(handle, digits); }
    public void setFixedDigits(int n) { nativeSetFixedDigits(handle, n); }

    /** Defined variables, one per line as "name=value". */
    public String variables() { return nativeVars(handle); }
    /** Defined functions, one per line as "name(params)". */
    public String functions() { return nativeFuncs(handle); }
    public boolean deleteVariable(String name) { return nativeDelVar(handle, name); }
    public boolean deleteFunction(String name) { return nativeDelFunc(handle, name); }

    // --- native methods ---
    private static native long nativeCreate();
    private static native void nativeDestroy(long handle);
    private static native String nativeEvaluate(long handle, String expr);
    private static native void nativeSetMathMode(long handle);
    private static native void nativeSetDecimalMode(long handle);
    private static native void nativeSetBase(long handle, int base);
    private static native void nativeSetPrecision(long handle, int prec);
    private static native void nativeSetFixedDigits(long handle, int n);
    private static native String nativeVars(long handle);
    private static native String nativeFuncs(long handle);
    private static native boolean nativeDelVar(long handle, String name);
    private static native boolean nativeDelFunc(long handle, String name);
}
