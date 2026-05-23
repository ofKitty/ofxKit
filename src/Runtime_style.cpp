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
    } catch (...) {}
}

void Runtime::saveThemePref()
{
    std::string path = dataPath("theme.json");
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        ofJson j;
        j["theme"] = m_themeId;
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

void Runtime::registerComponent(ComponentDescriptor desc)
{
    m_components.push_back(std::move(desc));
}

const std::vector<Runtime::ComponentDescriptor>& Runtime::componentDescriptors() const
{
    return m_components;
}

std::vector<std::string> Runtime::componentCategories() const
{
    std::vector<std::string> cats;
    for (auto& d : m_components) {
        if (std::find(cats.begin(), cats.end(), d.category) == cats.end())
            cats.push_back(d.category);
    }
    return cats;
}

void Runtime::registerBuiltInComponents()
{
    if (m_builtInComponentsRegistered)
        return;
    m_builtInComponentsRegistered = true;

    ecs::registerKitComponentMenu([this](const ecs::ComponentMenuEntry& row) {
        ComponentDescriptor d;
        d.name        = row.name;
        d.category    = row.category;
        d.description = row.description;
        d.has         = row.has;
        d.add         = row.add;
        d.remove      = row.remove;
        registerComponent(std::move(d));
    });
}

} // namespace ofkitty
