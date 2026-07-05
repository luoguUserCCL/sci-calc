package com.scicalc;

/**
 * Command-line entry point for the Jar: evaluates one expression or runs a
 * tiny demo. Demonstrates that the cross-platform Jar loads its bundled native
 * library and computes results without any external JVM-side math dependency.
 */
public class Main {
    public static void main(String[] args) {
        try (SciCalc calc = new SciCalc()) {
            if (args.length > 0) {
                StringBuilder sb = new StringBuilder();
                for (String a : args) { if (sb.length() > 0) sb.append(' '); sb.append(a); }
                System.out.println("= " + calc.evaluate(sb.toString()));
                return;
            }
            // demo
            String[] demo = { "1/2 + 1/3", "2^100", "sqrt(2) * sqrt(8)", "sum(i,1,100,i)",
                              "Iverson(2 < 3 < 10)", "100!", "pi", "sin(pi/6)" };
            for (String e : demo) System.out.println(e + " = " + calc.evaluate(e));
        }
    }
}
