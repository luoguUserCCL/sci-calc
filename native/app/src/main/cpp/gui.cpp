// gui.cpp — Dear ImGui + GLFW application for sci-calc.
// Provides: math input area, box-model formula render area, output, settings
// (output mode / base / precision / format), and a variable/function panel.
// Compiled in when SCICALC_WITH_GUI is defined.
#include "scicalc/Engine.hpp"
#include "scicalc/Lexer.hpp"
#include "scicalc/Parser.hpp"
#include "box_renderer.hpp"

// Ensure GL prototypes are declared (glcorearb.h) before ImGui's backend
// headers, and enable GL extension prototypes.
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>   // MessageBoxA
#endif

// 内嵌字体（Latin Modern Math + Noto Sans SC 子集）
#include "embedded/latinmodern_math.h"
#include "embedded/noto_sans_sc.h"

using namespace scicalc;

static int runGuiImpl(int argc, char** argv, Engine& engine);

static void glfwError(int code, const char* desc) {
#ifdef _WIN32
    char buf[512];
    std::snprintf(buf, sizeof(buf), "GLFW error %d: %s", code, desc ? desc : "(null)");
    MessageBoxA(nullptr, buf, "sci-calc GLFW error", MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "GLFW error %d: %s\n", code, desc ? desc : "(null)");
#endif
}

// Locate a Unicode TTF with math-symbol coverage for the renderer.
// 优先从内嵌字体加载（Latin Modern Math 用于数学符号，Noto Sans SC 用于中英文）。
static const char* findUnicodeFont() { return nullptr; }  // 不再从文件系统加载

int runGui(int argc, char** argv, Engine& engine) {
    // 用 try/catch 包裹整个 GUI 逻辑，崩溃时弹出 MessageBox（Windows GUI
    // 子系统没有 stderr，否则错误不可见）。
    try {
        return runGuiImpl(argc, argv, engine);
    } catch (const std::exception& e) {
        const char* msg = e.what();
#ifdef _WIN32
        MessageBoxA(nullptr, msg, "sci-calc GUI error", MB_OK | MB_ICONERROR);
#else
        std::fprintf(stderr, "sci-calc GUI error: %s\n", msg);
#endif
        return 1;
    } catch (...) {
#ifdef _WIN32
        MessageBoxA(nullptr, "unknown exception", "sci-calc GUI error", MB_OK | MB_ICONERROR);
#else
        std::fprintf(stderr, "sci-calc GUI: unknown exception\n");
#endif
        return 1;
    }
}

static int runGuiImpl(int argc, char** argv, Engine& engine) {
    // Optional: --screenshot "EXPR" OUT.ppm  -> render one frame and save a PPM.
    bool screenshot = false;
    std::string shotExpr, shotPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--screenshot" && i + 2 < argc) {
            screenshot = true; shotExpr = argv[i+1]; shotPath = argv[i+2]; break;
        }
    }

    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) return 1;

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    GLFWwindow* window = glfwCreateWindow(1100, 760, "sci-calc", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 字体: 内嵌 Latin Modern Math (数学符号) + Noto Sans SC (中英文 UI)。
    // 两个字体都从编译进二进制的字节数组加载，无需任何外部文件。
    ImFont* mainFont = io.Fonts->AddFontDefault();
    ImFont* mathFont = mainFont;
    ImFont* uiFont = mainFont;
    // 加载 Noto Sans SC (中英文 UI) 作为主字体
    static const ImWchar uiRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1
        0x0100, 0x017F, // Latin Extended-A
        0x3000, 0x303F, // CJK punctuation
        0x4E00, 0x5000, // CJK 常用
        0xFF00, 0xFFEF, // halfwidth/fullwidth
        0,
    };
    ImFontConfig uiCfg; uiCfg.MergeMode = false;
    uiFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)kNotoSansSC, kNotoSansSC_len, 22.0f, &uiCfg, uiRanges);
    if (uiFont) io.FontDefault = uiFont;
    mainFont = uiFont ? uiFont : mainFont;
    // 加载 Latin Modern Math (数学符号)，覆盖所有数学 Unicode 块
    static const ImWchar mathRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 (数字、运算符)
        0x0100, 0x017F, // Latin Extended-A
        0x0370, 0x03FF, // Greek
        0x2070, 0x209F, // superscripts/subscripts
        0x2200, 0x22FF, // math operators
        0x27C0, 0x27EF, // misc math
        0x2A00, 0x2AFF, // supp math operators
        0x3000, 0x303F, // CJK punct
        0x4E00, 0x5000, // CJK 常用
        0xFF00, 0xFFEF, // fullwidth
        0,
    };
    mathFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)kLatinModernMath, kLatinModernMath_len, 24.0f, nullptr, mathRanges);
    if (!mathFont) mathFont = mainFont;
    ImFont* smallFont = mathFont;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    BoxRenderer renderer;
    renderer.setFonts(mainFont, mathFont, smallFont);
    renderer.setScale(24.0f);

    // UI state
    char inputBuf[1024] = "";
    std::string lastOutput;
    std::string lastError;
    BoxPtr lastTree;          // result box tree
    BoxPtr inputTree;         // input echo box tree
    struct History { std::string in; std::string out; bool ok; };
    std::vector<History> history;
    int modeIdx = 0;          // 0=Math, 1=Decimal
    int baseIdx = 2;          // 2,8,10,16 -> index
    int fmtIdx = 0;           // General/Scientific/FixedPoint/FixedSig/Float
    int precision = 50;
    int fixedDigits = 6;

    const int bases[4] = {2, 8, 10, 16};
    const char* modeNames[2] = {"Math (symbolic)", "Decimal (high-precision)"};
    const char* fmtNames[5] = {"General", "Scientific", "Fixed point", "Fixed significant", "Float"};
    const char* baseNames[4] = {"2 (bin)", "8 (oct)", "10 (dec)", "16 (hex)"};

    // For screenshot mode, pre-evaluate the expression before the loop.
    if (screenshot) {
        std::strncpy(inputBuf, shotExpr.c_str(), sizeof(inputBuf) - 1);
        EngineResult r = engine.evaluate(shotExpr);
        lastOutput = r.ok ? r.output : "";
        lastError = r.ok ? "" : r.error;
        lastTree = std::move(r.renderTree);
        try { Lexer lex(shotExpr); Parser p(lex.tokenize()); ExprPtr ast = p.parse(); inputTree = buildInputBox(*ast); } catch(...) {}
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
        ImGui::Begin("sci-calc", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // --- Settings bar ---
        if (ImGui::Combo("Mode", &modeIdx, modeNames, 2)) {
            engine.config().outputMode = (modeIdx == 0) ? OutputMode::Math : OutputMode::Decimal;
            BigFloat::defaultPrecision() = engine.config().precision;
        }
        ImGui::SameLine();
        if (ImGui::Combo("Base", &baseIdx, baseNames, 4)) engine.config().numberBase = bases[baseIdx];
        ImGui::SameLine();
        if (ImGui::Combo("Format", &fmtIdx, fmtNames, 5)) {
            engine.config().numFormat = (EngineConfig::NumFormat)fmtIdx;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("Prec", &precision)) {
            if (precision < 1) precision = 1;
            engine.config().precision = (unsigned)precision;
            BigFloat::defaultPrecision() = (unsigned)precision;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("Fixed", &fixedDigits)) {
            if (fixedDigits < 0) fixedDigits = 0;
            engine.config().fixedDigits = fixedDigits;
        }
        ImGui::Separator();

        // --- Input area ---
        ImGui::Text("Math input:");
        ImGui::PushItemWidth(-120);
        bool submitted = ImGui::InputText("##input", inputBuf, sizeof(inputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            nullptr, nullptr);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Evaluate (Enter)") || submitted) {
            std::string expr(inputBuf);
            if (!expr.empty()) {
                EngineResult r = engine.evaluate(expr);
                lastOutput = r.ok ? r.output : "";
                lastError = r.ok ? "" : r.error;
                lastTree = std::move(r.renderTree);
                // build input echo tree
                try {
                    Lexer lex(expr); Parser p(lex.tokenize());
                    ExprPtr ast = p.parse();
                    inputTree = buildInputBox(*ast);
                } catch (...) { inputTree.reset(); }
                history.push_back({expr, r.ok ? r.output : ("error: " + r.error), r.ok});
                inputBuf[0] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) { inputBuf[0]='\0'; lastOutput.clear(); lastError.clear(); lastTree.reset(); inputTree.reset(); }

        // --- Formula render area (input echo + result) ---
        ImGui::Separator();
        ImGui::Text("Formula rendering:");
        ImVec2 area = ImGui::GetContentRegionAvail();
        float renderH = area.y * 0.45f;
        ImVec2 rp = ImGui::GetCursorScreenPos();
        ImVec2 rs = ImVec2(area.x, renderH);
        ImGui::InvisibleButton("##render", rs);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(rp, ImVec2(rp.x + rs.x, rp.y + rs.y), IM_COL32(250, 250, 252, 255));
        dl->AddRect(rp, ImVec2(rp.x + rs.x, rp.y + rs.y), IM_COL32(180, 180, 190, 255));
        // draw input echo + result stacked
        float curY = rp.y + 12;
        if (inputTree) {
            ImGui::PushFont(mathFont);
            renderer.setScale(22.0f);
            float used = renderer.draw(*inputTree, rp.x + 16, curY);
            (void)used;
            ImGui::PopFont();
            auto im = renderer.measure(*inputTree);
            curY += im.height + 18;
        }
        if (lastTree) {
            ImGui::PushFont(mathFont);
            renderer.setScale(26.0f);
            renderer.draw(*lastTree, rp.x + 16, curY);
            ImGui::PopFont();
        } else if (!lastOutput.empty()) {
            ImGui::PushFont(mathFont);
            dl->AddText(mathFont, 24, ImVec2(rp.x + 16, curY), IM_COL32(20, 20, 30, 255), lastOutput.c_str());
            ImGui::PopFont();
        }

        // --- Output / error ---
        ImGui::Separator();
        if (!lastError.empty()) {
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "error: %s", lastError.c_str());
        } else if (!lastOutput.empty()) {
            ImGui::TextDisabled("= %s", lastOutput.c_str());
        }

        // --- History + variable/function panel (remaining space) ---
        ImGui::Separator();
        float remainH = ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginTabBar("##tabs")) {
            if (ImGui::BeginTabItem("History")) {
                ImGui::BeginChild("##hist", ImVec2(0, remainH - 30), true);
                for (int i = (int)history.size() - 1; i >= 0; --i) {
                    ImGui::Text("%s", history[i].in.c_str());
                    ImGui::SameLine();
                    if (history[i].ok) ImGui::TextDisabled("= %s", history[i].out.c_str());
                    else ImGui::TextColored(ImVec4(0.9f,0.3f,0.3f,1), "%s", history[i].out.c_str());
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Variables")) {
                ImGui::BeginChild("##vars", ImVec2(0, remainH - 60), true);
                for (auto& [k, v] : engine.vars()) {
                    ImGui::Text("%s = %s", k.c_str(), engine.format(v).c_str());
                    ImGui::SameLine();
                    ImGui::PushID(k.c_str());
                    if (ImGui::SmallButton("Del")) { engine.delVar(k); }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Functions")) {
                ImGui::BeginChild("##funcs", ImVec2(0, remainH - 60), true);
                for (auto& [k, f] : engine.funcs()) {
                    ImGui::Text("%s(", k.c_str());
                    for (size_t i = 0; i < f.params.size(); ++i) { if (i) ImGui::SameLine(0,0); ImGui::TextUnformatted(f.params[i].c_str()); if (i+1<f.params.size()) ImGui::SameLine(0,0); }
                    ImGui::SameLine(0,0); ImGui::Text(")");
                    ImGui::SameLine();
                    ImGui::PushID(k.c_str());
                    if (ImGui::SmallButton("Del")) engine.delFunc(k);
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        int dw, dh; glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        // Screenshot: capture the framebuffer to a PPM after the first rendered frame.
        if (screenshot) {
            std::vector<unsigned char> px((size_t)dw * dh * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, dw, dh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            std::ofstream f(shotPath, std::ios::binary);
            f << "P6\n" << dw << " " << dh << "\n255\n";
            // PPM is top-down; GL is bottom-up, so flip rows.
            for (int y = dh - 1; y >= 0; --y)
                f.write((const char*)&px[(size_t)y * dw * 3], (std::streamsize)dw * 3);
            f.close();
            fprintf(stderr, "screenshot saved -> %s (%dx%d)\n", shotPath.c_str(), dw, dh);
            break;
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
