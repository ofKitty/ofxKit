#include "Runtime.h"
#include "Runtime_private.h"

#include <ofxEnTTKit/src/component_editor_registration.h>

#include "ofJson.h"

#include <ofxImGuiStyle/src/ofxImGuiStyle.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

#if !defined(TARGET_OPENGLES) && !defined(TARGET_RASPBERRY_PI)
#  define OFXKIT_HAS_GLFW 1
#  include <GLFW/glfw3.h>
#endif

namespace ofkitty {

float Runtime::detectUIScale()
{
#ifdef OFXKIT_HAS_GLFW
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        float xs = 1.f, ys = 1.f;
        glfwGetMonitorContentScale(monitor, &xs, &ys);
        float scale = std::max(xs, ys);
        if (scale > 0.1f && scale < 8.f)
            return scale;
    }
#endif
    return 1.0f;
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
    m_style.applyScale(m_uiScale);
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMinSize = ImVec2(160.f * m_uiScale, 50.f * m_uiScale);
}

void Runtime::loadUIScalePref()
{
    std::string path = ofToDataPath("ofxKit/uiScale.json", true);
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
    std::string path = ofToDataPath("ofxKit/uiScale.json", true);
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        ofJson        j;
        j["uiScale"] = m_uiScale;
        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) {}
}

void Runtime::setTheme(Theme theme)
{
    m_theme        = theme;
    m_themeSet     = true;
    if (!ImGui::GetCurrentContext()) {
        saveThemePref();
        return;
    }
    ImGui::GetStyle() = ImGuiStyle{};
    applyTheme();
    m_style.captureBaseStyle();
    applyUIScale();
    saveThemePref();
}

void Runtime::applyTheme()
{
    if (!ImGui::GetCurrentContext())
        return;
    switch (m_theme) {
        case Theme::Dark: ofxImGuiStyle::applyDarkTheme(); break;
        case Theme::Light: ofxImGuiStyle::applyLightTheme(); break;
        case Theme::Classic: ofxImGuiStyle::applyClassicTheme(); break;
    }
}

void Runtime::loadThemePref()
{
    std::string path = ofToDataPath("ofxKit/theme.json", true);
    if (!of::filesystem::exists(of::filesystem::path(path)))
        return;
    try {
        std::ifstream in(path);
        ofJson        j;
        in >> j;
        if (j.contains("theme")) {
            std::string s = j["theme"].get<std::string>();
            if (s == "dark")
                m_theme = Theme::Dark;
            else if (s == "light")
                m_theme = Theme::Light;
            else if (s == "classic")
                m_theme = Theme::Classic;
            m_themeSet = true;
        }
    } catch (...) {}
}

void Runtime::saveThemePref()
{
    std::string path = ofToDataPath("ofxKit/theme.json", true);
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        const char* s = "dark";
        switch (m_theme) {
            case Theme::Dark: s = "dark"; break;
            case Theme::Light: s = "light"; break;
            case Theme::Classic: s = "classic"; break;
        }
        ofJson        j;
        j["theme"] = s;
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
