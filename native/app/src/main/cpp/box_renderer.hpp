// box_renderer.hpp — Walks a scicalc::Box tree and lays it out / draws it with
// Dear ImGui draw lists. This is the on-screen realisation of the box-model
// formula rendering engine (no external LaTeX/MathJax/KaTeX).
#pragma once
#include "scicalc/Box.hpp"
#include <string>

struct ImFont;

namespace scicalc {

struct BoxMetrics {
    float width = 0;
    float height = 0;     // total height
    float ascent = 0;     // baseline offset from top
    float descent = 0;    // distance below baseline
};

class BoxRenderer {
public:
    BoxRenderer();

    // Font configuration. main = UI/identifier font; math = math-symbol font.
    void setFonts(ImFont* main, ImFont* math, ImFont* small);
    void setBigOpFont(ImFont* bigOp) { bigOpFont_ = bigOp; }
    void setAmsFont(ImFont* ams) { amsFont_ = ams; }  // KaTeX_AMS for double-struck
    void setMathItalicFont(ImFont* mi) { mathItalicFont_ = mi; }  // KaTeX_Math-Italic
    void setMainItalicFont(ImFont* mi) { mainItalicFont_ = mi; }  // KaTeX_Main-Italic (大写字母)
    void setScale(float px) { fontSize_ = px; }

    /// Compute layout metrics for a box tree (used to size the render area).
    BoxMetrics measure(const Box& b);

    /// Draw a box tree with its top-left at (x, y). Returns total width.
    float draw(const Box& b, float x, float y);

private:
    ImFont* main_ = nullptr;
    ImFont* math_ = nullptr;
    ImFont* small_ = nullptr;
    ImFont* bigOpFont_ = nullptr;  // 大运算符字体 (KaTeX Size1)
    ImFont* amsFont_ = nullptr;    // KaTeX_AMS (双线字母)
    ImFont* mathItalicFont_ = nullptr; // KaTeX_Math-Italic (小写字母、希腊、符号)
    ImFont* mainItalicFont_ = nullptr; // KaTeX_Main-Italic (大写字母)
    float fontSize_ = 20.0f;

    // internal layout+draw combined; measure() calls with draw=false.
    BoxMetrics layout(const Box& b, float x, float y, bool doDraw);
    ImFont* fontFor(const Box& b);
    float textWidth(const std::string& s, ImFont* f);
    float fontHeight(ImFont* f);
};

} // namespace scicalc
