#include "Runtime.h"

#include <ofxEnTTInspector/src/ofxEnTTInspector.h>
#include <ofxImGui/src/ofxImGui.h>

#include <algorithm>
#include <cctype>
#include <utility>

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

static ofxImGui::Gui s_gui;

void Runtime::onSetup(ofEventArgs&)
{
    ensureAppName();
    registerBuiltInWindows();

    ImGuiConfigFlags imguiFlags = ImGuiConfigFlags_DockingEnable;
#ifndef TARGET_OPENGLES
    imguiFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
    s_gui.setup(nullptr, false, imguiFlags, true);

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
    if (!m_editMode) return;
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
    s_gui.begin();

    renderMainMenuBar();
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    for (auto& window : m_windows) {
        if (window.visible && window.draw) {
            window.draw(window.visible);
        }
    }

    s_gui.end();
}

void Runtime::renderMainMenuBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 8));
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();

        if (ImGui::BeginMenu(m_appName.c_str())) {
            if (ImGui::MenuItem("Hide Edit Mode", "Cmd+E")) {
                setEditMode(false);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                ofExit();
            }
            ImGui::EndMenu();
        }

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

    registerWindow({"Scene", "View", true, [this](bool& visible) {
        drawSceneWindow(visible);
    }});
    registerWindow({"Properties", "View", true, [this](bool& visible) {
        drawPropertiesWindow(visible);
    }});
    registerWindow({"Shortcuts", "View", true, [this](bool& visible) {
        drawShortcutsWindow(visible);
    }});

    m_builtInWindowsRegistered = true;
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

void Runtime::setAppName(std::string name)
{
    if (!name.empty()) {
        m_appName = std::move(name);
    }
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
