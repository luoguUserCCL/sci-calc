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
    if (b.style == Box::Symbol || b.style == Box::Operator) return fallback(math_, main_);
    return fallback(main_, ImGui::GetFont());
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
                dl->AddText(f, fontSize_, ImVec2(x, y), IM_COL32(20, 20, 30, 255), s.c_str());
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
                cx += cm.width + 2.0f; // small inter-box gap
                maxAsc = std::max(maxAsc, cm.ascent);
                maxDesc = std::max(maxDesc, cm.descent);
            }
            if (!b.children.empty()) cx -= 2.0f; // remove trailing gap
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
                dl->AddLine(ImVec2(x, barY), ImVec2(x + w, barY), IM_COL32(20, 20, 30, 255), 1.5f);
            }
            break;
        }
        case Box::SupSub: {
            BoxMetrics bm = layout(*b.base, x, y, false);
            float smallSize = fontSize_ * 0.65f;
            float baseW = bm.width;
            float curX = x + baseW;
            float asc = bm.ascent, desc = bm.descent;
            float subW = 0, supW = 0;
            if (b.sup) {
                float oldSize = fontSize_;
                fontSize_ = smallSize;
                BoxMetrics sm = layout(*b.sup, curX, y - bm.ascent * 0.45f, doDraw);
                fontSize_ = oldSize;
                supW = sm.width;
                curX += sm.width;
                asc = std::max(asc, sm.ascent + bm.ascent * 0.45f);
            }
            if (b.sub) {
                float oldSize = fontSize_;
                fontSize_ = smallSize;
                BoxMetrics sm = layout(*b.sub, x + baseW, y + bm.ascent * 0.55f, doDraw);
                fontSize_ = oldSize;
                subW = sm.width;
                curX = std::max(curX, x + baseW + sm.width);
                desc = std::max(desc, sm.height + bm.ascent * 0.55f);
            }
            m.width = (b.sup && b.sub) ? std::max(supW, subW) + baseW : curX - x;
            m.height = asc + desc;
            m.ascent = asc; m.descent = desc;
            if (doDraw) layout(*b.base, x, y, true);
            break;
        }
        case Box::Radical: {
            BoxMetrics rm = layout(*b.radicand, x, y, false);
            float radSymW = fontSize_ * 0.6f;
            float w = radSymW + rm.width + 4.0f;
            m.width = w;
            m.height = rm.height + 2.0f;
            m.ascent = rm.ascent + 2.0f;
            m.descent = rm.descent;
            if (doDraw) {
                ImFont* f = fallback(math_, ImGui::GetFont());
                ImGui::PushFont(f);
                dl->AddText(f, fontSize_, ImVec2(x, y), IM_COL32(20,20,30,255), "\xE2\x88\x9A"); // √
                ImGui::PopFont();
                layout(*b.radicand, x + radSymW + 2, y, true);
                // overline
                dl->AddLine(ImVec2(x + radSymW, y - 1), ImVec2(x + radSymW + rm.width + 2, y - 1),
                            IM_COL32(20,20,30,255), 1.0f);
            }
            break;
        }
        case Box::BigOp: {
            // symbol centered, lower/upper limits stacked, body to the right
            ImFont* f = fallback(math_, ImGui::GetFont());
            float symH = fontSize_ * 1.4f;
            float symW = textWidth(b.opSymbol, f) * 1.4f + 4;
            float limitsW = 0, limitsH = 0;
            float loW = 0, hiW = 0;
            if (b.lower) { BoxMetrics lm = layout(*b.lower, x, y, false); loW = lm.width; }
            if (b.upper) { BoxMetrics um = layout(*b.upper, x, y, false); hiW = um.width; }
            limitsW = std::max(loW, hiW);
            limitsH = (b.lower ? 1 : 0) + (b.upper ? 1 : 0) ? fontSize_ * 1.1f : 0;
            float bodyW = 0, bodyH = fontSize_;
            if (b.body) { BoxMetrics bm = layout(*b.body, x, y, false); bodyW = bm.width; bodyH = bm.height; }
            float opBlockW = std::max(symW, limitsW);
            m.width = opBlockW + 6 + bodyW;
            m.height = std::max(symH + limitsH, bodyH);
            m.ascent = m.height * 0.7f; m.descent = m.height * 0.3f;
            if (doDraw) {
                ImGui::PushFont(f);
                float symY = y + (m.height - symH) / 2;
                dl->AddText(f, fontSize_ * 1.2f, ImVec2(x + (opBlockW - symW)/2, symY),
                            IM_COL32(20,20,30,255), b.opSymbol.c_str());
                ImGui::PopFont();
                float smallSize = fontSize_ * 0.7f;
                float cy = y;
                float oldSize = fontSize_;
                if (b.upper) {
                    fontSize_ = smallSize;
                    BoxMetrics um = layout(*b.upper, x + (opBlockW - hiW)/2, cy, true);
                    cy += um.height;
                    fontSize_ = oldSize;
                }
                // symbol occupies middle
                cy = y + symH;
                if (b.lower) {
                    fontSize_ = smallSize;
                    layout(*b.lower, x + (opBlockW - loW)/2, cy, true);
                    fontSize_ = oldSize;
                }
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
                    dl->AddText(f, fontSize_, ImVec2(x, y), IM_COL32(20,20,30,255), b.leftDelim.c_str());
                    ImGui::PopFont();
                }
                layout(inner, x + lW + 2, y, true);
                if (!b.rightDelim.empty()) {
                    ImGui::PushFont(f);
                    dl->AddText(f, fontSize_, ImVec2(x + lW + im.width + 2, y), IM_COL32(20,20,30,255), b.rightDelim.c_str());
                    ImGui::PopFont();
                }
            }
            break;
        }
        case Box::Function: {
            ImFont* f = fallback(main_, ImGui::GetFont());
            float nameW = textWidth(b.funcName, f);
            float subW = b.funcSub ? layout(*b.funcSub, x, y, false).width : 0;
            float argW = b.funcArg ? layout(*b.funcArg, x, y, false).width : 0;
            float cx = x;
            m.width = nameW + subW + argW + 4;
            m.height = fontHeight(f);
            m.ascent = m.height * 0.8f; m.descent = m.height * 0.2f;
            if (doDraw) {
                ImGui::PushFont(f);
                dl->AddText(f, fontSize_, ImVec2(cx, y), IM_COL32(20,20,30,255), b.funcName.c_str());
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
    }
    return m;
}

} // namespace scicalc
