// box_renderer.cpp — layout + draw the Box tree with ImGui.
#include "box_renderer.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace scicalc {

BoxRenderer::BoxRenderer() = default;
void BoxRenderer::setFonts(ImFont* main, ImFont* math, ImFont* small) {
    main_ = main; math_ = math; small_ = small;
}

static ImFont* fallback(ImFont* f, ImFont* def) { return f ? f : def; }

ImFont* BoxRenderer::fontFor(const Box& b) {
    // 双线字母 (𝕀, ℝ, ℚ, ℤ) 用 KaTeX_AMS 字体 (字号已在 gui.cpp 中补偿)
    if (b.style == Box::DoubleStruck) return fallback(amsFont_, math_);
    // Number 和单字母 Identifier 用 KaTeX_Math-Italic (数学斜体)
    if (b.style == Box::Number) return fallback(mathItalicFont_, math_);
    if (b.style == Box::Identifier && b.text.size() <= 1)
        return fallback(mathItalicFont_, math_);
    // 多字符函数名/变量名、运算符、符号等用 KaTeX_Main
    return fallback(math_, ImGui::GetFont());
}

float BoxRenderer::textWidth(const std::string& s, ImFont* f) {
    f = fallback(f, ImGui::GetFont());
    return f->CalcTextSizeA(fontSize_, FLT_MAX, 0.0f, s.c_str()).x;
}
float BoxRenderer::fontHeight(ImFont* f) {
    f = fallback(f, ImGui::GetFont());
    return f->FontSize;
}

BoxMetrics BoxRenderer::measure(const Box& b) { return layout(b, 0, 0, false); }

float BoxRenderer::draw(const Box& b, float x, float y) {
    auto m = layout(b, x, y, true);
    return m.width;
}

BoxMetrics BoxRenderer::layout(const Box& b, float x, float y, bool doDraw) {
    BoxMetrics m;
    auto* dl = ImGui::GetWindowDrawList();
    switch (b.kind) {
        case Box::Text: {
            ImFont* f = fontFor(b);
            std::string s = b.text;
            float w = textWidth(s, f);
            float h = fontHeight(f);
            m.width = w;
            m.height = h;
            m.ascent = h * 0.8f;
            m.descent = h * 0.2f;
            if (doDraw) {
                ImGui::PushFont(f);
                ImGui::SetWindowFontScale(1.0f);
                dl->AddText(f, fontSize_, ImVec2(x, y), IM_COL32(0, 0, 0, 255), s.c_str());
                ImGui::PopFont();
            }
            break;
        }
        case Box::Row: {
            float cx = x;
            float maxAsc = 0, maxDesc = 0;
            for (auto& c : b.children) {
                BoxMetrics cm = layout(*c, cx, y, doDraw);
                // align baselines: we draw children at y; their ascent defines baseline.
                cx += cm.width + 6.0f; // inter-box gap (函数之间留空)
                maxAsc = std::max(maxAsc, cm.ascent);
                maxDesc = std::max(maxDesc, cm.descent);
            }
            if (!b.children.empty()) cx -= 6.0f; // remove trailing gap
            m.width = cx - x;
            m.height = maxAsc + maxDesc;
            m.ascent = maxAsc;
            m.descent = maxDesc;
            break;
        }
        case Box::RowStacked: {
            float cy = y; float w = 0;
            for (auto& c : b.children) {
                BoxMetrics cm = layout(*c, x, cy, doDraw);
                cy += cm.height + 2.0f;
                w = std::max(w, cm.width);
            }
            m.width = w; m.height = cy - y; m.ascent = m.height * 0.8f; m.descent = m.height * 0.2f;
            break;
        }
        case Box::Fraction: {
            BoxMetrics nm = layout(*b.num, x, y, false);
            BoxMetrics dm = layout(*b.den, x, y, false);
            float w = std::max(nm.width, dm.width) + 8.0f;
            float numH = nm.height, denH = dm.height;
            float barGap = 3.0f;
            float numY = y;
            float denY = y + numH + barGap;
            m.width = w;
            m.height = numH + denH + barGap * 2;
            m.ascent = numH + barGap;
            m.descent = denH + barGap;
            if (doDraw) {
                layout(*b.num, x + (w - nm.width) / 2, numY, true);
                layout(*b.den, x + (w - dm.width) / 2, denY, true);
                float barY = y + numH + barGap;
                dl->AddLine(ImVec2(x, barY), ImVec2(x + w, barY), IM_COL32(0, 0, 0, 255), 1.5f);
            }
            break;
        }
        case Box::SupSub: {
            BoxMetrics bm;
            if (b.base) bm = layout(*b.base, x, y, false);
            float smallSize = fontSize_ * 0.65f;
            float baseW = bm.width;
            float curX = x + baseW;
            float asc = bm.ascent, desc = bm.descent;
            float subW = 0, supW = 0;
            if (b.sup) {
                float oldSize = fontSize_;
                fontSize_ = smallSize;
                BoxMetrics sm = layout(*b.sup, curX, y - (b.base ? bm.ascent * 0.45f : 0), doDraw);
                fontSize_ = oldSize;
                supW = sm.width;
                curX += sm.width;
                asc = std::max(asc, sm.ascent + bm.ascent * 0.45f);
            }
            if (b.sub) {
                float oldSize = fontSize_;
                fontSize_ = smallSize;
                BoxMetrics sm = layout(*b.sub, x + baseW, y + (b.base ? bm.ascent * 0.55f : 0), doDraw);
                fontSize_ = oldSize;
                subW = sm.width;
                curX = std::max(curX, x + baseW + sm.width);
                desc = std::max(desc, sm.height + (b.base ? bm.ascent * 0.55f : 0));
            }
            m.width = (b.sup && b.sub) ? std::max(supW, subW) + baseW : curX - x;
            m.height = asc + desc;
            m.ascent = asc; m.descent = desc;
            if (doDraw && b.base) layout(*b.base, x, y, true);
            break;
        }
        case Box::Radical: {
            // 用 KaTeX_Size1 字体的 √ 字符渲染根号 (大尺寸, 有完整钩形)
            BoxMetrics rm = layout(*b.radicand, x, y, false);
            ImFont* radFont = fallback(bigOpFont_ ? bigOpFont_ : math_, ImGui::GetFont());
            float radSize = fontSize_ * 1.2f;
            float radSymW = radFont->CalcTextSizeA(radSize, FLT_MAX, 0, "\xE2\x88\x9A").x; // √
            float w = radSymW + rm.width + 4.0f;
            m.width = w;
            m.height = rm.height + fontSize_ * 0.15f;
            m.ascent = rm.ascent + fontSize_ * 0.15f;
            m.descent = rm.descent;
            if (doDraw) {
                ImU32 col = IM_COL32(0, 0, 0, 255);
                // 用 KaTeX_Size1 字体画 √ 符号
                ImGui::PushFont(radFont);
                dl->AddText(radFont, radSize, ImVec2(x, y), col, "\xE2\x88\x9A"); // √
                ImGui::PopFont();
                // radicand 在 √ 右侧
                layout(*b.radicand, x + radSymW, y + fontSize_ * 0.15f, true);
                // overline 覆盖 radicand (根号上划线)
                float lineThick = std::max(1.5f, fontSize_ * 0.06f);
                float barY = y + fontSize_ * 0.05f;
                dl->AddLine(ImVec2(x + radSymW * 0.7f, barY),
                            ImVec2(x + radSymW + rm.width + 2, barY),
                            col, lineThick);
            }
            break;
        }
        case Box::BigOp: {
            // 求和/连乘: 符号居中, 上限在正上方, 下限在正下方, body 在右侧
            // 用 KaTeX Size1 字体渲染大运算符 (符号更美观)
            ImFont* f = fallback(bigOpFont_ ? bigOpFont_ : math_, ImGui::GetFont());
            float symSize = fontSize_ * 1.3f;
            float symW = textWidth(b.opSymbol, f) * 1.1f + 4;
            float symH = symSize * 0.9f;
            float loW = 0, hiW = 0, loH = 0, hiH = 0;
            float smallSize = fontSize_ * 0.6f;
            if (b.lower) {
                float old = fontSize_; fontSize_ = smallSize;
                BoxMetrics lm = layout(*b.lower, x, y, false); loW = lm.width; loH = lm.height;
                fontSize_ = old;
            }
            if (b.upper) {
                float old = fontSize_; fontSize_ = smallSize;
                BoxMetrics um = layout(*b.upper, x, y, false); hiW = um.width; hiH = um.height;
                fontSize_ = old;
            }
            float opBlockW = std::max(symW, std::max(loW, hiW));
            float opBlockH = hiH + symH + loH;
            float bodyW = 0, bodyH = fontSize_;
            if (b.body) { BoxMetrics bm = layout(*b.body, x, y, false); bodyW = bm.width; bodyH = bm.height; }
            m.width = opBlockW + 6 + bodyW;
            m.height = std::max(opBlockH, bodyH);
            // 整体垂直居中: ascent/descent 按 opBlock 和 body 的较大者分配
            m.ascent = m.height * 0.6f; m.descent = m.height * 0.4f;
            if (doDraw) {
                ImU32 col = IM_COL32(0, 0, 0, 255);
                // opBlock 垂直居中在 m.height 内
                float opY = y + (m.height - opBlockH) / 2;
                float cy = opY;
                // 上限 (正上方)
                if (b.upper) {
                    float old = fontSize_; fontSize_ = smallSize;
                    layout(*b.upper, x + (opBlockW - hiW)/2, cy, true);
                    fontSize_ = old;
                    cy += hiH;
                }
                // 符号 (居中)
                ImGui::PushFont(f);
                dl->AddText(f, symSize, ImVec2(x + (opBlockW - symW)/2, cy), col, b.opSymbol.c_str());
                ImGui::PopFont();
                cy += symH;
                // 下限 (正下方)
                if (b.lower) {
                    float old = fontSize_; fontSize_ = smallSize;
                    layout(*b.lower, x + (opBlockW - loW)/2, cy, true);
                    fontSize_ = old;
                }
                // body 在右侧, 垂直居中
                if (b.body) layout(*b.body, x + opBlockW + 6, y + (m.height - bodyH)/2, true);
            }
            break;
        }
        case Box::Delimited: {
            Box& inner = *b.children[0];
            BoxMetrics im = layout(inner, x, y, false);
            ImFont* f = fallback(math_, main_);
            float lW = b.leftDelim.empty() ? 0 : textWidth(b.leftDelim, f);
            float rW = b.rightDelim.empty() ? 0 : textWidth(b.rightDelim, f);
            float innerH = im.height;
            float w = lW + im.width + rW + 4;
            m.width = w;
            m.height = innerH;
            m.ascent = im.ascent; m.descent = im.descent;
            if (doDraw) {
                if (!b.leftDelim.empty()) {
                    ImGui::PushFont(f);
                    dl->AddText(f, fontSize_, ImVec2(x, y), IM_COL32(0,0,0,255), b.leftDelim.c_str());
                    ImGui::PopFont();
                }
                layout(inner, x + lW + 2, y, true);
                if (!b.rightDelim.empty()) {
                    ImGui::PushFont(f);
                    dl->AddText(f, fontSize_, ImVec2(x + lW + im.width + 2, y), IM_COL32(0,0,0,255), b.rightDelim.c_str());
                    ImGui::PopFont();
                }
            }
            break;
        }
        case Box::Function: {
            ImFont* f = fallback(math_, ImGui::GetFont());
            float nameW = textWidth(b.funcName, f);
            float subW = b.funcSub ? layout(*b.funcSub, x, y, false).width : 0;
            float argW = b.funcArg ? layout(*b.funcArg, x, y, false).width : 0;
            float cx = x;
            m.width = nameW + subW + argW + 4;
            m.height = fontHeight(f);
            m.ascent = m.height * 0.8f; m.descent = m.height * 0.2f;
            if (doDraw) {
                ImGui::PushFont(f);
                dl->AddText(f, fontSize_, ImVec2(cx, y), IM_COL32(0,0,0,255), b.funcName.c_str());
                ImGui::PopFont();
                cx += nameW;
                if (b.funcSub) {
                    float oldSize = fontSize_; fontSize_ = fontSize_ * 0.65f;
                    layout(*b.funcSub, cx, y + m.ascent * 0.5f, true);
                    fontSize_ = oldSize;
                    cx += subW;
                }
                if (b.funcArg) layout(*b.funcArg, cx, y, true);
            }
            break;
        }
        case Box::Padded: {
            if (!b.children.empty()) {
                BoxMetrics im = layout(*b.children[0], x, y, doDraw);
                m = im; m.width += 6;
            }
            break;
        }
        case Box::IversonSym: {
            // 自绘双线 I (𝕀) + (条件)
            // 先测量内部条件
            BoxMetrics condM;
            if (!b.children.empty()) condM = layout(*b.children[0], x, y, false);
            float iW = fontSize_ * 0.5f;
            float parenW = fontSize_ * 0.3f;
            float gap = fontSize_ * 0.15f;
            m.width = iW + gap + parenW + condM.width + parenW + gap;
            m.height = condM.height;
            m.ascent = condM.ascent; m.descent = condM.descent;
            if (doDraw) {
                ImU32 col = IM_COL32(0, 0, 0, 255);
                float cx = x;
                // 自绘双线 I: 两条竖线 + 上下短横 (衬线)
                float lineThick = std::max(1.5f, fontSize_ * 0.06f);
                float iH = condM.height;
                float iTop = y;
                float iBot = y + iH;
                float serifW = iW * 0.7f;
                // 左竖线
                dl->AddLine(ImVec2(cx + iW*0.25f, iTop), ImVec2(cx + iW*0.25f, iBot), col, lineThick);
                // 右竖线
                dl->AddLine(ImVec2(cx + iW*0.75f, iTop), ImVec2(cx + iW*0.75f, iBot), col, lineThick);
                // 上衬线
                dl->AddLine(ImVec2(cx + iW*0.15f, iTop), ImVec2(cx + iW*0.85f, iTop), col, lineThick);
                // 下衬线
                dl->AddLine(ImVec2(cx + iW*0.15f, iBot), ImVec2(cx + iW*0.85f, iBot), col, lineThick);
                cx += iW + gap;
                // 左括号
                ImFont* f = fallback(math_, ImGui::GetFont());
                ImGui::PushFont(f);
                dl->AddText(f, fontSize_, ImVec2(cx, y), col, "(");
                cx += parenW;
                ImGui::PopFont();
                // 条件
                if (!b.children.empty()) layout(*b.children[0], cx, y, true);
                cx += condM.width;
                // 右括号
                ImGui::PushFont(f);
                dl->AddText(f, fontSize_, ImVec2(cx, y), col, ")");
                ImGui::PopFont();
            }
            break;
        }
    }
    return m;
}

} // namespace scicalc
