#include "Runtime.h"
#include "Runtime_private.h"

#include <ofxImGuiStyle/src/ofxImGuiStyle.h>

#include "imgui_internal.h"

#include <ofMain.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace ofkitty {

void Runtime::registerPreferencePage(PreferencePage page)
{
    if (page.id.empty()) {
        ofLogWarning("ofxKit") << "Cannot register a preference page with no id.";
        return;
    }
    for (const auto& existing : m_preferencePages) {
        if (existing.id == page.id) {
            ofLogWarning("ofxKit") << "Preference page '" << page.id << "' is already registered.";
            return;
        }
    }
    m_preferencePages.push_back(std::move(page));
}

bool Runtime::unregisterPreferencePage(const std::string& id)
{
    auto it = std::find_if(m_preferencePages.begin(), m_preferencePages.end(),
                           [&](const PreferencePage& p) { return p.id == id; });
    if (it == m_preferencePages.end())
        return false;
    if (m_selectedPreferencePage == id)
        m_selectedPreferencePage.clear();
    m_preferencePages.erase(it);
    return true;
}

void Runtime::registerBuiltInPreferencePages()
{
    if (m_builtInPreferencePagesRegistered)
        return;
    m_builtInPreferencePagesRegistered = true;

    registerPreferencePage({"ofKitty",
                            "Appearance",
                            "ofxkit.prefs.appearance",
                            [this] { drawPrefsAppearance(); }});
    registerPreferencePage({"openFrameworks",
                            "General",
                            "ofxkit.prefs.general",
                            [this] { drawPrefsGeneral(); }});
    registerPreferencePage({"openFrameworks",
                            "Rendering",
                            "ofxkit.prefs.rendering",
                            [this] { drawPrefsRendering(); }});
    registerPreferencePage({"openFrameworks",
                            "Logging",
                            "ofxkit.prefs.logging",
                            [this] { drawPrefsLogging(); }});
    registerPreferencePage({"openFrameworks",
                            "Status Bar",
                            "ofxkit.prefs.statusbar",
                            [this] { drawPrefsStatusBar(); }});
}

void Runtime::drawPreferencePageList()
{
    std::vector<std::string> cats;
    for (auto& p : m_preferencePages) {
        if (std::find(cats.begin(), cats.end(), p.category) == cats.end())
            cats.push_back(p.category);
    }

    for (auto& cat : cats) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", cat.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        for (auto& p : m_preferencePages) {
            if (p.category != cat)
                continue;
            ImGui::PushID(p.id.c_str());
            bool selected = (p.id == m_selectedPreferencePage);
            if (ImGui::Selectable(("  " + p.name).c_str(), selected))
                m_selectedPreferencePage = p.id;
            ImGui::PopID();
        }
    }
}

void Runtime::drawPreferencePageContent()
{
    for (auto& p : m_preferencePages) {
        if (p.id != m_selectedPreferencePage)
            continue;
        ImGui::TextUnformatted(p.name.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        if (p.draw) {
            ImGui::PushID(p.id.c_str());
            p.draw();
            ImGui::PopID();
        }
        break;
    }
}

void Runtime::drawPrefsAppearance()
{
    ImGui::SeparatorText("Theme");
    if (ImGui::Button("Dark"))
        setTheme(Theme::Dark);
    ImGui::SameLine();
    if (ImGui::Button("Light"))
        setTheme(Theme::Light);
    ImGui::SameLine();
    if (ImGui::Button("Classic"))
        setTheme(Theme::Classic);
    ImGui::SameLine();
    if (ImGui::Button("Random \xef\x8b\x8b")) {
        ofxImGuiStyle::applyRandomAccentTheme();
        m_style.captureBaseStyle();
        applyUIScale();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Randomise the accent colour");

    ImGui::SeparatorText("UI Scale");
    float scale = m_uiScale;
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::SliderFloat("##scale", &scale, 0.5f, 3.0f, "%.2fx"))
        setUIScale(scale);
    ImGui::SameLine();
    if (ImGui::Button("Auto"))
        setUIScale(detectUIScale());

    ImGui::SeparatorText("Theme File");
    ImGui::TextDisabled("Themes are saved as .bin files (binary ImGuiStyle snapshot).");
    if (ImGui::Button("Save Theme As...")) {
        saveFileDialog(
            "save_theme",
            "Save Theme",
            ".bin",
            "my_theme.bin",
            [this](const std::string& path) {
                std::string p = path;
                if (p.size() < 4 || p.substr(p.size() - 4) != ".bin")
                    p += ".bin";
                ofxImGuiStyle::saveTheme(p);
            });
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Theme...")) {
        openFileDialog("load_theme", "Load Theme", ".bin",
                       [this](const std::string& path) {
                           if (ofxImGuiStyle::loadTheme(path)) {
                               ofxImGuiStyle::applyCompactMetrics();
                               m_style.captureBaseStyle();
                               applyUIScale();
                           }
                       });
    }

    ImGui::SeparatorText("Edit Mode Toggle");
    ImGui::TextDisabled("Controls what Tab / Ctrl+E hides when leaving edit mode.");
    bool hideAll = m_prefs.hideAllUI;
    if (ImGui::RadioButton("Hide windows only", !hideAll)) {
        m_prefs.hideAllUI = false;
        saveAppPrefs();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Hide entire UI", hideAll)) {
        m_prefs.hideAllUI = true;
        saveAppPrefs();
    }

    ImGui::SeparatorText("Style Editor");
    ImGui::ShowStyleEditor();
}

void Runtime::drawPrefsGeneral()
{
    if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::SliderInt("Target FPS", &m_prefs.targetFps, 1, 240)) {
            ofSetFrameRate(m_prefs.targetFps);
            saveAppPrefs();
        }

        ImGui::BeginDisabled(true);
        float actualFps = static_cast<float>(ofGetFrameRate());
        ImGui::InputFloat("Actual FPS", &actualFps, 0.f, 0.f, "%.1f");
        ImGui::EndDisabled();

        if (ImGui::Checkbox("Vertical Sync", &m_prefs.vsync)) {
            ofSetVerticalSync(m_prefs.vsync);
            saveAppPrefs();
        }
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Background", ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::Checkbox("Auto Clear", &m_prefs.backgroundAuto)) {
            ofSetBackgroundAuto(m_prefs.backgroundAuto);
            saveAppPrefs();
        }

        float col[4] = {
            m_prefs.bgColor.r / 255.f,
            m_prefs.bgColor.g / 255.f,
            m_prefs.bgColor.b / 255.f,
            m_prefs.bgColor.a / 255.f,
        };
        if (ImGui::ColorEdit4("Background Colour", col)) {
            m_prefs.bgColor.set(
                static_cast<unsigned char>(col[0] * 255.f),
                static_cast<unsigned char>(col[1] * 255.f),
                static_cast<unsigned char>(col[2] * 255.f),
                static_cast<unsigned char>(col[3] * 255.f));
            ofBackground(m_prefs.bgColor);
            saveAppPrefs();
        }
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Window")) {

        ImGui::BeginDisabled(true);
        int sz[2]  = {ofGetWidth(), ofGetHeight()};
        int pos[2] = {ofGetWindowPositionX(), ofGetWindowPositionY()};
        ImGui::InputInt2("Size (px)", sz);
        ImGui::InputInt2("Position (px)", pos);
        ImGui::EndDisabled();

        static char titleBuf[128] = {};
        ImGui::InputText("Set Title", titleBuf, sizeof(titleBuf));
        ImGui::SameLine();
        if (ImGui::Button("Apply"))
            ofSetWindowTitle(titleBuf);
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Rulers")) {
        if (ImGui::SliderFloat("Ruler Size", &m_prefs.rulerScale, 0.5f, 3.0f, "%.2fx"))
            saveAppPrefs();
        ImGui::SameLine();
        if (ImGui::Button("Reset##rulerScale")) {
            m_prefs.rulerScale = 1.0f;
            saveAppPrefs();
        }
        ImGui::TextDisabled(
            "Scales ruler strip width and tick labels (base x UI Scale).");

        ImGui::Spacing();
        static const char* kUnits[] = {"Pixels", "Millimetres"};
        int unitIdx = (int)m_prefs.rulerUnit;
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::Combo("Panel ruler unit", &unitIdx, kUnits, 2)) {
            m_prefs.rulerUnit = (AppPrefs::RulerUnit)unitIdx;
            saveAppPrefs();
        }
        ImGui::SetItemTooltip(
            "Unit shown on per-panel rulers (e.g. Preview, Viewport).\n"
            "Full-window rulers always show pixels.");
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Margin overlay")) {
        if (ImGui::Checkbox("Show margin rectangle", &m_prefs.showMarginRect))
            saveAppPrefs();
        ImGui::SetItemTooltip(
            "Draw the printable / safe area inside the canvas in panels\n"
            "that opt in via ofkitty::drawMarginRect().");

        float col[4] = {
            m_prefs.marginColor.r / 255.f,
            m_prefs.marginColor.g / 255.f,
            m_prefs.marginColor.b / 255.f,
            m_prefs.marginColor.a / 255.f,
        };
        if (ImGui::ColorEdit4("Margin Colour", col)) {
            m_prefs.marginColor.set(
                static_cast<unsigned char>(col[0] * 255.f),
                static_cast<unsigned char>(col[1] * 255.f),
                static_cast<unsigned char>(col[2] * 255.f),
                static_cast<unsigned char>(col[3] * 255.f));
            saveAppPrefs();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##marginColor")) {
            m_prefs.marginColor.set(180, 90, 220, 200);
            saveAppPrefs();
        }
        ImGui::TextDisabled(
            "Distinct colour (default purple) so margins are visually\n"
            "separable from cyan draggable guides.");
        ImGui::Spacing();
    }
}

void Runtime::drawPrefsRendering()
{
    if (ImGui::SliderInt("Circle Resolution", &m_prefs.circleRes, 3, 128)) {
        ofSetCircleResolution(m_prefs.circleRes);
        saveAppPrefs();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Number of line segments used to draw circles and arcs.");

    if (ImGui::SliderFloat("Line Width", &m_prefs.lineWidth, 0.5f, 20.f, "%.1f px")) {
        ofSetLineWidth(m_prefs.lineWidth);
        saveAppPrefs();
    }

    if (ImGui::Checkbox("Smooth Lighting", &m_prefs.smoothLighting)) {
        ofSetSmoothLighting(m_prefs.smoothLighting);
        saveAppPrefs();
    }

    if (ImGui::Checkbox("Depth Test", &m_prefs.depthTest)) {
        if (m_prefs.depthTest)
            ofEnableDepthTest();
        else
            ofDisableDepthTest();
        saveAppPrefs();
    }
}

void Runtime::drawPrefsLogging()
{
    const char* levels[] = {"Verbose", "Notice", "Warning", "Error", "Fatal Error", "Silent"};
    if (ImGui::Combo("Log Level", &m_prefs.logLevel, levels, 6)) {
        ofSetLogLevel(static_cast<ofLogLevel>(m_prefs.logLevel));
        saveAppPrefs();
    }
}

void Runtime::drawPrefsStatusBar()
{
    ImGui::TextWrapped("Toggle which status bar items are visible. Changes take effect immediately.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    for (auto& item : m_statusItems) {
        std::string label = item.id;
        auto        dot = label.rfind('.');
        if (dot != std::string::npos)
            label = label.substr(dot + 1);
        for (char& c : label)
            if (c == '_')
                c = ' ';
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));

        ImGui::PushID(item.id.c_str());
        ImGui::Checkbox(label.c_str(), &item.visible);
        if (!item.group.empty()) {
            ImGui::SameLine(0, 8.f);
            ImGui::TextDisabled("(%s)", item.group.c_str());
        }
        ImGui::PopID();
    }
}

void Runtime::registerStatusItem(StatusItem item)
{
    if (item.id.empty()) {
        ofLogWarning("ofxKit") << "Cannot register a status item with no id.";
        return;
    }
    for (const auto& existing : m_statusItems) {
        if (existing.id == item.id) {
            ofLogWarning("ofxKit") << "Status item '" << item.id << "' is already registered.";
            return;
        }
    }
    m_statusItems.push_back(std::move(item));
}

bool Runtime::unregisterStatusItem(const std::string& id)
{
    auto it =
        std::find_if(m_statusItems.begin(), m_statusItems.end(),
                     [&](const StatusItem& s) { return s.id == id; });
    if (it == m_statusItems.end())
        return false;
    m_statusItems.erase(it);
    return true;
}

void Runtime::registerBuiltInStatusItems()
{
    if (m_builtInStatusItemsRegistered)
        return;
    m_builtInStatusItemsRegistered = true;

    registerStatusItem(
        {"ofxkit.status.editmode",
         "ofxkit",
         false,
         [this] {
             if (m_editMode)
                 ImGui::TextColored({0.39f, 0.90f, 0.50f, 1.f}, "Edit Mode");
             else
                 ImGui::TextDisabled("Edit off  (Ctrl+E)");
         }});
    registerStatusItem({"ofxkit.status.appname",
                        "ofxkit",
                        false,
                        [this] { ImGui::TextDisabled("%s", m_appName.c_str()); }});
    registerStatusItem({"ofxkit.status.entities",
                        "ofxkit.stats",
                        true,
                        [this] {
                            auto& reg = registry();
                            ImGui::TextDisabled("entities: %d",
                                                static_cast<int>(reg.storage<
                                                                     entt::entity>()
                                                                     .size()));
                        }});
    registerStatusItem({"ofxkit.status.fps",
                        "ofxkit.stats",
                        true,
                        [] { ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate); }});
    registerStatusItem({"ofxkit.status.hint",
                        "ofxkit.hint",
                        true,
                        [] { ImGui::TextDisabled("CTRL+E  toggle edit mode"); }});
}

void Runtime::drawStatusBar()
{
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_MenuBar;
    float height = ImGui::GetFrameHeight();

    if (ImGui::BeginViewportSideBar("##StatusBar", nullptr, ImGuiDir_Down, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            bool        first     = true;
            std::string prevGroup = "\x01";

            for (auto& item : m_statusItems) {
                if (!item.visible || !item.draw)
                    continue;

                bool newGroup =
                    !item.group.empty() && item.group != prevGroup && prevGroup != "\x01";

                if (!first) {
                    if (newGroup) {
                        ImGui::SameLine(0, 6.f);
                        ImGui::TextDisabled("|");
                    }
                    ImGui::SameLine(0, 8.f);
                }

                prevGroup = item.group;
                first     = false;
                ImGui::PushID(item.id.c_str());
                item.draw();
                ImGui::PopID();
            }

            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

void Runtime::drawPreferencesWindow(bool& visible)
{
    ImGui::SetNextWindowPos(ImVec2(360, 50), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(580, 520), ImGuiCond_Once);

    if (!ImGui::Begin("Preferences###ofxkit.window.preferences", &visible)) {
        ImGui::End();
        return;
    }

    if (!m_prefs.initialized) {
        m_prefs.initialized    = true;
        m_prefs.backgroundAuto = ofGetBackgroundAuto();
        m_prefs.bgColor        = ofGetBackgroundColor();
        m_prefs.logLevel       = static_cast<int>(ofGetLogLevel());
    }

    if (m_selectedPreferencePage.empty() && !m_preferencePages.empty())
        m_selectedPreferencePage = m_preferencePages.front().id;

    const float leftW = std::max(140.f, 140.f * m_uiScale);

    ImGui::BeginChild("##PrefList", ImVec2(leftW, 0), true);
    drawPreferencePageList();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild(
        "##PrefContent",
        ImVec2(0, 0),
        false,
        ImGuiWindowFlags_HorizontalScrollbar);
    drawPreferencePageContent();
    ImGui::EndChild();

    ImGui::End();
}

void Runtime::drawRulers()
{
    const float  scale = m_uiScale * m_prefs.rulerScale;
    const float  RS    = std::round(20.f * scale);
    const float  fs    = std::round(9.f * scale);
    constexpr ImU32 kBg    = IM_COL32(25, 25, 35, 235);
    constexpr ImU32 kBord  = IM_COL32(70, 70, 85, 255);
    constexpr ImU32 kTick  = IM_COL32(160, 160, 175, 210);
    constexpr ImU32 kLabel = IM_COL32(130, 130, 145, 255);
    constexpr ImU32 kCurs  = IM_COL32(240, 80, 80, 220);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2         org(vp->WorkPos);
    const ImVec2         size(vp->WorkSize);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont*     fn = ImGui::GetFont();

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float  mx = mouse.x - org.x - RS;
    const float  my = mouse.y - org.y - RS;

    {
        const ImVec2 rMin(org.x + RS, org.y);
        const ImVec2 rMax(org.x + size.x, org.y + RS);
        dl->AddRectFilled(rMin, rMax, kBg);
        dl->AddLine({rMin.x, rMax.y}, {rMax.x, rMax.y}, kBord);

        for (float px = 0.f; px < size.x - RS; px += 10.f) {
            const float x       = rMin.x + px;
            const bool  major   = std::fmod(px, 100.f) < 0.5f;
            const bool  mid     = std::fmod(px, 50.f) < 0.5f;
            const float tickLen = major ? RS * 0.65f : mid ? RS * 0.45f : RS * 0.22f;
            dl->AddLine({x, rMax.y - tickLen}, {x, rMax.y}, kTick);
            if (major && px > 0.f) {
                char buf[12];
                snprintf(buf, sizeof(buf), "%.0f", px);
                dl->AddText(fn, fs, {x + 2.f, org.y + 2.f}, kLabel, buf);
            }
        }

        if (mx >= 0.f && mx < size.x - RS) {
            const float cx = rMin.x + mx;
            dl->AddLine({cx, org.y}, {cx, org.y + RS}, kCurs, 1.5f);

            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", mx);
            const float tw = ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize());
            dl->AddText(fn, fs, {cx - tw * 0.5f, org.y + 2.f}, kCurs, buf);
        }
    }

    {
        const ImVec2 rMin(org.x, org.y + RS);
        const ImVec2 rMax(org.x + RS, org.y + size.y);
        dl->AddRectFilled(rMin, rMax, kBg);
        dl->AddLine({rMax.x, rMin.y}, {rMax.x, rMax.y}, kBord);

        for (float py = 0.f; py < size.y - RS; py += 10.f) {
            const float y       = rMin.y + py;
            const bool  major   = std::fmod(py, 100.f) < 0.5f;
            const bool  mid     = std::fmod(py, 50.f) < 0.5f;
            const float tickLen = major ? RS * 0.65f : mid ? RS * 0.45f : RS * 0.22f;
            dl->AddLine({rMax.x - tickLen, y}, {rMax.x, y}, kTick);
            if (major && py > 0.f) {
                char buf[12];
                snprintf(buf, sizeof(buf), "%.0f", py);
                dl->AddText(fn, fs, {org.x + 1.f, y + 2.f}, kLabel, buf);
            }
        }

        if (my >= 0.f && my < size.y - RS) {
            const float cy = rMin.y + my;
            dl->AddLine({org.x, cy}, {org.x + RS, cy}, kCurs, 1.5f);

            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", my);
            dl->AddText(fn, fs, {org.x + 1.f, cy + 2.f}, kCurs, buf);
        }
    }

    {
        dl->AddRectFilled(org, {org.x + RS, org.y + RS}, kBg);
        dl->AddLine({org.x + RS, org.y}, {org.x + RS, org.y + RS}, kBord);
        dl->AddLine({org.x, org.y + RS}, {org.x + RS, org.y + RS}, kBord);

        if (mx >= 0.f && my >= 0.f) {
            char buf[20];
            snprintf(buf,
                     sizeof(buf),
                     "%d,%d",
                     static_cast<int>(mx),
                     static_cast<int>(my));
            const float tw =
                ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize());
            dl->AddText(fn, fs,
                        {mouse.x - tw * 0.5f, mouse.y - RS - 2.f}, kCurs, buf);
        }
    }
}

void Runtime::loadAppPrefs()
{
    std::string path = ofToDataPath("ofxKit/appPrefs.json", true);
    try {
        std::ifstream in(path);
        if (!in.is_open())
            return;
        ofJson j;
        in >> j;

        if (j.contains("circleRes"))
            m_prefs.circleRes = j["circleRes"];
        if (j.contains("targetFps"))
            m_prefs.targetFps = j["targetFps"];
        if (j.contains("vsync"))
            m_prefs.vsync = j["vsync"];
        if (j.contains("backgroundAuto"))
            m_prefs.backgroundAuto = j["backgroundAuto"];
        if (j.contains("lineWidth"))
            m_prefs.lineWidth = j["lineWidth"];
        if (j.contains("smoothLighting"))
            m_prefs.smoothLighting = j["smoothLighting"];
        if (j.contains("depthTest"))
            m_prefs.depthTest = j["depthTest"];
        if (j.contains("logLevel"))
            m_prefs.logLevel = j["logLevel"];
        if (j.contains("rulerScale"))
            m_prefs.rulerScale =
                std::clamp(j["rulerScale"].get<float>(), 0.5f, 3.0f);
        if (j.contains("rulerUnit"))
            m_prefs.rulerUnit = static_cast<AppPrefs::RulerUnit>(
                std::clamp(j["rulerUnit"].get<int>(), 0, 1));
        if (j.contains("hideAllUI"))
            m_prefs.hideAllUI = j["hideAllUI"].get<bool>();
        if (j.contains("bgR")) {
            m_prefs.bgColor.set(static_cast<unsigned char>((int)j["bgR"]),
                                static_cast<unsigned char>((int)j["bgG"]),
                                static_cast<unsigned char>((int)j["bgB"]),
                                static_cast<unsigned char>((int)j["bgA"]));
        }
        if (j.contains("showMarginRect"))
            m_prefs.showMarginRect = j["showMarginRect"].get<bool>();
        if (j.contains("marginR")) {
            m_prefs.marginColor.set(static_cast<unsigned char>((int)j["marginR"]),
                                    static_cast<unsigned char>((int)j["marginG"]),
                                    static_cast<unsigned char>((int)j["marginB"]),
                                    static_cast<unsigned char>((int)j["marginA"]));
        }

        if (j.contains("windowVisibility") && j["windowVisibility"].is_object()) {
            m_savedWindowVisibility =
                j["windowVisibility"].get<std::unordered_map<std::string, bool>>();
            for (auto& w : m_windows) {
                if (!w.id.empty() && m_savedWindowVisibility.count(w.id))
                    w.visible = m_savedWindowVisibility.at(w.id);
                else if (m_savedWindowVisibility.count(w.name))
                    w.visible = m_savedWindowVisibility.at(w.name);
            }
        }

        ofSetCircleResolution(m_prefs.circleRes);
        ofSetFrameRate(m_prefs.targetFps);
        ofSetVerticalSync(m_prefs.vsync);
        ofSetBackgroundAuto(m_prefs.backgroundAuto);
        ofSetLineWidth(m_prefs.lineWidth);
        ofSetSmoothLighting(m_prefs.smoothLighting);
        if (m_prefs.depthTest)
            ofEnableDepthTest();
        else
            ofDisableDepthTest();
        ofSetLogLevel(static_cast<ofLogLevel>(m_prefs.logLevel));
        ofBackground(m_prefs.bgColor);
    } catch (...) {}
}

void Runtime::saveAppPrefs()
{
    if (const char* iniPath = ImGui::GetIO().IniFilename) {
        detail::createParentDirectoryIfNeeded(iniPath);
        ImGui::SaveIniSettingsToDisk(iniPath);
    }

    std::string path = ofToDataPath("ofxKit/appPrefs.json", true);
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        ofJson j;
        j["circleRes"]       = m_prefs.circleRes;
        j["targetFps"]       = m_prefs.targetFps;
        j["vsync"]           = m_prefs.vsync;
        j["backgroundAuto"] = m_prefs.backgroundAuto;
        j["lineWidth"]       = m_prefs.lineWidth;
        j["smoothLighting"] = m_prefs.smoothLighting;
        j["depthTest"]       = m_prefs.depthTest;
        j["logLevel"]        = m_prefs.logLevel;
        j["rulerScale"]      = m_prefs.rulerScale;
        j["rulerUnit"]       = static_cast<int>(m_prefs.rulerUnit);
        j["hideAllUI"]       = m_prefs.hideAllUI;
        j["bgR"]             = static_cast<int>(m_prefs.bgColor.r);
        j["bgG"]             = static_cast<int>(m_prefs.bgColor.g);
        j["bgB"]             = static_cast<int>(m_prefs.bgColor.b);
        j["bgA"]             = static_cast<int>(m_prefs.bgColor.a);
        j["showMarginRect"]  = m_prefs.showMarginRect;
        j["marginR"]         = static_cast<int>(m_prefs.marginColor.r);
        j["marginG"]         = static_cast<int>(m_prefs.marginColor.g);
        j["marginB"]         = static_cast<int>(m_prefs.marginColor.b);
        j["marginA"]         = static_cast<int>(m_prefs.marginColor.a);

        ofJson vis = ofJson::object();
        for (const auto& w : m_windows)
            vis[w.id.empty() ? w.name : w.id] = w.visible;
        j["windowVisibility"] = vis;

        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) {}
}

void Runtime::buildDefaultDockLayout(ImGuiID dockId)
{
    const ImVec2 size = ImGui::GetMainViewport()->WorkSize;

    ImGui::DockBuilderRemoveNode(dockId);
    ImGuiDockNodeFlags addNodeFlags = ImGuiDockNodeFlags_DockSpace;
    if (m_passthruCentralNode)
        addNodeFlags |= ImGuiDockNodeFlags_PassthruCentralNode
                     |  ImGuiDockNodeFlags_NoDockingOverCentralNode;
    ImGui::DockBuilderAddNode(dockId, addNodeFlags);
    ImGui::DockBuilderSetNodeSize(dockId, size);

    ImGuiID left, right, centre = dockId;
    ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.25f, &left, &centre);
    ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.28f, &right, &centre);

    ImGui::DockBuilderDockWindow("Scene###ofxkit.window.scene", left);
    for (const auto& name : m_defaultLayoutExtraLeftDocks) {
        if (!name.empty())
            ImGui::DockBuilderDockWindow(name.c_str(), left);
    }
    ImGui::DockBuilderDockWindow("Properties###ofxkit.window.properties", right);
    ImGui::DockBuilderDockWindow("Shortcuts###ofxkit.window.shortcuts", right);
    ImGui::DockBuilderDockWindow("Preferences###ofxkit.window.preferences", right);
    for (const auto& name : m_defaultLayoutExtraRightDocks) {
        if (!name.empty())
            ImGui::DockBuilderDockWindow(name.c_str(), right);
    }
    for (const auto& name : m_defaultLayoutExtraCenterDocks) {
        if (!name.empty())
            ImGui::DockBuilderDockWindow(name.c_str(), centre);
    }

    ImGui::DockBuilderFinish(dockId);

    if (ImGuiDockNode* cn = ImGui::DockBuilderGetCentralNode(dockId)) {
        // Only suppress the tab bar when the centre is purely passthru/empty.
        // When app windows are seeded here, keep the tab bar so titles show.
        if (m_defaultLayoutExtraCenterDocks.empty())
            cn->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        if (m_passthruCentralNode)
            cn->LocalFlags |= ImGuiDockNodeFlags_PassthruCentralNode
                           |  ImGuiDockNodeFlags_NoDockingOverCentralNode;
    }
}

} // namespace ofkitty
