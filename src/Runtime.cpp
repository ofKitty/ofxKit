#include "Runtime.h"
#include "panels/CodeEditorPanel.h"
#include "panels/PathEditorPanel.h"
#include "ProgressWindow.h"
#include "Runtime_private.h"

#include "ofxEnTTKit_all.h"
#include "ofJson.h"
#include "imgui_internal.h"   // DockBuilder API

#ifndef TARGET_OPENGLES
    #include "ofAppGLFWWindow.h"   // ofGetWindowPtr() downcast target
    #define GLFW_INCLUDE_NONE      // OF already provides the GL loader
    #include <GLFW/glfw3.h>        // GLFWdropfun + glfwSetDropCallback/glfwGetCursorPos
#endif

#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>

namespace ofkitty {

// File-scope static for the View-menu extra callback.
// Declared here (inside the namespace, before first use) so MenuBarCallback
// resolves without full qualification and the symbol is visible to all
// functions in this translation unit.
static Runtime::MenuBarCallback s_viewMenuExtra;

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
// OS file drop — chained GLFW drop callback
// ============================================================================
// ofxImGui's imgui_impl_glfw replaces OF's GLFW callbacks during gui.setup(),
// which kills the drop callback that drives ofApp::dragEvent. We install our
// own drop callback after gui.setup() and chain whatever was there before.

#ifndef TARGET_OPENGLES
namespace {
GLFWdropfun s_prevDropCallback = nullptr;

void kittyGlfwDropCallback(GLFWwindow* win, int count, const char** paths)
{
    // Intentionally NOT forwarding to s_prevDropCallback: the previous callback
    // is typically OF's own drop_cb (imgui_impl_glfw installs none), which would
    // re-dispatch ofApp::dragEvent and duplicate the drop. Apps route drops via
    // Runtime::setFileDropHandler() instead.
    (void)s_prevDropCallback;

    if (count <= 0 || paths == nullptr)
        return;

    std::vector<std::filesystem::path> files;
    files.reserve(count);
    for (int i = 0; i < count; ++i)
        if (paths[i]) files.emplace_back(paths[i]);

    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    Runtime::instance().dispatchFileDrop(files, glm::vec2((float)mx, (float)my));
}
} // namespace
#endif

void Runtime::setFileDropHandler(
    std::function<void(const std::vector<std::filesystem::path>&, glm::vec2)> cb)
{
    m_fileDropHandler = std::move(cb);
}

void Runtime::dispatchFileDrop(const std::vector<std::filesystem::path>& files, glm::vec2 pos)
{
    if (m_fileDropHandler)
        m_fileDropHandler(files, pos);
}

void Runtime::installFileDropCallback()
{
#ifndef TARGET_OPENGLES
    auto* glfwWin = dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());
    if (!glfwWin) return;
    GLFWwindow* w = glfwWin->getGLFWWindow();
    if (!w) return;
    // Install ours last so it wins, but keep the previous one in the chain.
    s_prevDropCallback = glfwSetDropCallback(w, &kittyGlfwDropCallback);
#endif
}

// ============================================================================
// Selection
// ============================================================================

entt::entity Runtime::selected() const
{
    return m_selected;
}

void Runtime::select(entt::entity e)
{
    if (e != entt::null && !registry().valid(e))
        e = entt::null;

    m_selected = e;
    ecs::selectEntity(registry(), e);
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

    // F2 to toggle rulers (scene editor only)
    if (m_sceneEditorFeaturesEnabled) {
        m_shortcuts.bind(OF_KEY_F2, 0, "Toggle Rulers",
                         [this] { toggleRulers(); });
    }

    ImGuiConfigFlags imguiFlags = ImGuiConfigFlags_DockingEnable;
#ifndef TARGET_OPENGLES
    imguiFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
    m_gui.setup(nullptr, false, imguiFlags, true);

    // Load Input Sans + Font Awesome for UI, JetBrains Mono for the code editor.
    // Must happen right after gui.setup() before the first frame renders.
    ImFont* codeEditorFont = nullptr;
    if (ImFont* font = ImFonts::LoadDefaultFonts(ImGui::GetIO().Fonts, 14.0f)) {
        m_gui.setDefaultFont(font);
        codeEditorFont = ImFonts::LoadCodeEditorFont(ImGui::GetIO().Fonts, 14.0f);
    }
    m_gui.rebuildFontsTexture();

    // Register event filters that block OF mouse/keyboard events from reaching
    // the app while ImGui has claimed them.  Must happen after m_gui.setup()
    // so the ImGui context exists when the first WantCaptureMouse check fires.
    m_eventHelper.setup();

    // ofxImGui just replaced OF's GLFW callbacks (including the drop callback
    // that drives ofApp::dragEvent). Re-install a chained drop callback so file
    // drops reach apps that registered via setFileDropHandler().
    installFileDropCallback();

    // Keep docking/layout state in a stable app data location. ImGui's default
    // "imgui.ini" is relative to the process working directory, which can
    // change depending on how the example is launched.
    if (m_imguiIniPath.empty()) {
        m_imguiIniPath = dataPath("imgui.ini");
    }
    detail::createParentDirectoryIfNeeded(m_imguiIniPath);
    ImGui::GetIO().IniFilename = m_imguiIniPath.c_str();
    if (of::filesystem::exists(of::filesystem::path(m_imguiIniPath))) {
        ImGui::LoadIniSettingsFromDisk(m_imguiIniPath.c_str());
    }

    if (!m_codeEditor) {
        m_codeEditor = std::make_unique<CodeEditorPanel>();
        m_codeEditor->setup();
        if (codeEditorFont) {
            m_codeEditorFont = codeEditorFont;
            m_codeEditor->setFont(codeEditorFont);
        }
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

    if (!m_uiScaleSet)
        loadUIScalePref();
    if (!m_uiScaleSet)
        m_uiScale = detectUIScale();

    if (!m_themeSet)
        loadThemePref();

    ImTheme::SetUIScale(m_uiScale);
    applyTheme();
    applyUIScale();

    for (auto& hook : m_postSetupHooks)
        hook(m_gui);

    // Built-in shortcut — defaults + optional merge from data/shortcuts.json
    // (or `<dataSubdir()>/shortcuts.json`).
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
        "Toggle All UI (windows + menu bar + status bar)",
        [this] { toggleAllUI(); });

    m_shortcuts.registerAction(
        "ofkitty.toggle_edit_tab",
        OF_KEY_TAB,
        0,
        "Toggle Edit-mode windows only",
        [this] {
            // Block only when a text-input widget has focus so Tab still works
            // for toggling windows even when ImGui panels are visible.
            if (!ImGui::GetIO().WantTextInput)
                toggleEditMode();
        });

    // Gizmo operation shortcuts (scene editor only)
    if (m_sceneEditorFeaturesEnabled) {
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
    }

    m_shortcuts.loadBindingsFromFile(ShortcutManager::defaultBindingsPath());
    m_shortcuts.setAutoSaveEnabled(true);
}

void Runtime::onUpdate(ofEventArgs&)
{
    if (m_hueShift >= 0.f && ImGui::GetCurrentContext())
        applyTheme();
}

void Runtime::onExit(ofEventArgs&)
{
    // Ensure imgui.ini and our prefs are written before the process ends.
    // ImGui's auto-save has a 5-second delay so the last docking state can
    // otherwise be lost if the window is closed quickly.
    saveFileDialogPrefs();
    saveAppPrefs();
}

void Runtime::toggleEditMode()
{
    const int f = ofGetFrameNum();
    if (m_toggleEditLastFrame == f) {
        return;
    }
    m_toggleEditLastFrame = f;

    // Tab always reveals chrome so the menu bar and status bar stay visible.
    // This also ensures Tab works correctly when Ctrl+E previously hid everything:
    // pressing Tab brings back the full UI (chrome + windows).
    m_hideChrome = false;
    m_editMode   = !m_editMode;
    ofLogNotice("ofxKit") << "Edit mode (windows) " << (m_editMode ? "ON" : "OFF");
}

void Runtime::toggleAllUI()
{
    const int f = ofGetFrameNum();
    if (m_toggleEditLastFrame == f) {
        return;
    }
    m_toggleEditLastFrame = f;

    if (m_prefs.hideAllUI) {
        // "Hide entire UI" mode: toggle chrome and windows together.
        // Use m_hideChrome as the primary toggle so this is idempotent even if
        // Tab was pressed beforehand (m_editMode may already be false).
        m_hideChrome = !m_hideChrome;
        m_editMode   = !m_hideChrome;
        ofLogNotice("ofxKit") << "All UI " << (!m_hideChrome ? "shown" : "hidden");
    } else {
        // "Hide windows only" mode: same as Tab — chrome always stays visible.
        m_hideChrome = false;
        m_editMode   = !m_editMode;
        ofLogNotice("ofxKit") << "Edit mode (windows) " << (m_editMode ? "ON" : "OFF");
    }
}

void Runtime::disableBuiltInWindows()
{
    m_autoRegisterBuiltIns = false;
    m_requestedBuiltInWindows.clear();
}

void Runtime::enableBuiltInWindow(const std::string& nameOrId)
{
    m_autoRegisterBuiltIns = false;
    m_requestedBuiltInWindows.insert(nameOrId);
}

void Runtime::enableBuiltInWindows()
{
    enableBuiltInWindow("Scene");
    enableBuiltInWindow("Properties");
}

void Runtime::enableAllBuiltInWindows()
{
    m_autoRegisterBuiltIns = true;
    m_requestedBuiltInWindows.clear();
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

    // Menu bar — hidden when m_hideChrome is set (toggled by Ctrl/Cmd+E via
    // toggleAllUI).  Tab clears m_hideChrome so chrome is always visible after Tab.
    const bool showChrome = !m_hideChrome;
    if (showChrome)
        renderMainMenuBar();

    if (m_editMode)
        ImGuizmo::BeginFrame();

    // Status bar before dockspace so the main viewport work area excludes it.
    const float statusBarHeight = showChrome ? ImGui::GetFrameHeight() : 0.f;
    if (showChrome)
        drawStatusBar();

    // The dockspace is created unconditionally on every frame so that ImGui's
    // saved-ini dock state (node IDs, splits, window positions) is always
    // backed by a live dockspace.  Without this, ImGui loads the dock nodes
    // from the .ini file but never finds the host window, which can leave
    // orphaned nodes in an inconsistent state and segfault on early frames.
    //
    // In non-edit mode the central node is forced to PassthruCentralNode so
    // the OF background renders through cleanly; in edit mode the app's own
    // m_passthruCentralNode preference is honoured.
    {
        ImGuiDockNodeFlags dsFlags = ImGuiDockNodeFlags_None;
        if (!m_editMode || m_passthruCentralNode)
            dsFlags |= ImGuiDockNodeFlags_PassthruCentralNode
                    |  ImGuiDockNodeFlags_NoDockingOverCentralNode;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        // Dock host: from below main menu (WorkPos) to above status bar (explicit).
        const ImVec2 dockPos(vp->WorkPos.x, vp->WorkPos.y);
        const float dockBottom =
            vp->Pos.y + vp->Size.y - (showChrome ? statusBarHeight : 0.f);
        ImVec2 dockSize(vp->WorkSize.x, std::max(0.f, dockBottom - dockPos.y));

        ImGui::SetNextWindowPos(dockPos);
        ImGui::SetNextWindowSize(dockSize);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
            | ImGuiWindowFlags_NoDocking;
        if (!m_editMode || m_passthruCentralNode)
            hostFlags |= ImGuiWindowFlags_NoBackground;
        ImGui::Begin("##ofxkit.DockHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);
        ImGuiID dockId = ImGui::DockSpace(ImGui::GetID("ofxkit.DockSpace"), ImVec2(0.f, 0.f), dsFlags);

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
            // Only suppress the tab bar when no app windows are seeded into the
            // central node. When a window like Preview is docked there it needs
            // its own title/menu bar to be visible.
            if (m_defaultLayoutExtraCenterDocks.empty())
                cn->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            if (!m_editMode || m_passthruCentralNode)
                cn->LocalFlags |= ImGuiDockNodeFlags_PassthruCentralNode
                               |  ImGuiDockNodeFlags_NoDockingOverCentralNode;
        }

        if (m_editMode) {
            if (m_sceneEditorFeaturesEnabled && m_showRulers)
                drawRulers();
            if (m_sceneEditorFeaturesEnabled)
                drawGizmoOverlay();
        }
        ImGui::End(); // ##ofxkit.DockHost
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

    // Main-scene 2D overlay (handles, zone grids etc. drawn over the OF scene).
    drawMainView2DOverlay();

    m_gui.end();
    m_gui.draw();

    if (m_sceneCamera) {
        const ofRectangle sceneRect =
            m_editMode
                ? m_gui.getMainWindowViewportRect(true, true, true)
                : ofRectangle(0.f, 0.f, ofGetWidth(), ofGetHeight());

        m_sceneViewport.update(*m_sceneCamera, sceneRect);

        const bool wantsInput = m_sceneViewport.isHovered() && !isGizmoActive();
        if (m_sceneEasyCam) {
            if (wantsInput)
                m_sceneEasyCam->enableMouseInput();
            else
                m_sceneEasyCam->disableMouseInput();
        }

        if (m_editMode && wantsInput && detail::isClickWithoutDrag()) {
            const ImGuiIO& io = ImGui::GetIO();
            pickAtScreen(*m_sceneCamera, {io.MousePos.x, io.MousePos.y}, sceneRect);
        }
    }
}

void Runtime::renderGizmoMenu()
{
    if (!ImGui::BeginMenu("Edit"))
        return;

    if (ImGui::MenuItem("Translate", "W", m_gizmoOp == GizmoOperation::Translate))
        m_gizmoOp = GizmoOperation::Translate;
    if (ImGui::MenuItem("Rotate", "E", m_gizmoOp == GizmoOperation::Rotate))
        m_gizmoOp = GizmoOperation::Rotate;
    if (ImGui::MenuItem("Scale", "R", m_gizmoOp == GizmoOperation::Scale))
        m_gizmoOp = GizmoOperation::Scale;
    if (ImGui::MenuItem("Universal", nullptr, m_gizmoOp == GizmoOperation::Universal))
        m_gizmoOp = GizmoOperation::Universal;

    ImGui::Separator();

    if (ImGui::MenuItem("World Space", nullptr, m_gizmoMode == GizmoMode::World))
        m_gizmoMode = GizmoMode::World;
    if (ImGui::MenuItem("Local Space", nullptr, m_gizmoMode == GizmoMode::Local))
        m_gizmoMode = GizmoMode::Local;

    ImGui::Separator();

    if (ImGui::MenuItem("Toggle World / Local", "X"))
        m_gizmoMode = (m_gizmoMode == GizmoMode::World) ? GizmoMode::Local
                                                          : GizmoMode::World;

    ImGui::EndMenu();
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
                // Quick-pick subset of ImTheme built-ins. The full selector
                // (all 17 built-ins + addon-registered customs) lives in
                // Preferences > Appearance via ImTheme::ShowSelector.
                struct QuickPick { ImTheme::Theme_ id; const char* label; };
                static constexpr QuickPick picks[] = {
                    {ImTheme::Theme_DarculaDarker,   "Darcula Darker"},
                    {ImTheme::Theme_Darcula,         "Darcula"},
                    {ImTheme::Theme_ImGuiColorsDark, "ImGui Dark"},
                    {ImTheme::Theme_MaterialFlat,    "Material Flat"},
                    {ImTheme::Theme_LightRounded,    "Light Rounded"},
                    {ImTheme::Theme_ImGuiColorsLight,"ImGui Light"},
                };
                for (const auto& p : picks) {
                    const char* id = ImTheme::Name(p.id);
                    if (ImGui::MenuItem(p.label, nullptr, m_themeId == id))
                        setTheme(id);
                }
                ImGui::Separator();
                ImGui::MenuItem("More themes in Preferences \xe2\x80\xa6", nullptr, false, false);
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

        // Main-scene 2D view menu/toolbar (zoom controls, overlay toggles, …).
        if (m_mainView2D && m_mainView2D->menuBarDraw) {
            ImGui::PushID("ofkitty.mainview2d.menubar");
            m_mainView2D->menuBarDraw();
            ImGui::PopID();
        }

        if (m_editMode && m_sceneEditorFeaturesEnabled)
            renderGizmoMenu();

        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Windows")) {
                for (auto& window : m_windows) {
                    if (window.menuGroup == "View") {
                        bool prev = window.visible;
                        ImGui::MenuItem(window.name.c_str(), nullptr, &window.visible);
                        if (window.visible != prev)
                            saveAppPrefs();
                    }
                }
                ImGui::EndMenu();
            }
            if (s_viewMenuExtra) {
                ImGui::PushID("ofkitty.viewMenuExtra");
                s_viewMenuExtra();
                ImGui::PopID();
            }
            if (m_editMode && m_sceneEditorFeaturesEnabled) {
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

void Runtime::addDefaultLayoutRightDock(std::string imguiWindowTitle)
{
    if (imguiWindowTitle.empty())
        return;
    for (const auto& existing : m_defaultLayoutExtraRightDocks) {
        if (existing == imguiWindowTitle)
            return;
    }
    m_defaultLayoutExtraRightDocks.push_back(std::move(imguiWindowTitle));
}

void Runtime::addDefaultLayoutCenterDock(std::string imguiWindowTitle)
{
    if (imguiWindowTitle.empty())
        return;
    for (const auto& existing : m_defaultLayoutExtraCenterDocks) {
        if (existing == imguiWindowTitle)
            return;
    }
    m_defaultLayoutExtraCenterDocks.push_back(std::move(imguiWindowTitle));
}

void Runtime::addDefaultLayoutBottomDock(std::string imguiWindowTitle)
{
    if (imguiWindowTitle.empty())
        return;
    for (const auto& existing : m_defaultLayoutExtraBottomDocks) {
        if (existing == imguiWindowTitle)
            return;
    }
    m_defaultLayoutExtraBottomDocks.push_back(std::move(imguiWindowTitle));
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

void Runtime::setViewMenuExtra(MenuBarCallback cb)
{
    s_viewMenuExtra = std::move(cb);
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

void Runtime::setDataSubdir(const std::string& subdir)
{
    m_dataSubdir = subdir;
    while (!m_dataSubdir.empty()
           && (m_dataSubdir.front() == '/' || m_dataSubdir.front() == '\\'))
        m_dataSubdir.erase(0, 1);
    while (!m_dataSubdir.empty()
           && (m_dataSubdir.back() == '/' || m_dataSubdir.back() == '\\'))
        m_dataSubdir.pop_back();
}

std::string Runtime::dataPath(const std::string& filename) const
{
    if (m_dataSubdir.empty())
        return ofToDataPath(filename, true);
    return ofToDataPath(m_dataSubdir + "/" + filename, true);
}

void Runtime::setAppName(std::string name)
{
    if (!name.empty())
        m_appName = std::move(name);
}

// ============================================================================
// MainView2D — pan/zoom for the OF main window (ofSpace)
// ============================================================================

Runtime::MainView2D* Runtime::setMainView2D(glm::vec2 contentSize, std::string contentUnit)
{
    if (m_mainView2D)
        unregisterMainViewListeners();

    m_mainView2D = std::make_unique<MainView2D>();
    m_mainView2D->view2D.contentSize = contentSize;
    m_mainView2D->contentUnit        = std::move(contentUnit);
    registerMainViewListeners();
    return m_mainView2D.get();
}

void Runtime::clearMainView2D()
{
    if (!m_mainView2D) return;
    unregisterMainViewListeners();
    m_mainView2D.reset();
}

void Runtime::registerMainViewListeners()
{
    ofAddListener(ofEvents().draw,         this, &Runtime::onMainViewPreDraw,
                  OF_EVENT_ORDER_BEFORE_APP);
    ofAddListener(ofEvents().mouseScrolled, this, &Runtime::onMainViewMouseScrolled,
                  OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().mouseDragged,  this, &Runtime::onMainViewMouseDragged,
                  OF_EVENT_ORDER_AFTER_APP);
    ofAddListener(ofEvents().mousePressed,  this, &Runtime::onMainViewMousePressed,
                  OF_EVENT_ORDER_AFTER_APP);
}

void Runtime::unregisterMainViewListeners()
{
    ofRemoveListener(ofEvents().draw,         this, &Runtime::onMainViewPreDraw,
                     OF_EVENT_ORDER_BEFORE_APP);
    ofRemoveListener(ofEvents().mouseScrolled, this, &Runtime::onMainViewMouseScrolled,
                     OF_EVENT_ORDER_AFTER_APP);
    ofRemoveListener(ofEvents().mouseDragged,  this, &Runtime::onMainViewMouseDragged,
                     OF_EVENT_ORDER_AFTER_APP);
    ofRemoveListener(ofEvents().mousePressed,  this, &Runtime::onMainViewMousePressed,
                     OF_EVENT_ORDER_AFTER_APP);
}

// Runs BEFORE ofApp::draw() — publish last-frame's canvas geometry so the
// app transform in draw() matches what the overlay computed in the previous frame.
// The accurate geometry (central-node bounds) is set inside drawMainView2DOverlay()
// which runs inside the ImGui frame; we just carry it forward here so draw() reads
// a valid, stable value. On the very first frame the full window is used as fallback.
void Runtime::onMainViewPreDraw(ofEventArgs&)
{
    if (!m_mainView2D) return;
    auto& v = m_mainView2D->view2D;

    // If we have a cached central-node rect from the previous ImGui frame use it;
    // otherwise fall back to the full window (single-frame approximation on startup).
    if (m_mainViewCentralW > 0.f && m_mainViewCentralH > 0.f) {
        v.canvasOrigin = { m_mainViewCentralX, m_mainViewCentralY };
        v.canvasW      = m_mainViewCentralW;
        v.canvasH      = m_mainViewCentralH;
    } else {
        v.canvasOrigin = { 0.f, 0.f };
        v.canvasW      = static_cast<float>(ofGetWidth());
        v.canvasH      = static_cast<float>(ofGetHeight());
    }
    v.hovered = m_mainViewHovered;
    v.updateDerived();
}

void Runtime::onMainViewMouseScrolled(ofMouseEventArgs& e)
{
    if (!m_mainView2D || !m_mainView2D->view2D.hovered) return;
    // e.x/e.y are window-relative OF coords — same space as View2DState.
    m_mainView2D->view2D.applyScrollZoom(
        e.scrollY, static_cast<float>(e.x), static_cast<float>(e.y));
}

void Runtime::onMainViewMouseDragged(ofMouseEventArgs& e)
{
    if (!m_mainView2D || !m_mainView2D->view2D.hovered) return;
    const bool isPan =
        (m_mainView2D->panOnMiddle && e.button == OF_MOUSE_BUTTON_MIDDLE)
     || (m_mainView2D->panOnAltLMB && e.button == OF_MOUSE_BUTTON_LEFT
         && ofGetKeyPressed(OF_KEY_ALT));
    // Deltas are offset-invariant so no desktop-global correction needed.
    if (isPan)
        m_mainView2D->view2D.applyPanDelta(
            static_cast<float>(e.x) - m_mainViewPrevMouseX,
            static_cast<float>(e.y) - m_mainViewPrevMouseY);
    m_mainViewPrevMouseX = static_cast<float>(e.x);
    m_mainViewPrevMouseY = static_cast<float>(e.y);
}

void Runtime::onMainViewMousePressed(ofMouseEventArgs& e)
{
    m_mainViewPrevMouseX = static_cast<float>(e.x);
    m_mainViewPrevMouseY = static_cast<float>(e.y);

    // Manual double-click detection — ofCoreEvents has no mouseDoubleClicked.
    // Double-click LMB (no Alt) over the scene fits content to the window.
    const float now = ofGetElapsedTimef();
    const bool sameButton = (e.button == OF_MOUSE_BUTTON_LEFT);
    const bool quick = (now - m_mainViewLastClickTime) < 0.3f;
    if (sameButton && quick && m_mainView2D
        && m_mainView2D->view2D.hovered && !ofGetKeyPressed(OF_KEY_ALT)) {
        m_mainView2D->view2D.fitToCanvas();
        m_mainViewLastClickTime = 0.f;  // consume so a triple-click doesn't re-fire
    } else if (sameButton) {
        m_mainViewLastClickTime = now;
    }
}

// Called from onDraw() inside the ImGui frame.
void Runtime::drawMainView2DOverlay()
{
    if (!m_mainView2D) return;

    const ImGuiViewport* iv = ImGui::GetMainViewport();

    // ---- Resolve the actual passthru central-node bounds --------------------
    // The central node is what's left after all docked side panels take their
    // space. Its rect is in ImGui desktop-global coords (because ViewportsEnable
    // is on). We convert to window-relative so canvasOrigin/W/H match the OF
    // drawing coordinate system (which uses window-relative pixels).
    //
    // With ViewportsEnable, iv->Pos is the window's desktop position. Subtracting
    // it converts desktop-global ImGui coords → window-relative OF coords.

    ImVec2 centralPos  = iv->WorkPos;   // fallback: full work area
    ImVec2 centralSize = iv->WorkSize;

    ImGuiID dockId = ImGui::GetID("ofxkit.DockSpace");
    if (ImGuiDockNode* cn = ImGui::DockBuilderGetCentralNode(dockId)) {
        if (cn->Pos.x != 0.f || cn->Pos.y != 0.f || cn->Size.x > 0.f) {
            centralPos  = cn->Pos;
            centralSize = cn->Size;
        }
    }

    // Window-relative canvas geometry (OF draw() uses window-relative coords).
    const float cx = centralPos.x  - iv->Pos.x;
    const float cy = centralPos.y  - iv->Pos.y;
    const float cw = centralSize.x;
    const float ch = centralSize.y;

    // Cache for onMainViewPreDraw (runs before the next ImGui frame).
    m_mainViewCentralX = cx;
    m_mainViewCentralY = cy;
    m_mainViewCentralW = cw;
    m_mainViewCentralH = ch;

    // Update this frame's view geometry now so the overlay callback reads the
    // same state that draw() will use next frame (one-frame lag is acceptable).
    auto& v   = m_mainView2D->view2D;
    v.canvasOrigin = { cx, cy };
    v.canvasW      = cw;
    v.canvasH      = ch;

    // Hover: mouse is over the central area and ImGui isn't consuming it.
    const ImGuiIO& io = ImGui::GetIO();
    const bool mouseInCentral =
        io.MousePos.x >= centralPos.x && io.MousePos.x < centralPos.x + centralSize.x &&
        io.MousePos.y >= centralPos.y && io.MousePos.y < centralPos.y + centralSize.y;
    const bool imguiOwnsMouse =
        io.WantCaptureMouse
        || ImGui::IsAnyItemActive()
        || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    m_mainViewHovered = mouseInCentral && !imguiOwnsMouse;
    v.hovered = m_mainViewHovered;

    v.updateDerived();

    if (!m_mainView2D->overlayDraw) return;

    // ---- Draw overlay -------------------------------------------------------
    // The overlay window covers the full work area (transparent, no-input) so
    // it sits above the OF scene but below all docked panels.
    // DrawList vertices are in desktop-global coords (ViewportsEnable). Callers
    // that draw to ImDrawList must add iv->Pos to window-relative screen coords
    // from toScreen() to convert to desktop-global. This offset is exposed via
    // MainView2D::imguiScreenOffset so overlayDraw callbacks can use it.
    m_mainView2D->imguiScreenOffset = { iv->Pos.x, iv->Pos.y };

    ImGui::SetNextWindowPos(iv->WorkPos);
    ImGui::SetNextWindowSize(iv->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##ofkitty_main_view2d_overlay", nullptr, flags)) {
        m_mainView2D->overlayDraw(*m_mainView2D);
    }
    ImGui::End();
}

} // namespace ofkitty
