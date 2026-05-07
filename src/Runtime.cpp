#include "Runtime.h"

#include <ofxEnTTInspector/src/ofxEnTTInspector.h>
#include "ofJson.h"

#include <algorithm>
#include <cctype>
#include <fstream>

// GLFW is the desktop OF window backend and is what ofxImGui uses too.
// Skip on mobile / RPi targets where GLFW isn't present.
#if !defined(TARGET_OPENGLES) && !defined(TARGET_RASPBERRY_PI)
#  define OFXKIT_HAS_GLFW 1
#  include <GLFW/glfw3.h>
#endif

namespace ofkitty {

// ============================================================================
// Singleton
// ============================================================================

Runtime& Runtime::instance()
{
    static Runtime s_instance;
    return s_instance;
}

// ============================================================================
// attach
// ============================================================================

void Runtime::attach(shared_ptr<ofAppBaseWindow> window,
                     shared_ptr<ofBaseApp>       app)
{
    Runtime& rt = instance();
    rt.attachInternal(window, app, rt.m_registry);
}

void Runtime::attach(shared_ptr<ofAppBaseWindow> window,
                     shared_ptr<ofBaseApp>       app,
                     entt::registry&             registry)
{
    instance().attachInternal(window, app, registry);
}

void Runtime::attachInternal(shared_ptr<ofAppBaseWindow> /*window*/,
                             shared_ptr<ofBaseApp>       /*app*/,
                             entt::registry&             registry)
{
    if (m_attached) return;

    m_registryView = &registry;
    m_selected = entt::null;
    m_attached = true;

    ofAddListener(ofEvents().setup,      this, &Runtime::onSetup,      OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().update,     this, &Runtime::onUpdate,     OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().draw,       this, &Runtime::onDraw,       OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().keyPressed, this, &Runtime::onShortcutCaptureBeforeApp,
                  OF_EVENT_ORDER_BEFORE_APP);
    ofAddListener(ofEvents().keyPressed, this, &Runtime::onKeyPressed, OF_EVENT_ORDER_AFTER_APP);
}

// ============================================================================
// Event handlers
// ============================================================================

void Runtime::onSetup(ofEventArgs&)
{
    ensureAppName();
    registerBuiltInWindows();

    ImGuiConfigFlags imguiFlags = ImGuiConfigFlags_DockingEnable;
#ifndef TARGET_OPENGLES
    imguiFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
    m_gui.setup(nullptr, false, imguiFlags, true);

    // If a custom ini path was registered before setup, apply it now that
    // the ImGui context exists.
    if (!m_imguiIniPath.empty()) {
        ImGui::GetIO().IniFilename = m_imguiIniPath.c_str();
    }

    // Apply theme first (loads persisted choice if any), then capture the
    // base style with theme colours, then layer the scale on top.
    if (!m_themeSet) loadThemePref();
    applyTheme();

    m_baseStyle          = ImGui::GetStyle();
    m_baseStyleCaptured  = true;

    // Auto-detect HiDPI / 4K scale unless the user explicitly set one
    // before setup, or a saved preference exists in bin/data/ofxKit/.
    if (!m_uiScaleSet) {
        loadUIScalePref();
    }
    if (!m_uiScaleSet) {
        m_uiScale = detectUIScale();
    }
    applyUIScale();

    for (auto& hook : m_postSetupHooks)
        hook(m_gui);

    // Built-in shortcut — defaults + optional merge from data/ofxKit/shortcuts.json
    m_shortcuts.setAutoSaveEnabled(false);
#ifdef TARGET_OSX
    constexpr int kToggleEditMod = OF_KEY_COMMAND;
#else
    constexpr int kToggleEditMod = OF_KEY_CONTROL;
#endif
    m_shortcuts.registerAction(
        "ofkitty.toggle_edit",
        'e',
        kToggleEditMod,
        "Toggle Edit mode",
        [this] { toggleEditMode(); });
    m_shortcuts.loadBindingsFromFile(ShortcutManager::defaultBindingsPath());
    m_shortcuts.setAutoSaveEnabled(true);
}

void Runtime::onUpdate(ofEventArgs&)
{
    // Future: TransformSystem, physics, etc.
}

void Runtime::onDraw(ofEventArgs&)
{
    if (m_windows.empty()) return;
    drawOverlay();
}

void Runtime::onShortcutCaptureBeforeApp(ofKeyEventArgs& e)
{
    if (m_shortcuts.handleCaptureKey(e)) {
        m_skipShortcutDispatch = true;
    }
}

void Runtime::onKeyPressed(ofKeyEventArgs& e)
{
    if (m_skipShortcutDispatch) {
        m_skipShortcutDispatch = false;
        return;
    }
    m_shortcuts.dispatch(e.key);
}

// ============================================================================
// Overlay
// ============================================================================

void Runtime::drawOverlay()
{
    m_gui.begin();

    renderMainMenuBar();
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    for (auto& window : m_windows) {
        if (window.visible && window.draw) {
            if (!window.editModeOnly || m_editMode)
                window.draw(window.visible);
        }
    }

    m_gui.end();
    // Manual draw mode requires explicit draw() to render the frame.
    // Without this, Gui::end() only calls ImGui::EndFrame() and nothing
    // is ever submitted to OpenGL — invisible menu bar / windows.
    m_gui.draw();
}

void Runtime::renderMainMenuBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 8));
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();

        if (ImGui::BeginMenu(m_appName.c_str())) {
            if (m_editMode) {
                if (ImGui::MenuItem("Hide Edit Mode", "Cmd+E")) {
                    setEditMode(false);
                }
            } else {
                if (ImGui::MenuItem("Show Edit Mode", "Cmd+E")) {
                    setEditMode(true);
                }
            }

            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Enhanced Dark",  nullptr, m_theme == Theme::EnhancedDark))  setTheme(Theme::EnhancedDark);
                if (ImGui::MenuItem("Enhanced Light", nullptr, m_theme == Theme::EnhancedLight)) setTheme(Theme::EnhancedLight);
                ImGui::Separator();
                if (ImGui::MenuItem("ImGui Dark",     nullptr, m_theme == Theme::Dark))          setTheme(Theme::Dark);
                if (ImGui::MenuItem("ImGui Light",    nullptr, m_theme == Theme::Light))         setTheme(Theme::Light);
                if (ImGui::MenuItem("ImGui Classic",  nullptr, m_theme == Theme::Classic))       setTheme(Theme::Classic);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("UI Scale")) {
                float scale = m_uiScale;
                if (ImGui::SliderFloat("##scale", &scale, 0.5f, 3.0f, "%.2f x")) {
                    setUIScale(scale);
                }
                ImGui::Separator();
                struct { const char* label; float value; } presets[] = {
                    { "1.0x  (native)",   1.0f },
                    { "1.25x (mid HiDPI)", 1.25f },
                    { "1.5x  (HiDPI)",    1.5f  },
                    { "2.0x  (4K)",       2.0f  },
                    { "2.5x  (4K large)", 2.5f  },
                };
                for (auto& p : presets) {
                    bool sel = std::fabs(m_uiScale - p.value) < 0.01f;
                    if (ImGui::MenuItem(p.label, nullptr, sel)) setUIScale(p.value);
                }
                ImGui::Separator();
                float autoScale = detectUIScale();
                std::string autoLabel = "Auto-detect (" + ofToString(autoScale, 2) + "x)";
                if (ImGui::MenuItem(autoLabel.c_str())) setUIScale(autoScale);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                ofExit();
            }
            ImGui::EndMenu();
        }

        // Registered external groups (File, Edit, etc. from ofxBapp)
        for (auto& [groupName, cb] : m_menuGroups) {
            if (ImGui::BeginMenu(groupName.c_str())) {
                cb();
                ImGui::EndMenu();
            }
        }

        // Raw callbacks — each handles its own BeginMenu/EndMenu calls
        for (auto& cb : m_menuBarRawCallbacks) cb();

        if (ImGui::BeginMenu("View")) {
            for (auto& window : m_windows) {
                if (window.menuGroup == "View") {
                    ImGui::MenuItem(window.name.c_str(), nullptr, &window.visible);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    } else {
        ImGui::PopStyleVar();
    }
}

Runtime::RuntimeWindow* Runtime::registerWindow(RuntimeWindow window)
{
    if (window.name.empty()) {
        ofLogWarning("ofxKit") << "Cannot register a window with no name.";
        return nullptr;
    }

    if (auto* existing = findWindow(window.name)) {
        ofLogWarning("ofxKit") << "Window '" << window.name << "' is already registered.";
        return existing;
    }

    m_windows.push_back(std::move(window));
    return &m_windows.back();
}

Runtime::RuntimeWindow* Runtime::findWindow(const std::string& name)
{
    for (auto& window : m_windows) {
        if (window.name == name) {
            return &window;
        }
    }
    return nullptr;
}

const Runtime::RuntimeWindow* Runtime::findWindow(const std::string& name) const
{
    for (const auto& window : m_windows) {
        if (window.name == name) {
            return &window;
        }
    }
    return nullptr;
}

bool Runtime::setWindowVisible(const std::string& name, bool visible)
{
    if (auto* window = findWindow(name)) {
        window->visible = visible;
        return true;
    }
    return false;
}

void Runtime::registerBuiltInWindows()
{
    if (m_builtInWindowsRegistered) return;

    registerWindow({"Toolbar",    "View", true,  false, [this](bool& visible) { drawToolbarWindow(visible);    }});
    registerWindow({"Scene",      "View", true,  true,  [this](bool& visible) { drawSceneWindow(visible);      }});
    registerWindow({"Properties", "View", true,  true,  [this](bool& visible) { drawPropertiesWindow(visible); }});
    registerWindow({"Shortcuts",  "View", true,  true,  [this](bool& visible) { drawShortcutsWindow(visible);  }});

    m_builtInWindowsRegistered = true;
}

void Runtime::addMenuBarGroup(const std::string& groupName, MenuBarCallback cb)
{
    for (auto& [name, existing] : m_menuGroups) {
        if (name == groupName) {
            ofLogWarning("ofxKit") << "Menu group '" << groupName << "' is already registered.";
            return;
        }
    }
    m_menuGroups.emplace_back(groupName, std::move(cb));
}

void Runtime::addPostSetupHook(PostSetupHook hook)
{
    m_postSetupHooks.push_back(std::move(hook));
}

void Runtime::addMenuBarRawCallback(MenuBarCallback cb)
{
    m_menuBarRawCallbacks.push_back(std::move(cb));
}

void Runtime::drawSceneWindow(bool& visible)
{
    auto& reg = registry();

    ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_Once);

    if (ImGui::Begin("Scene", &visible)) {
        ImGui::Text("Registry: %d entities",
                    (int)reg.storage<entt::entity>().size());
        ImGui::Separator();

        for (entt::entity e : reg.storage<entt::entity>()) {
            ImGui::PushID((int)e);
            bool isSelected = (e == m_selected);
            std::string label = "Entity " + ofToString((unsigned)e);
            if (auto* node = reg.try_get<ecs::node_component>(e)) {
                if (!node->getName().empty()) {
                    label = node->getName();
                }
            }
            if (ImGui::Selectable(label.c_str(), isSelected))
                m_selected = e;
            ImGui::PopID();
        }

        ImGui::Separator();

        if (m_selected != entt::null && reg.valid(m_selected))
            ImGui::Text("Selected: %u", (unsigned)m_selected);
    }
    ImGui::End();
}

void Runtime::drawPropertiesWindow(bool& visible)
{
    auto& reg = registry();

    ImGui::SetNextWindowPos(ImVec2(320, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_Once);

    if (ImGui::Begin("Properties", &visible)) {
        if (m_selected != entt::null && reg.valid(m_selected)) {
            inspector::inspectEntity(reg, m_selected);
        } else {
            ImGui::TextDisabled("Select an entity in the Scene window.");
        }
    }
    ImGui::End();
}

void Runtime::drawShortcutsWindow(bool& visible)
{
    ImGui::SetNextWindowPos(ImVec2(10, 550), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Once);

    if (ImGui::Begin("Shortcuts", &visible)) {
        ImGui::TextWrapped(
            "Named shortcuts can be remapped below; changes are saved to %s",
            ShortcutManager::defaultBindingsPath().c_str());
        ImGui::Spacing();

        if (ImGui::BeginTable("shortcut_rows", 3, ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90.f);
            ImGui::TableHeadersRow();

            for (const auto& s : m_shortcuts.all()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(s.description.c_str());
                ImGui::TableSetColumnIndex(1);
                const bool capturing =
                    m_shortcuts.isCapturing() && m_shortcuts.captureActionId() == s.actionId;
                if (capturing) {
                    ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f),
                                       "Press new shortcut... (Esc cancel)");
                } else {
                    ImGui::TextUnformatted(
                        ShortcutManager::formatBindingLabel(s.key, s.modifiers).c_str());
                }
                ImGui::TableSetColumnIndex(2);
                if (!s.actionId.empty()) {
                    ImGui::PushID(s.actionId.c_str());
                    if (capturing) {
                        if (ImGui::Button("Cancel")) {
                            m_shortcuts.cancelCapture();
                        }
                    } else if (ImGui::Button("Change...")) {
                        m_shortcuts.beginCapture(s.actionId);
                    }
                    ImGui::PopID();
                } else {
                    ImGui::TextDisabled("—");
                }
            }
            ImGui::EndTable();
        }

        if (!m_shortcuts.lastCaptureError().empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s",
                               m_shortcuts.lastCaptureError().c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("Save bindings now")) {
            m_shortcuts.saveBindingsToFile(ShortcutManager::defaultBindingsPath());
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload from disk")) {
            m_shortcuts.loadBindingsFromFile(ShortcutManager::defaultBindingsPath());
        }
    }
    ImGui::End();
}

// ============================================================================
// Toolbar
// ============================================================================

void Runtime::registerToolbarItem(ToolbarItem item)
{
    if (item.id.empty()) {
        ofLogWarning("ofxKit") << "Cannot register a toolbar item with no id.";
        return;
    }
    for (const auto& existing : m_toolbarItems) {
        if (existing.id == item.id) {
            ofLogWarning("ofxKit") << "Toolbar item '" << item.id << "' is already registered.";
            return;
        }
    }
    m_toolbarItems.push_back(std::move(item));
}

bool Runtime::unregisterToolbarItem(const std::string& id)
{
    auto it = std::find_if(m_toolbarItems.begin(), m_toolbarItems.end(),
                           [&](const ToolbarItem& t) { return t.id == id; });
    if (it == m_toolbarItems.end()) return false;
    m_toolbarItems.erase(it);
    return true;
}

void Runtime::drawToolbarWindow(bool& visible)
{
    if (m_toolbarItems.empty()) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always); // auto-size

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(2, 2));

    if (ImGui::Begin("Toolbar", &visible, flags)) {
        const ImVec2 btnSize(32, 32);
        const ImVec2 framePad(4, 4);
        const ImVec4 activeCol(0.2f, 0.6f, 0.9f, 1.0f);

        std::string prevGroup = "\x01"; // sentinel — not a valid group string

        for (const auto& item : m_toolbarItems) {
            // Separator between groups (only when both sides are named groups)
            if (!item.group.empty() && item.group != prevGroup && prevGroup != "\x01") {
                ImGui::Separator();
            }
            prevGroup = item.group;

            bool active = item.isActive && item.isActive();
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, activeCol);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePad);

            const char* label = (item.icon && item.icon[0] != '\0') ? item.icon : item.id.c_str();
            ImGui::PushID(item.id.c_str());
            bool clicked = ImGui::Button(label, btnSize);
            ImGui::PopID();

            ImGui::PopStyleVar();
            if (active) ImGui::PopStyleColor();

            if (!item.tooltip.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", item.tooltip.c_str());
            }

            if (clicked && item.onSelect) {
                item.onSelect();
            }
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::End();
}

void Runtime::setAppName(std::string name)
{
    if (!name.empty()) {
        m_appName = std::move(name);
    }
}

void Runtime::setImGuiIniPath(std::string path)
{
    m_imguiIniPath = std::move(path);
    // If ImGui is already initialised, apply immediately. Otherwise onSetup
    // will pick this up after m_gui.setup() runs.
    if (ImGui::GetCurrentContext()) {
        ImGui::GetIO().IniFilename = m_imguiIniPath.empty()
            ? nullptr
            : m_imguiIniPath.c_str();
    }
}

// ============================================================================
// UI scale
// ============================================================================

float Runtime::detectUIScale()
{
#ifdef OFXKIT_HAS_GLFW
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        float xs = 1.f, ys = 1.f;
        glfwGetMonitorContentScale(monitor, &xs, &ys);
        // X and Y content scale are normally identical; pick the larger
        // to be safe so widgets are never undersized on mixed-DPI rigs.
        float scale = std::max(xs, ys);
        if (scale > 0.1f && scale < 8.f) return scale;
    }
#endif
    return 1.0f;
}

void Runtime::setUIScale(float scale)
{
    // Clamp to a sane range so a corrupt prefs file can't wedge the UI.
    scale = std::clamp(scale, 0.5f, 4.0f);
    m_uiScale    = scale;
    m_uiScaleSet = true;
    if (m_baseStyleCaptured) applyUIScale();
    saveUIScalePref();
}

void Runtime::applyUIScale()
{
    if (!ImGui::GetCurrentContext() || !m_baseStyleCaptured) return;
    ImGuiStyle& style = ImGui::GetStyle();
    style = m_baseStyle;             // restore unscaled baseline
    style.ScaleAllSizes(m_uiScale);  // then scale once
    ImGui::GetIO().FontGlobalScale = m_uiScale;
}

void Runtime::loadUIScalePref()
{
    std::string path = ofToDataPath("ofxKit/uiScale.json", true);
    if (!of::filesystem::exists(of::filesystem::path(path))) return;
    try {
        std::ifstream in(path);
        ofJson j; in >> j;
        if (j.contains("uiScale")) {
            m_uiScale    = std::clamp(j["uiScale"].get<float>(), 0.5f, 4.0f);
            m_uiScaleSet = true;
        }
    } catch (...) { /* corrupt file — fall through to auto-detect */ }
}

void Runtime::saveUIScalePref()
{
    std::string path = ofToDataPath("ofxKit/uiScale.json", true);
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        ofJson j; j["uiScale"] = m_uiScale;
        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) { /* don't crash the app on a save failure */ }
}

// ============================================================================
// Theme
// ============================================================================

namespace {

/// Doug Binks' enhanced theme (https://gist.github.com/dougbinks/8089b4bbaccaaf6fa204236978d165a9).
/// Originally a refined version of itamago's light style; the dark variant is
/// produced by inverting the value channel of low-saturation colours.
/// Ported to modern ImGui colour enums (ImGuiCol_ChildWindowBg, ImGuiCol_Column*,
/// ImGuiCol_CloseButton*, ImGuiCol_ComboBg, ImGuiCol_ModalWindowDarkening have
/// all been renamed or removed since the gist was written).
void applyEnhancedTheme(bool darkVariant, float alpha)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha         = 1.0f;
    style.FrameRounding = 3.0f;

    auto& C = style.Colors;
    C[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    C[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    C[ImGuiCol_WindowBg]              = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
    C[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    C[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    C[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
    C[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    C[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    C[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    C[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    C[ImGuiCol_TitleBg]               = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    C[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    C[ImGuiCol_TitleBgActive]         = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    C[ImGuiCol_MenuBarBg]             = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    C[ImGuiCol_ScrollbarBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    C[ImGuiCol_ScrollbarGrab]         = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    C[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    C[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    C[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    C[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    C[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    C[ImGuiCol_Button]                = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    C[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    C[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    C[ImGuiCol_Header]                = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    C[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    C[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    C[ImGuiCol_Separator]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    C[ImGuiCol_SeparatorHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    C[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    C[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
    C[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    C[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    C[ImGuiCol_Tab]                   = ImVec4(0.76f, 0.80f, 0.84f, 0.93f);
    C[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    C[ImGuiCol_TabActive]             = ImVec4(0.60f, 0.73f, 0.88f, 1.00f);
    C[ImGuiCol_TabUnfocused]          = ImVec4(0.92f, 0.93f, 0.94f, 0.99f);
    C[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.74f, 0.82f, 0.91f, 1.00f);
    C[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    C[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    C[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    C[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    C[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    C[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    C[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    C[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

    if (darkVariant) {
        // Invert the value channel of low-saturation colours to get a dark palette,
        // and modulate alpha by `alpha`. Saturated accent hues are preserved.
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            ImVec4& col = C[i];
            float h = 0.f, s = 0.f, v = 0.f;
            ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);
            if (s < 0.1f) {
                v = 1.0f - v;
                ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);
            }
            if (col.w < 1.00f) col.w *= alpha;
        }
    } else {
        // Light variant: just modulate alpha for translucent colours.
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            ImVec4& col = C[i];
            if (col.w < 1.00f) {
                col.x *= alpha; col.y *= alpha; col.z *= alpha; col.w *= alpha;
            }
        }
    }
}

} // namespace

void Runtime::setTheme(Theme theme)
{
    m_theme    = theme;
    m_themeSet = true;
    if (!ImGui::GetCurrentContext()) {
        // Setup hasn't run yet — applyTheme() / scale will run from onSetup.
        saveThemePref();
        return;
    }
    applyTheme();
    // Re-capture the new theme's base style and reapply the active scale on
    // top so widgets keep their HiDPI sizes after a theme change.
    m_baseStyle = ImGui::GetStyle();
    applyUIScale();
    saveThemePref();
}

void Runtime::applyTheme()
{
    if (!ImGui::GetCurrentContext()) return;
    switch (m_theme) {
        case Theme::EnhancedDark:  applyEnhancedTheme(true,  1.0f); break;
        case Theme::EnhancedLight: applyEnhancedTheme(false, 1.0f); break;
        case Theme::Dark:          ImGui::StyleColorsDark();        break;
        case Theme::Light:         ImGui::StyleColorsLight();       break;
        case Theme::Classic:       ImGui::StyleColorsClassic();     break;
    }
}

void Runtime::loadThemePref()
{
    std::string path = ofToDataPath("ofxKit/theme.json", true);
    if (!of::filesystem::exists(of::filesystem::path(path))) return;
    try {
        std::ifstream in(path);
        ofJson j; in >> j;
        if (j.contains("theme")) {
            std::string s = j["theme"].get<std::string>();
            if      (s == "enhanced_dark")  m_theme = Theme::EnhancedDark;
            else if (s == "enhanced_light") m_theme = Theme::EnhancedLight;
            else if (s == "dark")           m_theme = Theme::Dark;
            else if (s == "light")          m_theme = Theme::Light;
            else if (s == "classic")        m_theme = Theme::Classic;
            m_themeSet = true;
        }
    } catch (...) { /* corrupt file — fall through to default EnhancedDark */ }
}

void Runtime::saveThemePref()
{
    std::string path = ofToDataPath("ofxKit/theme.json", true);
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        const char* s = "enhanced_dark";
        switch (m_theme) {
            case Theme::EnhancedDark:  s = "enhanced_dark";  break;
            case Theme::EnhancedLight: s = "enhanced_light"; break;
            case Theme::Dark:          s = "dark";           break;
            case Theme::Light:         s = "light";          break;
            case Theme::Classic:       s = "classic";        break;
        }
        ofJson j; j["theme"] = s;
        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) { /* don't crash the app on a save failure */ }
}

void Runtime::ensureAppName()
{
    if (!m_appName.empty()) return;

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
        c = capitalizeNext
            ? static_cast<char>(std::toupper(uc))
            : static_cast<char>(std::tolower(uc));
        capitalizeNext = false;
    }

    m_appName = name;
}

} // namespace ofkitty
