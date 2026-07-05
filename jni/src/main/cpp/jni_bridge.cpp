// jni/src/main/cpp/jni_bridge.cpp — JNI bridge between Java and the C++ engine.
//
// A SciCalc Java instance holds a native handle (long) to a heap-allocated
// scicalc::Engine. The native library for the running platform is extracted
// from the Jar at load time (see SciCalc.java).
#include "scicalc/Engine.hpp"
#include <jni.h>
#include <memory>
#include <string>

using namespace scicalc;

namespace {
Engine* asEngine(jlong handle) { return reinterpret_cast<Engine*>(handle); }
}

extern "C" {

JNIEXPORT jlong JNICALL Java_com_scicalc_SciCalc_nativeCreate(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(new Engine());
}
JNIEXPORT void JNICALL Java_com_scicalc_SciCalc_nativeDestroy(JNIEnv*, jclass, jlong handle) {
    delete asEngine(handle);
}

JNIEXPORT jstring JNICALL Java_com_scicalc_SciCalc_nativeEvaluate(JNIEnv* env, jclass, jlong handle, jstring jexpr) {
    const char* s = env->GetStringUTFChars(jexpr, nullptr);
    EngineResult r = asEngine(handle)->evaluate(s ? s : "");
    env->ReleaseStringUTFChars(jexpr, s);
    std::string out = r.ok ? r.output : ("error: " + r.error);
    return env->NewStringUTF(out.c_str());
}

JNIEXPORT void JNICALL Java_com_scicalc_SciCalc_nativeSetMathMode(JNIEnv*, jclass, jlong handle) {
    asEngine(handle)->config().outputMode = OutputMode::Math;
}
JNIEXPORT void JNICALL Java_com_scicalc_SciCalc_nativeSetDecimalMode(JNIEnv*, jclass, jlong handle) {
    asEngine(handle)->config().outputMode = OutputMode::Decimal;
}
JNIEXPORT void JNICALL Java_com_scicalc_SciCalc_nativeSetBase(JNIEnv*, jclass, jlong handle, jint base) {
    asEngine(handle)->config().numberBase = base;
}
JNIEXPORT void JNICALL Java_com_scicalc_SciCalc_nativeSetPrecision(JNIEnv*, jclass, jlong handle, jint prec) {
    asEngine(handle)->config().precision = prec;
    BigFloat::defaultPrecision() = prec;
}
JNIEXPORT void JNICALL Java_com_scicalc_SciCalc_nativeSetFixedDigits(JNIEnv*, jclass, jlong handle, jint n) {
    asEngine(handle)->config().numFormat = EngineConfig::NumFormat::FixedPoint;
    asEngine(handle)->config().fixedDigits = n;
}
JNIEXPORT jstring JNICALL Java_com_scicalc_SciCalc_nativeVars(JNIEnv* env, jclass, jlong handle) {
    std::string s;
    for (auto& [k, v] : asEngine(handle)->vars()) { s += k; s += "="; s += asEngine(handle)->format(v); s += "\n"; }
    return env->NewStringUTF(s.c_str());
}
JNIEXPORT jstring JNICALL Java_com_scicalc_SciCalc_nativeFuncs(JNIEnv* env, jclass, jlong handle) {
    std::string s;
    for (auto& [k, f] : asEngine(handle)->funcs()) {
        s += k; s += "(";
        for (size_t i = 0; i < f.params.size(); ++i) { if (i) s += ", "; s += f.params[i]; }
        s += ")\n";
    }
    return env->NewStringUTF(s.c_str());
}
JNIEXPORT jboolean JNICALL Java_com_scicalc_SciCalc_nativeDelVar(JNIEnv* env, jclass, jlong handle, jstring jn) {
    const char* s = env->GetStringUTFChars(jn, nullptr);
    std::string name(s ? s : "");
    env->ReleaseStringUTFChars(jn, s);
    return asEngine(handle)->delVar(name) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jboolean JNICALL Java_com_scicalc_SciCalc_nativeDelFunc(JNIEnv* env, jclass, jlong handle, jstring jn) {
    const char* s = env->GetStringUTFChars(jn, nullptr);
    std::string name(s ? s : "");
    env->ReleaseStringUTFChars(jn, s);
    return asEngine(handle)->delFunc(name) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
