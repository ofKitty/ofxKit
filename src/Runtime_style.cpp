#include "Runtime.h"
#include "Runtime_private.h"

#include "component_editor_registration.h"

#include "ofJson.h"

#include "ImTheme.h"
#include "ImThemeRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace ofkitty {

float Runtime::detectUIScale()
{
    // Delegate to ImTheme so the GLFW HiDPI query only lives in one place
    // (addons/ofxImGuiStyle/src/ImThemeRegistry.cpp). This stays here as a
    // thin compatibility wrapper because the function is part of ofxKit's
    // public API (declared static in Runtime.h).
    return ImTheme::DetectOsScale();
}

void Runtime::setUIScale(float scale)
{
    scale        = std::clamp(scale, 0.5f, 4.0f);
    m_uiScale    = scale;
    m_uiScaleSet = true;
    if (ImGui::GetCurrentContext())
        applyUIScale();
    saveUIScalePref();
}

void Runtime::applyUIScale()
{
    if (!ImGui::GetCurrentContext())
        return;
    ImTheme::SetUIScale(m_uiScale);
    applyThemeHueShift();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMinSize = ImVec2(160.f * m_uiScale, 50.f * m_uiScale);
}

void Runtime::loadUIScalePref()
{
    std::string path = dataPath("uiScale.json");
    if (!of::filesystem::exists(of::filesystem::path(path)))
        return;
    try {
        std::ifstream in(path);
        ofJson        j;
        in >> j;
        if (j.contains("uiScale")) {
            m_uiScale = std::clamp(j["uiScale"].get<float>(), 0.5f, 4.0f);
            m_uiScaleSet = true;
        }
    } catch (...) {}
}

void Runtime::saveUIScalePref()
{
    std::string path = dataPath("uiScale.json");
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        ofJson        j;
        j["uiScale"] = m_uiScale;
        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) {}
}

void Runtime::setTheme(const std::string& id)
{
    m_themeId  = id;
    m_themeSet = true;
    if (!ImGui::GetCurrentContext()) {
        saveThemePref();
        return;
    }
    ImTheme::SetUIScale(m_uiScale);
    applyTheme();
    applyUIScale();
    saveThemePref();
}

void Runtime::applyThemeHueShift()
{
    if (m_hueShift < 0.f || !ImGui::GetCurrentContext())
        return;

    const float hueOffset =
        std::fmod(ofGetElapsedTimef() * m_hueShift, 1.f);

    ImGuiStyle& style = ImGui::GetStyle();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        ImVec4& col = style.Colors[i];
        float   h, s, v;
        ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);
        if (s > 0.05f) {
            h += hueOffset;
            if (h >= 1.f)
                h -= 1.f;
            else if (h < 0.f)
                h += 1.f;
        }
        ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);
    }
    // Do not Commit() here — that would snapshot already-scaled metrics as the
    // new baseline and double-apply UI scale (padding / spacing jump).
}

void Runtime::applyTheme()
{
    if (!ImGui::GetCurrentContext())
        return;
    if (!ImTheme::ApplyByName(m_themeId)) {
        ofLogWarning("ofxKit") << "Theme '" << m_themeId
                               << "' not registered - falling back to '"
                               << kDefaultThemeId << "'.";
        ImTheme::Setup(ImTheme::Theme_DarculaDarker, m_uiScale);
        m_themeId = kDefaultThemeId;
    }
    applyThemeHueShift();
}

void Runtime::loadThemePref()
{
    std::string path = dataPath("theme.json");
    if (!of::filesystem::exists(of::filesystem::path(path)))
        return;
    try {
        std::ifstream in(path);
        ofJson        j;
        in >> j;
        if (j.contains("theme")) {
            m_themeId  = j["theme"].get<std::string>();
            m_themeSet = true;
        }
        if (j.contains("hueShiftSpeed")) {
            m_hueShift = j["hueShiftSpeed"].get<float>();
            if (m_hueShift >= 0.f)
                m_hueShift = std::clamp(m_hueShift, 0.001f, 2.f);
        } else if (j.contains("hueShift")) {
            // Legacy: stored a static hue offset — enable at a gentle default speed.
            const float v = j["hueShift"].get<float>();
            m_hueShift = (v >= 0.f) ? 0.05f : -1.f;
        }
    } catch (...) {}
}

void Runtime::saveThemePref()
{
    std::string path = dataPath("theme.json");
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        ofJson j;
        j["theme"]         = m_themeId;
        j["hueShiftSpeed"] = m_hueShift;
        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) {}
}

void Runtime::ensureAppName()
{
    if (!m_appName.empty())
        return;

    std::string name = ofPathToString(of::filesystem::path(ofFilePath::getAppName()).stem());
    if (name.empty()) {
        m_appName = "ofKitty";
        return;
    }

    std::replace(name.begin(), name.end(), '_', ' ');
    std::replace(name.begin(), name.end(), '-', ' ');

    bool capitalizeNext = true;
    for (char& c : name) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isspace(uc)) {
            capitalizeNext = true;
            continue;
        }
        c = capitalizeNext ? static_cast<char>(std::toupper(uc))
                           : static_cast<char>(std::tolower(uc));
        capitalizeNext = false;
    }

    m_appName = name;
}

void Runtime::registerBuiltInComponents()
{
    if (m_componentMenuFinalized)
        return;
    m_componentMenuFinalized = true;

    ecs::finalizeComponentMenu();
}

} // namespace ofkitty
