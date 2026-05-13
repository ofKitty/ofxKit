#include "Runtime.h"
#include "panels/CodeEditorPanel.h"
#include "panels/PathEditorPanel.h"
#include "ProgressWindow.h"
#include "Runtime_private.h"

#include "ofJson.h"
#include "imgui_internal.h"   // DockBuilder API

#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>

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
    m_selected     = entt::null;
    m_attached     = true;

    ofAddListener(ofEvents().setup,      this, &Runtime::onSetup,      OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().update,     this, &Runtime::onUpdate,     OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().draw,       this, &Runtime::onDraw,       OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().exit,       this, &Runtime::onExit,       OF_EVENT_ORDER_AFTER_APP);
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
    registerBuiltInComponents();
    registerBuiltInPreferencePages();
    registerBuiltInStatusItems();
    ProgressWindow::instance().attachStatusBarItem();
    loadAppPrefs();

    // F2 to toggle rulers
    m_shortcuts.bind(OF_KEY_F2, 0, "Toggle Rulers",
                     [this] { toggleRulers(); });

    ImGuiConfigFlags imguiFlags = ImGuiConfigFlags_DockingEnable;
#ifndef TARGET_OPENGLES
    imguiFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
    m_gui.setup(nullptr, false, imguiFlags, true);

    // Load Input Sans + Font Awesome icons so toolbar / UI can use FA glyphs.
    // Must happen right after gui.setup() before the first frame renders.
    m_style.loadFonts(m_gui, 14.0f, 1.0f);

    // Register event filters that block OF mouse/keyboard events from reaching
    // the app while ImGui has claimed them.  Must happen after m_gui.setup()
    // so the ImGui context exists when the first WantCaptureMouse check fires.
    m_eventHelper.setup();

    // Keep docking/layout state in a stable app data location. ImGui's default
    // "imgui.ini" is relative to the process working directory, which can
    // change depending on how the example is launched.
    if (m_imguiIniPath.empty()) {
        m_imguiIniPath = ofToDataPath("ofxKit/imgui.ini", true);
    }
    detail::createParentDirectoryIfNeeded(m_imguiIniPath);
    ImGui::GetIO().IniFilename = m_imguiIniPath.c_str();
    if (of::filesystem::exists(of::filesystem::path(m_imguiIniPath))) {
        ImGui::LoadIniSettingsFromDisk(m_imguiIniPath.c_str());
    }

    if (!m_codeEditor) {
        m_codeEditor = std::make_unique<CodeEditorPanel>();
        m_codeEditor->setup();
        m_codeEditor->setDialogCallbacks(
            [this](const std::string& key, const std::string& title, const std::string& filters,
                   std::function<void(const std::string& path)> onConfirm) {
                openFileDialog(key, title, filters, std::move(onConfirm));
            },
            [this](const std::string& key, const std::string& title, const std::string& filters,
                   const std::string&                     defaultFileName,
                   std::function<void(const std::string& path)> onConfirm) {
                saveFileDialog(key, title, filters, defaultFileName, std::move(onConfirm));
            });
    }
    if (!m_pathEditor) {
        m_pathEditor = std::make_unique<PathEditorPanel>();
    }

    // Apply the persisted theme through ofxImGuiStyle, then capture that
    // unscaled baseline before ofxKit layers editor-scale policy on top.
    if (!m_themeSet)
        loadThemePref();
    applyTheme();
    m_style.captureBaseStyle();

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

    // Gizmo operation shortcuts (only fire when edit mode is active)
    m_shortcuts.registerAction("ofkitty.gizmo_translate", 'w', 0,
                               "Gizmo: Translate",
                               [this] {
                                   if (m_editMode)
                                       m_gizmoOp = GizmoOperation::Translate;
                               });
    m_shortcuts.registerAction("ofkitty.gizmo_rotate", 'e', 0,
                               "Gizmo: Rotate",
                               [this] {
                                   if (m_editMode)
                                       m_gizmoOp = GizmoOperation::Rotate;
                               });
    m_shortcuts.registerAction("ofkitty.gizmo_scale", 'r', 0,
                               "Gizmo: Scale",
                               [this] {
                                   if (m_editMode)
                                       m_gizmoOp = GizmoOperation::Scale;
                               });
    m_shortcuts.registerAction(
        "ofkitty.gizmo_mode_toggle",
        'x',
        0,
        "Gizmo: Toggle World/Local",
        [this] {
            if (m_editMode)
                m_gizmoMode = (m_gizmoMode == GizmoMode::World) ? GizmoMode::Local
                                                                : GizmoMode::World;
        });

    m_shortcuts.loadBindingsFromFile(ShortcutManager::defaultBindingsPath());
    m_shortcuts.setAutoSaveEnabled(true);
}

void Runtime::onUpdate(ofEventArgs&)
{
    // Future: TransformSystem, physics, etc.
}

void Runtime::onExit(ofEventArgs&)
{
    // Ensure imgui.ini and our prefs are written before the process ends.
    // ImGui's auto-save has a 5-second delay so the last docking state can
    // otherwise be lost if the window is closed quickly.
    saveAppPrefs();
}

void Runtime::toggleEditMode()
{
    const int f = ofGetFrameNum();
    if (m_toggleEditLastFrame == f) {
        return;
    }
    m_toggleEditLastFrame = f;

    m_editMode = !m_editMode;
    ofLogNotice("ofxKit") << "Edit mode " << (m_editMode ? "ON" : "OFF");
}

void Runtime::onDraw(ofEventArgs&)
{
    if (m_windows.empty())
        return;
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
    m_shortcuts.dispatch(e);
}

// ============================================================================
// Overlay
// ============================================================================

void Runtime::drawOverlay()
{
    m_gui.begin();

    if (m_editMode) {
        ImGuizmo::BeginFrame();
        renderMainMenuBar();
    }

    // Always reserve the bottom status strip (progress, optional hints, FPS when visible, …).
    drawStatusBar();

    if (m_editMode) {

        ImGuiID dockId = ImGui::DockSpaceOverViewport(
            0, ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_PassthruCentralNode |
                ImGuiDockNodeFlags_NoDockingOverCentralNode);

        if (!m_defaultLayoutBuilt) {
            bool noIni = true;
            if (const char* iniPath = ImGui::GetIO().IniFilename)
                noIni = !of::filesystem::exists(of::filesystem::path(iniPath));

            if (noIni || m_layoutResetPending) {
                buildDefaultDockLayout(dockId);
                m_layoutResetPending = false;
            }
            m_defaultLayoutBuilt = true;
        }

        if (ImGuiDockNode* cn = ImGui::DockBuilderGetCentralNode(dockId)) {
            cn->LocalFlags |= ImGuiDockNodeFlags_PassthruCentralNode |
                              ImGuiDockNodeFlags_NoDockingOverCentralNode |
                              ImGuiDockNodeFlags_NoTabBar;
        }

        if (m_showRulers)
            drawRulers();

        drawGizmoOverlay();
    }

    for (auto& window : m_windows) {
        if (window.visible && window.draw) {
            if (!window.editModeOnly || m_editMode) {
                bool prev = window.visible;
                ImGui::PushID(window.id.c_str());
                window.draw(window.visible);
                ImGui::PopID();
                if (window.visible != prev)
                    saveAppPrefs();
            }
        }
    }

    processFileDialogs();

    m_gui.end();
    m_gui.draw();

    if (m_sceneEasyCam) {
        m_sceneViewport.update(*m_sceneEasyCam);
        const bool wantsInput = m_sceneViewport.isHovered() && !isGizmoActive();
        if (wantsInput)
            m_sceneEasyCam->enableMouseInput();
        else
            m_sceneEasyCam->disableMouseInput();
    }
}

void Runtime::renderMainMenuBar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 8));
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();

        if (ImGui::BeginMenu(m_appName.c_str())) {
            if (m_editMode) {
                if (ImGui::MenuItem("Hide Edit Mode", "Ctrl/Cmd+E")) {
                    setEditMode(false);
                }
            } else {
                if (ImGui::MenuItem("Show Edit Mode", "Ctrl/Cmd+E")) {
                    setEditMode(true);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...")) {
                setWindowVisible("Preferences", true);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Dark", nullptr, m_theme == Theme::Dark))
                    setTheme(Theme::Dark);
                if (ImGui::MenuItem("Light", nullptr, m_theme == Theme::Light))
                    setTheme(Theme::Light);
                if (ImGui::MenuItem("Classic", nullptr, m_theme == Theme::Classic))
                    setTheme(Theme::Classic);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("UI Scale")) {
                float scale = m_uiScale;
                if (ImGui::SliderFloat("##scale", &scale, 0.5f, 3.0f, "%.2f x")) {
                    setUIScale(scale);
                }
                ImGui::Separator();
                struct {
                    const char* label;
                    float       value;
                } presets[] = {
                    {"1.0x  (native)", 1.0f},
                    {"1.25x (mid HiDPI)", 1.25f},
                    {"1.5x  (HiDPI)", 1.5f},
                    {"2.0x  (4K)", 2.0f},
                    {"2.5x  (4K large)", 2.5f},
                };
                for (auto& p : presets) {
                    bool sel = std::fabs(m_uiScale - p.value) < 0.01f;
                    if (ImGui::MenuItem(p.label, nullptr, sel))
                        setUIScale(p.value);
                }
                ImGui::Separator();
                float       autoScale = detectUIScale();
                std::string autoLabel =
                    "Auto-detect (" + ofToString(autoScale, 2) + "x)";
                if (ImGui::MenuItem(autoLabel.c_str()))
                    setUIScale(autoScale);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                ofExit();
            }
            ImGui::EndMenu();
        }

        for (auto& [groupName, cb] : m_menuGroups) {
            if (ImGui::BeginMenu(groupName.c_str())) {
                ImGui::PushID(groupName.c_str());
                cb();
                ImGui::PopID();
                ImGui::EndMenu();
            }
        }

        for (size_t i = 0; i < m_menuBarRawCallbacks.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            m_menuBarRawCallbacks[i]();
            ImGui::PopID();
        }

        if (ImGui::BeginMenu("View")) {
            for (auto& window : m_windows) {
                if (window.menuGroup == "View") {
                    bool prev = window.visible;
                    ImGui::MenuItem(window.name.c_str(), nullptr, &window.visible);
                    if (window.visible != prev)
                        saveAppPrefs();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New Scene View")) {
                addViewportWindow();
            }
            ImGui::Separator();
            ImGui::MenuItem("Rulers", "F2", &m_showRulers);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_defaultLayoutBuilt = false;
                m_layoutResetPending = true;
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
    if (window.id.empty())
        window.id =
            "ofxkit.window." + detail::makeImGuiIdFromLabel(window.name);

    for (auto& existing : m_windows) {
        if (existing.id == window.id) {
            ofLogWarning("ofxKit") << "Window id '" << window.id << "' is already registered.";
            return &existing;
        }
    }

    if (auto* existing = findWindow(window.name)) {
        ofLogWarning("ofxKit") << "Window '" << window.name << "' is already registered.";
        return existing;
    }

    auto it = m_savedWindowVisibility.find(window.id);
    if (it == m_savedWindowVisibility.end())
        it = m_savedWindowVisibility.find(window.name);
    if (it != m_savedWindowVisibility.end())
        window.visible = it->second;

    m_windows.push_back(std::move(window));
    return &m_windows.back();
}

Runtime::RuntimeWindow* Runtime::findWindow(const std::string& name)
{
    for (auto& window : m_windows) {
        if (window.name == name)
            return &window;
    }
    return nullptr;
}

const Runtime::RuntimeWindow* Runtime::findWindow(const std::string& name) const
{
    for (const auto& window : m_windows) {
        if (window.name == name)
            return &window;
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

void Runtime::addDefaultLayoutLeftDock(std::string imguiWindowTitle)
{
    if (imguiWindowTitle.empty())
        return;
    for (const auto& existing : m_defaultLayoutExtraLeftDocks) {
        if (existing == imguiWindowTitle)
            return;
    }
    m_defaultLayoutExtraLeftDocks.push_back(std::move(imguiWindowTitle));
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

void Runtime::setImGuiIniPath(std::string path)
{
    m_imguiIniPath = std::move(path);
    if (ImGui::GetCurrentContext()) {
        if (!m_imguiIniPath.empty())
            detail::createParentDirectoryIfNeeded(m_imguiIniPath);
        ImGui::GetIO().IniFilename =
            m_imguiIniPath.empty() ? nullptr : m_imguiIniPath.c_str();
        if (!m_imguiIniPath.empty()
            && of::filesystem::exists(of::filesystem::path(m_imguiIniPath))) {
            ImGui::LoadIniSettingsFromDisk(m_imguiIniPath.c_str());
        }
    }
}

void Runtime::setAppName(std::string name)
{
    if (!name.empty())
        m_appName = std::move(name);
}

} // namespace ofkitty
