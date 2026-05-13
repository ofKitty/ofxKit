#include "Runtime.h"
#include "ProgressWindow.h"

#include <ofxEnTTKit/src/component_editor_registration.h>
#include <ofxEnTTInspector/src/ofxEnTTInspector.h>
#include "ofJson.h"
#include "imgui_internal.h"   // DockBuilder API

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>

// GLFW is the desktop OF window backend and is what ofxImGui uses too.
// Skip on mobile / RPi targets where GLFW isn't present.
#if !defined(TARGET_OPENGLES) && !defined(TARGET_RASPBERRY_PI)
#  define OFXKIT_HAS_GLFW 1
#  include <GLFW/glfw3.h>
#endif

namespace ofkitty {

namespace {

std::string makeImGuiIdFromLabel(const std::string& label)
{
    std::string id = label;
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
        if (std::isalnum(c)) return static_cast<char>(std::tolower(c));
        return '_';
    });
    while (id.find("__") != std::string::npos)
        id.replace(id.find("__"), 2, "_");
    if (!id.empty() && id.front() == '_') id.erase(id.begin());
    if (!id.empty() && id.back() == '_') id.pop_back();
    return id.empty() ? "window" : id;
}

void createParentDirectoryIfNeeded(const std::string& path)
{
    const auto parent = of::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        of::filesystem::create_directories(parent);
    }
}

} // anonymous namespace

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
    createParentDirectoryIfNeeded(m_imguiIniPath);
    ImGui::GetIO().IniFilename = m_imguiIniPath.c_str();
    if (of::filesystem::exists(of::filesystem::path(m_imguiIniPath))) {
        ImGui::LoadIniSettingsFromDisk(m_imguiIniPath.c_str());
    }

    // Code editor defaults
    m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Glsl);
    m_textEditor.SetPalette(TextEditor::PaletteId::Dark);
    m_textEditor.SetShowLineNumbersEnabled(true);

    // Apply the persisted theme through ofxImGuiStyle, then capture that
    // unscaled baseline before ofxKit layers editor-scale policy on top.
    if (!m_themeSet) loadThemePref();
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
        "Gizmo: Translate", [this] { if (m_editMode) m_gizmoOp = GizmoOperation::Translate; });
    m_shortcuts.registerAction("ofkitty.gizmo_rotate",    'e', 0,
        "Gizmo: Rotate",    [this] { if (m_editMode) m_gizmoOp = GizmoOperation::Rotate; });
    m_shortcuts.registerAction("ofkitty.gizmo_scale",     'r', 0,
        "Gizmo: Scale",     [this] { if (m_editMode) m_gizmoOp = GizmoOperation::Scale; });
    m_shortcuts.registerAction("ofkitty.gizmo_mode_toggle", 'x', 0,
        "Gizmo: Toggle World/Local",
        [this] {
            if (m_editMode)
                m_gizmoMode = (m_gizmoMode == GizmoMode::World) ? GizmoMode::Local : GizmoMode::World;
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
    m_shortcuts.dispatch(e);
}

// ============================================================================
// Overlay
// ============================================================================

void Runtime::drawOverlay()
{
    m_gui.begin();

    // Ctrl/Cmd+E is handled only by ShortcutManager (`ofkitty.toggle_edit` in postSetup).
    // Do not also read ImGui keys here: OF dispatches the key on one frame and ImGui can
    // report ImGuiKey_E on the next, which double-toggles and makes Edit mode flicker OFF/ON.

    if (m_editMode) {
        ImGuizmo::BeginFrame();
        renderMainMenuBar();
    }

    // Always reserve the bottom status strip (progress, optional hints, FPS when visible, …).
    drawStatusBar();

    if (m_editMode) {

        // Status bar must claim its work-area slice BEFORE DockSpaceOverViewport
        // runs — otherwise the dockspace fills over the bar and PassthruCentralNode
        // no longer leaves a visible gap at the bottom.
        // (drawStatusBar is invoked just above.)

        // Use the standard DockSpaceOverViewport so PassthruCentralNode works
        // correctly (transparent + input pass-through in the empty central area).
        // NoDockingOverCentralNode prevents windows from being accidentally
        // dropped onto the central transparent zone, which would eliminate
        // the passthrough and cover the OF scene permanently.
        ImGuiID dockId = ImGui::DockSpaceOverViewport(
            0, ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_PassthruCentralNode |
            ImGuiDockNodeFlags_NoDockingOverCentralNode);

        // Build the default panel arrangement on first run (no ini file) or
        // after the user explicitly chooses Reset Layout.
        // When an ini file is present ImGui already restored the saved layout
        // from disk — calling buildDefaultDockLayout would destroy that work.
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

        // Re-apply central-node flags every frame: ini reloads and manual
        // docking can drop local flags.
        if (ImGuiDockNode* cn = ImGui::DockBuilderGetCentralNode(dockId)) {
            cn->LocalFlags |= ImGuiDockNodeFlags_PassthruCentralNode |
                              ImGuiDockNodeFlags_NoDockingOverCentralNode |
                              ImGuiDockNodeFlags_NoTabBar;
        }

        if (m_showRulers) drawRulers();

        // Main-scene gizmo — uses GetForegroundDrawList() so call order
        // relative to panel windows doesn't matter for rendering.
        drawGizmoOverlay();
    }

    // Panel windows — drawn AFTER the gizmo overlay so they appear on top.
    for (auto& window : m_windows) {
        if (window.visible && window.draw) {
            if (!window.editModeOnly || m_editMode) {
                bool prev = window.visible;
                ImGui::PushID(window.id.c_str());
                window.draw(window.visible);
                ImGui::PopID();
                // The draw callback passes visible by reference; ImGui sets it
                // to false when the user clicks the ✕ close button.
                if (window.visible != prev) saveAppPrefs();
            }
        }
    }

    // Process open file dialogs every frame (must be inside gui.begin/end).
    processFileDialogs();

    m_gui.end();
    // Manual draw mode requires explicit draw() to render the frame.
    // Without this, Gui::end() only calls ImGui::EndFrame() and nothing
    // is ever submitted to OpenGL — invisible menu bar / windows.
    m_gui.draw();

    // Update the scene viewport tracker and auto-manage ofEasyCam input.
    // Runs AFTER m_gui.end() so that ImGui's WantCaptureMouse flag reflects
    // the finished frame.
    if (m_sceneEasyCam) {
        m_sceneViewport.update(*m_sceneEasyCam);
        const bool wantsInput = m_sceneViewport.isHovered() && !isGizmoActive();
        if (wantsInput) m_sceneEasyCam->enableMouseInput();
        else            m_sceneEasyCam->disableMouseInput();
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
                if (ImGui::MenuItem("Dark",    nullptr, m_theme == Theme::Dark))    setTheme(Theme::Dark);
                if (ImGui::MenuItem("Light",   nullptr, m_theme == Theme::Light))   setTheme(Theme::Light);
                if (ImGui::MenuItem("Classic", nullptr, m_theme == Theme::Classic)) setTheme(Theme::Classic);
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
                ImGui::PushID(groupName.c_str());
                cb();
                ImGui::PopID();
                ImGui::EndMenu();
            }
        }

        // Raw callbacks — each handles its own BeginMenu/EndMenu calls
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
                    if (window.visible != prev) saveAppPrefs();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New Scene View")) {
                addViewportWindow(); // auto-names "Scene View N"
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
        window.id = "ofxkit.window." + makeImGuiIdFromLabel(window.name);

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

    // Restore visibility state that was loaded from disk before this window
    // was registered (handles both built-ins and late-registered addon windows).
    auto it = m_savedWindowVisibility.find(window.id);
    if (it == m_savedWindowVisibility.end())
        it = m_savedWindowVisibility.find(window.name); // legacy visible-name key
    if (it != m_savedWindowVisibility.end())
        window.visible = it->second;

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

void Runtime::addDefaultLayoutLeftDock(std::string imguiWindowTitle)
{
    if (imguiWindowTitle.empty()) return;
    for (const auto& existing : m_defaultLayoutExtraLeftDocks) {
        if (existing == imguiWindowTitle)
            return;
    }
    m_defaultLayoutExtraLeftDocks.push_back(std::move(imguiWindowTitle));
}

void Runtime::registerBuiltInWindows()
{
    if (m_builtInWindowsRegistered) return;

    // Default gizmo toolbar items (W/E/R shortcuts mirror the shortcuts above)
    registerToolbarItem({"ofkitty.gizmo_translate", ICON_FA_ARROWS_ALT, "Translate (W)", "gizmo",
        [this] { m_gizmoOp = GizmoOperation::Translate; },
        [this] { return m_gizmoOp == GizmoOperation::Translate; }});
    registerToolbarItem({"ofkitty.gizmo_rotate", ICON_FA_SYNC_ALT, "Rotate (E)", "gizmo",
        [this] { m_gizmoOp = GizmoOperation::Rotate; },
        [this] { return m_gizmoOp == GizmoOperation::Rotate; }});
    registerToolbarItem({"ofkitty.gizmo_scale", ICON_FA_EXPAND_ALT, "Scale (R)", "gizmo",
        [this] { m_gizmoOp = GizmoOperation::Scale; },
        [this] { return m_gizmoOp == GizmoOperation::Scale; }});
    registerToolbarItem({"ofkitty.gizmo_universal", ICON_FA_CROSSHAIRS, "Universal", "gizmo",
        [this] { m_gizmoOp = GizmoOperation::Universal; },
        [this] { return m_gizmoOp == GizmoOperation::Universal; }});
    registerToolbarItem({"ofkitty.gizmo_world", ICON_FA_GLOBE, "World space", "gizmo_mode",
        [this] { m_gizmoMode = GizmoMode::World; },
        [this] { return m_gizmoMode == GizmoMode::World; }});
    registerToolbarItem({"ofkitty.gizmo_local", ICON_FA_CUBE, "Local space", "gizmo_mode",
        [this] { m_gizmoMode = GizmoMode::Local; },
        [this] { return m_gizmoMode == GizmoMode::Local; }});

    registerWindow({"Toolbar",     "View", true,  true,  [this](bool& visible) { drawToolbarWindow(visible);      }, "ofxkit.window.toolbar"});
    registerWindow({"Scene",       "View", true,  true,  [this](bool& visible) { drawSceneWindow(visible);        }, "ofxkit.window.scene"});
    registerWindow({"Properties",  "View", true,  true,  [this](bool& visible) { drawPropertiesWindow(visible);   }, "ofxkit.window.properties"});
    registerWindow({"Shortcuts",   "View", false, true,  [this](bool& visible) { drawShortcutsWindow(visible);    }, "ofxkit.window.shortcuts"});
    registerWindow({"Preferences", "",     false, true,  [this](bool& visible) { drawPreferencesWindow(visible);  }, "ofxkit.window.preferences"});
    registerWindow({"Code Editor", "View", false, true,  [this](bool& visible) { drawCodeEditorWindow(visible);   }, "ofxkit.window.code_editor"});
    registerWindow({"Path Editor", "View", false, true,  [this](bool& visible) { drawPathEditorWindow(visible);   }, "ofxkit.window.path_editor"});

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

// ============================================================================
// Scene window — ofxNode-aware entity tree
// ============================================================================

namespace {

/// Recursive tree node helper.  Uses ofxNode::forEachChild when the entity
/// has the full ofxNode component set; falls back to direct Relationship
/// linked-list traversal for entities that carry only a Relationship.
void drawEntityNode(entt::registry& reg, entt::entity e, entt::entity& selected)
{
    if (!reg.valid(e)) return;

    // Prefer ofxNode getName() → node_component name → generic id string.
    std::string name;
    if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e))
        name = ofxNode::fromEntity(reg, e).getName();
    if (name.empty()) {
        if (auto* nc = reg.try_get<ecs::node_component>(e))
            name = nc->getName();
    }
    if (name.empty())
        name = "Entity " + std::to_string(static_cast<uint32_t>(e));

    auto* rel         = reg.try_get<ecs::Relationship>(e);
    bool  hasChildren = rel && rel->children_count > 0;

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (e == selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(static_cast<int>(e));
    bool open = ImGui::TreeNodeEx(name.c_str(), flags);

    // Click to select (don't fire when the user is toggling the arrow).
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        selected = e;

    if (open && hasChildren) {
        if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e)) {
            // Preferred path: use ofxNode for idiomatic ECS traversal.
            ofxNode node = ofxNode::fromEntity(reg, e);
            node.forEachChild([&](ofxNode child) {
                drawEntityNode(reg, child.entity(), selected);
            });
        } else {
            // Fallback: walk the Relationship sibling chain directly.
            entt::entity child = rel->first_child;
            while (child != entt::null) {
                drawEntityNode(reg, child, selected);
                auto* cr = reg.try_get<ecs::Relationship>(child);
                child = cr ? cr->next_sibling : entt::null;
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

} // anonymous namespace

void Runtime::drawSceneWindow(bool& visible)
{
    auto& reg = registry();

    ImGui::SetNextWindowPos (ImVec2(10,  40),  ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(280, 500), ImGuiCond_Once);

    if (!ImGui::Begin("Scene###ofxkit.window.scene", &visible)) { ImGui::End(); return; }

    int total = static_cast<int>(reg.storage<entt::entity>().size());
    ImGui::TextDisabled("%d entities", total);
    ImGui::Separator();
    ImGui::Spacing();

    // ── ofxNode / Relationship-based hierarchy roots ─────────────────────────
    for (auto [e, rel] : reg.view<ecs::Relationship>().each())
        if (rel.parent == entt::null)
            drawEntityNode(reg, e, m_selected);

    // ── Legacy flat nodes (node_component without Relationship) ──────────────
    for (auto [e, nc] : reg.view<ecs::node_component>().each())
        if (!reg.all_of<ecs::Relationship>(e))
            drawEntityNode(reg, e, m_selected);

    ImGui::End();
}

void Runtime::drawPropertiesWindow(bool& visible)
{
    auto& reg = registry();

    ImGui::SetNextWindowPos(ImVec2(320, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_Once);

    if (ImGui::Begin("Properties###ofxkit.window.properties", &visible)) {
        if (m_selected != entt::null && reg.valid(m_selected)) {
            inspector::inspectEntity(reg, m_selected);

            // ── Add Component picker ──────────────────────────────────────────
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("+ Add Component", {-1.f, 0.f}))
                ImGui::OpenPopup("##addComponentPopup");

            if (ImGui::BeginPopup("##addComponentPopup")) {
                ImGui::TextDisabled("── Add Component ──");
                ImGui::Separator();

                std::string lastCat;
                for (auto& desc : m_components) {
                    if (desc.category != lastCat) {
                        if (!lastCat.empty()) ImGui::Separator();
                        ImGui::TextDisabled("%s", desc.category.c_str());
                        lastCat = desc.category;
                    }

                    bool alreadyHas = desc.has && desc.has(reg, m_selected);
                    if (alreadyHas) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                        ImGui::MenuItem(("  " + desc.name).c_str(), nullptr, false, false);
                        ImGui::PopStyleColor();
                    } else {
                        if (ImGui::MenuItem(("  " + desc.name).c_str())) {
                            if (desc.add) desc.add(reg, m_selected);
                        }
                    }
                    if (!desc.description.empty() && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", desc.description.c_str());
                }
                ImGui::EndPopup();
            }
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

    if (ImGui::Begin("Shortcuts###ofxkit.window.shortcuts", &visible)) {
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
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always); // auto-size

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(2, 2));

    if (ImGui::Begin("Toolbar###ofxkit.window.toolbar", &visible, flags)) {

        // ── Drag grip + close ─────────────────────────────────────────────
        ImGui::TextDisabled(ICON_FA_GRIP_VERTICAL);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag to move toolbar");
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            ImVec2 p = ImGui::GetWindowPos();
            ImGui::SetWindowPos(ImVec2(p.x + d.x, p.y + d.y));
        }

        ImGui::SameLine();

        // Close button: small × right after the grip
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.7f,0.2f,0.2f,0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.9f,0.1f,0.1f,1.0f));
        if (ImGui::SmallButton(ICON_FA_TIMES))
            visible = false;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Close Toolbar");
        ImGui::PopStyleColor(3);

        ImGui::Separator();

        // ── Tool buttons ──────────────────────────────────────────────────
        const ImVec2 btnSize(32, 32);
        const ImVec2 framePad(4, 4);
        const ImVec4 activeCol(0.2f, 0.6f, 0.9f, 1.0f);

        std::string prevGroup = "\x01"; // sentinel — not a valid group string

        for (const auto& item : m_toolbarItems) {
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
        if (!m_imguiIniPath.empty()) {
            createParentDirectoryIfNeeded(m_imguiIniPath);
        }
        ImGui::GetIO().IniFilename = m_imguiIniPath.empty()
            ? nullptr
            : m_imguiIniPath.c_str();
        if (!m_imguiIniPath.empty() &&
            of::filesystem::exists(of::filesystem::path(m_imguiIniPath))) {
            ImGui::LoadIniSettingsFromDisk(m_imguiIniPath.c_str());
        }
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
    if (ImGui::GetCurrentContext()) applyUIScale();
    saveUIScalePref();
}

void Runtime::applyUIScale()
{
    if (!ImGui::GetCurrentContext()) return;
    m_style.applyScale(m_uiScale);
    ImGuiStyle& style = ImGui::GetStyle();
    // ImGui's default WindowMinSize {32,32} lets panels collapse to a sliver.
    // Enforce a usable minimum — 160×50 logical px — scaled with the UI scale.
    style.WindowMinSize = ImVec2(160.f * m_uiScale, 50.f * m_uiScale);
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

void Runtime::setTheme(Theme theme)
{
    m_theme    = theme;
    m_themeSet = true;
    if (!ImGui::GetCurrentContext()) {
        // Setup hasn't run yet — applyTheme() / scale will run from onSetup.
        saveThemePref();
        return;
    }
    // Reset to ImGui factory defaults first so theme switches never capture
    // already-scaled padding into ofxImGuiStyle's baseline.
    ImGui::GetStyle() = ImGuiStyle{};
    applyTheme();
    m_style.captureBaseStyle();
    applyUIScale();
    saveThemePref();
}

void Runtime::applyTheme()
{
    if (!ImGui::GetCurrentContext()) return;
    switch (m_theme) {
        case Theme::Dark:    ofxImGuiStyle::applyDarkTheme();    break;
        case Theme::Light:   ofxImGuiStyle::applyLightTheme();   break;
        case Theme::Classic: ofxImGuiStyle::applyClassicTheme(); break;
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
            if      (s == "dark")    m_theme = Theme::Dark;
            else if (s == "light")   m_theme = Theme::Light;
            else if (s == "classic") m_theme = Theme::Classic;
            m_themeSet = true;
        }
    } catch (...) { /* corrupt file — fall through to default Dark */ }
}

void Runtime::saveThemePref()
{
    std::string path = ofToDataPath("ofxKit/theme.json", true);
    try {
        of::filesystem::create_directories(of::filesystem::path(path).parent_path());
        const char* s = "dark";
        switch (m_theme) {
            case Theme::Dark:    s = "dark";    break;
            case Theme::Light:   s = "light";   break;
            case Theme::Classic: s = "classic"; break;
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

// ============================================================================
// Component registry
// ============================================================================

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

// ============================================================================
// Built-in component registrations (ofxEnTTKit — ecs::registerKitComponentMenu)
// ============================================================================

void Runtime::registerBuiltInComponents()
{
    if (m_builtInComponentsRegistered) return;
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

// ============================================================================
// Preference pages
// ============================================================================

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
    if (it == m_preferencePages.end()) return false;
    if (m_selectedPreferencePage == id) m_selectedPreferencePage.clear();
    m_preferencePages.erase(it);
    return true;
}

void Runtime::registerBuiltInPreferencePages()
{
    if (m_builtInPreferencePagesRegistered) return;
    m_builtInPreferencePagesRegistered = true;

    registerPreferencePage({"ofKitty",         "Appearance", "ofxkit.prefs.appearance", [this]{ drawPrefsAppearance(); }});
    registerPreferencePage({"openFrameworks",   "General",    "ofxkit.prefs.general",   [this]{ drawPrefsGeneral();    }});
    registerPreferencePage({"openFrameworks",   "Rendering",  "ofxkit.prefs.rendering", [this]{ drawPrefsRendering();  }});
    registerPreferencePage({"openFrameworks",   "Logging",    "ofxkit.prefs.logging",   [this]{ drawPrefsLogging();    }});
    registerPreferencePage({"openFrameworks",   "Status Bar", "ofxkit.prefs.statusbar", [this]{ drawPrefsStatusBar();  }});
}

void Runtime::drawPreferencePageList()
{
    // Collect unique categories in registration order.
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
            if (p.category != cat) continue;
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
        if (p.id != m_selectedPreferencePage) continue;
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
    // ── Theme presets ─────────────────────────────────────────────────────────
    ImGui::SeparatorText("Theme");
    if (ImGui::Button("Dark"))    setTheme(Theme::Dark);
    ImGui::SameLine();
    if (ImGui::Button("Light"))   setTheme(Theme::Light);
    ImGui::SameLine();
    if (ImGui::Button("Classic")) setTheme(Theme::Classic);
    ImGui::SameLine();
    if (ImGui::Button("Random \xef\x8b\x8b")) { // fa-dice
        ofxImGuiStyle::applyRandomAccentTheme();
        m_style.captureBaseStyle();
        applyUIScale();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Randomise the accent colour");

    // ── UI scale ──────────────────────────────────────────────────────────────
    ImGui::SeparatorText("UI Scale");
    float scale = m_uiScale;
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::SliderFloat("##scale", &scale, 0.5f, 3.0f, "%.2fx")) setUIScale(scale);
    ImGui::SameLine();
    if (ImGui::Button("Auto")) setUIScale(detectUIScale());

    // ── Save / Load theme ─────────────────────────────────────────────────────
    ImGui::SeparatorText("Theme File");
    ImGui::TextDisabled("Themes are saved as .bin files (binary ImGuiStyle snapshot).");
    if (ImGui::Button("Save Theme As...")) {
        saveFileDialog("save_theme", "Save Theme", ".bin", "my_theme.bin",
            [this](const std::string& path) {
                std::string p = path;
                if (p.size() < 4 || p.substr(p.size()-4) != ".bin") p += ".bin";
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

    // ── Full ImGui style editor ───────────────────────────────────────────────
    ImGui::SeparatorText("Style Editor");
    ImGui::ShowStyleEditor();
}

void Runtime::drawPrefsGeneral()
{
    // ── Frame ─────────────────────────────────────────────────────────────────
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

    // ── Background ────────────────────────────────────────────────────────────
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

    // ── Window ────────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Window")) {

        ImGui::BeginDisabled(true);
        int sz[2]  = { ofGetWidth(),             ofGetHeight()            };
        int pos[2] = { ofGetWindowPositionX(),    ofGetWindowPositionY()   };
        ImGui::InputInt2("Size (px)",     sz);
        ImGui::InputInt2("Position (px)", pos);
        ImGui::EndDisabled();

        static char titleBuf[128] = {};
        ImGui::InputText("Set Title", titleBuf, sizeof(titleBuf));
        ImGui::SameLine();
        if (ImGui::Button("Apply"))
            ofSetWindowTitle(titleBuf);
        ImGui::Spacing();
    }

    // ── Rulers ────────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Rulers")) {
        if (ImGui::SliderFloat("Ruler Size", &m_prefs.rulerScale, 0.5f, 3.0f, "%.2fx")) {
            saveAppPrefs();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##rulerScale")) {
            m_prefs.rulerScale = 1.0f;
            saveAppPrefs();
        }
        ImGui::TextDisabled("Scales ruler strip width and tick labels (base x UI Scale).");
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
        if (m_prefs.depthTest) ofEnableDepthTest();
        else                   ofDisableDepthTest();
        saveAppPrefs();
    }
}

void Runtime::drawPrefsLogging()
{
    const char* levels[] = {
        "Verbose", "Notice", "Warning", "Error", "Fatal Error", "Silent"
    };
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
        // Use a friendlier label — strip the reverse-DNS prefix, replace '.' with ' '
        std::string label = item.id;
        auto dot = label.rfind('.');
        if (dot != std::string::npos) label = label.substr(dot + 1);
        for (char& c : label) if (c == '_') c = ' ';
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

// ============================================================================
// Status items
// ============================================================================

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
    auto it = std::find_if(m_statusItems.begin(), m_statusItems.end(),
                           [&](const StatusItem& s) { return s.id == id; });
    if (it == m_statusItems.end()) return false;
    m_statusItems.erase(it);
    return true;
}

void Runtime::registerBuiltInStatusItems()
{
    if (m_builtInStatusItemsRegistered) return;
    m_builtInStatusItemsRegistered = true;

    registerStatusItem({"ofxkit.status.editmode", "ofxkit", true, [this]{
        if (m_editMode) {
            ImGui::TextColored({0.39f, 0.90f, 0.50f, 1.f}, "Edit Mode");
        } else {
            ImGui::TextDisabled("Edit off  (Ctrl+E)");
        }
    }});
    registerStatusItem({"ofxkit.status.appname", "ofxkit", true, [this]{
        ImGui::TextDisabled("%s", m_appName.c_str());
    }});
    registerStatusItem({"ofxkit.status.entities", "ofxkit.stats", true, [this]{
        auto& reg = registry();
        ImGui::TextDisabled("entities: %d",
                            static_cast<int>(reg.storage<entt::entity>().size()));
    }});
    registerStatusItem({"ofxkit.status.fps", "ofxkit.stats", true, []{
        ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate);
    }});
    registerStatusItem({"ofxkit.status.hint", "ofxkit.hint", true, []{
        ImGui::TextDisabled("CTRL+E  toggle edit mode");
    }});
}

// ============================================================================
// Status bar
// ============================================================================

void Runtime::drawStatusBar()
{
    // BeginViewportSideBar claims a horizontal strip from the bottom of the
    // work area BEFORE DockSpaceOverViewport runs, so the dockspace respects
    // the reserved space and PassthruCentralNode is never covered by the bar.
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_MenuBar;
    float height = ImGui::GetFrameHeight();

    if (ImGui::BeginViewportSideBar("##StatusBar", nullptr, ImGuiDir_Down, height, flags)) {
        if (ImGui::BeginMenuBar()) {
            bool        first     = true;
            std::string prevGroup = "\x01";   // sentinel — not a valid group

            for (auto& item : m_statusItems) {
                if (!item.visible || !item.draw) continue;

                bool newGroup = !item.group.empty()
                             && item.group != prevGroup
                             && prevGroup != "\x01";

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

// ============================================================================
// App Preferences window — two-pane: category list left, page content right
// ============================================================================

void Runtime::drawPreferencesWindow(bool& visible)
{
    ImGui::SetNextWindowPos (ImVec2(360, 50),  ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(580, 520), ImGuiCond_Once);

    if (!ImGui::Begin("Preferences###ofxkit.window.preferences", &visible)) { ImGui::End(); return; }

    // Seed from current OF state on first open.
    if (!m_prefs.initialized) {
        m_prefs.initialized    = true;
        m_prefs.backgroundAuto = ofGetBackgroundAuto();
        m_prefs.bgColor        = ofGetBackgroundColor();
        m_prefs.logLevel       = static_cast<int>(ofGetLogLevel());
    }

    // Auto-select the first page if none is selected yet.
    if (m_selectedPreferencePage.empty() && !m_preferencePages.empty())
        m_selectedPreferencePage = m_preferencePages.front().id;

    const float leftW = std::max(140.f, 140.f * m_uiScale);

    // Left panel — page list
    ImGui::BeginChild("##PrefList", ImVec2(leftW, 0), true);
    drawPreferencePageList();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel — selected page content
    ImGui::BeginChild("##PrefContent", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    drawPreferencePageContent();
    ImGui::EndChild();

    ImGui::End();
}

// ============================================================================
// Rulers
// ============================================================================

void Runtime::drawRulers()
{
    const float  scale  = m_uiScale * m_prefs.rulerScale;
    const float  RS     = std::round(20.f * scale);   // ruler strip width / height
    const float  fs     = std::round(9.f  * scale);   // tick-label font size
    constexpr ImU32  kBg    = IM_COL32( 25,  25,  35, 235);
    constexpr ImU32  kBord  = IM_COL32( 70,  70,  85, 255);
    constexpr ImU32  kTick  = IM_COL32(160, 160, 175, 210);
    constexpr ImU32  kLabel = IM_COL32(130, 130, 145, 255);
    constexpr ImU32  kCurs  = IM_COL32(240,  80,  80, 220);

    const ImGuiViewport* vp   = ImGui::GetMainViewport();
    const ImVec2         org  = vp->WorkPos;
    const ImVec2         size = vp->WorkSize;

    // GetForegroundDrawList() without args uses the main viewport and avoids
    // the const ImGuiViewport* → ImGuiViewport* mismatch in older ImGui builds.
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont*     fn = ImGui::GetFont();

    // Mouse position in work-area coordinates (origin = top-left of work area)
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float  mx    = mouse.x - org.x - RS;   // X relative to content start
    const float  my    = mouse.y - org.y - RS;

    // ── Horizontal ruler (top strip) ─────────────────────────────────────────
    {
        const ImVec2 rMin = { org.x + RS, org.y };
        const ImVec2 rMax = { org.x + size.x, org.y + RS };
        dl->AddRectFilled(rMin, rMax, kBg);
        dl->AddLine({ rMin.x, rMax.y }, { rMax.x, rMax.y }, kBord);

        for (float px = 0.f; px < size.x - RS; px += 10.f) {
            const float x       = rMin.x + px;
            const bool  major   = std::fmod(px, 100.f) < 0.5f;
            const bool  mid     = std::fmod(px,  50.f) < 0.5f;
            const float tickLen = major ? RS * 0.65f : mid ? RS * 0.45f : RS * 0.22f;
            dl->AddLine({ x, rMax.y - tickLen }, { x, rMax.y }, kTick);
            if (major && px > 0.f) {
                char buf[12];
                snprintf(buf, sizeof(buf), "%.0f", px);
                dl->AddText(fn, fs, { x + 2.f, org.y + 2.f }, kLabel, buf);
            }
        }

        // Cursor marker
        if (mx >= 0.f && mx < size.x - RS) {
            const float cx = rMin.x + mx;
            dl->AddLine({ cx, org.y }, { cx, org.y + RS }, kCurs, 1.5f);

            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", mx);
            const float tw = ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize());
            dl->AddText(fn, fs, { cx - tw * 0.5f, org.y + 2.f }, kCurs, buf);
        }
    }

    // ── Vertical ruler (left strip) ──────────────────────────────────────────
    {
        const ImVec2 rMin = { org.x, org.y + RS };
        const ImVec2 rMax = { org.x + RS, org.y + size.y };
        dl->AddRectFilled(rMin, rMax, kBg);
        dl->AddLine({ rMax.x, rMin.y }, { rMax.x, rMax.y }, kBord);

        for (float py = 0.f; py < size.y - RS; py += 10.f) {
            const float y       = rMin.y + py;
            const bool  major   = std::fmod(py, 100.f) < 0.5f;
            const bool  mid     = std::fmod(py,  50.f) < 0.5f;
            const float tickLen = major ? RS * 0.65f : mid ? RS * 0.45f : RS * 0.22f;
            dl->AddLine({ rMax.x - tickLen, y }, { rMax.x, y }, kTick);
            if (major && py > 0.f) {
                char buf[12];
                snprintf(buf, sizeof(buf), "%.0f", py);
                dl->AddText(fn, fs, { org.x + 1.f, y + 2.f }, kLabel, buf);
            }
        }

        // Cursor marker
        if (my >= 0.f && my < size.y - RS) {
            const float cy = rMin.y + my;
            dl->AddLine({ org.x, cy }, { org.x + RS, cy }, kCurs, 1.5f);

            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", my);
            dl->AddText(fn, fs, { org.x + 1.f, cy + 2.f }, kCurs, buf);
        }
    }

    // ── Corner square (shows live X / Y coordinate) ───────────────────────────
    {
        dl->AddRectFilled(org, { org.x + RS, org.y + RS }, kBg);
        dl->AddLine({ org.x + RS, org.y },      { org.x + RS, org.y + RS }, kBord);
        dl->AddLine({ org.x,      org.y + RS },  { org.x + RS, org.y + RS }, kBord);

        if (mx >= 0.f && my >= 0.f) {
            char buf[20];
            snprintf(buf, sizeof(buf), "%d,%d",
                     static_cast<int>(mx), static_cast<int>(my));
            const float tw = ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize());
            // Show coordinate as a floating tooltip near the cursor
            dl->AddText(fn, fs,
                        { mouse.x - tw * 0.5f, mouse.y - RS - 2.f },
                        kCurs, buf);
        }
    }
}

// ============================================================================
// App preferences persistence
// ============================================================================

void Runtime::loadAppPrefs()
{
    std::string path = ofToDataPath("ofxKit/appPrefs.json", true);
    try {
        std::ifstream in(path);
        if (!in.is_open()) return;
        ofJson j; in >> j;

        if (j.contains("circleRes"))      m_prefs.circleRes      = j["circleRes"];
        if (j.contains("targetFps"))      m_prefs.targetFps      = j["targetFps"];
        if (j.contains("vsync"))          m_prefs.vsync          = j["vsync"];
        if (j.contains("backgroundAuto")) m_prefs.backgroundAuto = j["backgroundAuto"];
        if (j.contains("lineWidth"))      m_prefs.lineWidth      = j["lineWidth"];
        if (j.contains("smoothLighting")) m_prefs.smoothLighting = j["smoothLighting"];
        if (j.contains("depthTest"))      m_prefs.depthTest      = j["depthTest"];
        if (j.contains("logLevel"))       m_prefs.logLevel       = j["logLevel"];
        if (j.contains("rulerScale"))     m_prefs.rulerScale     = std::clamp(j["rulerScale"].get<float>(), 0.5f, 3.0f);
        if (j.contains("bgR")) {
            m_prefs.bgColor.set(
                static_cast<unsigned char>((int)j["bgR"]),
                static_cast<unsigned char>((int)j["bgG"]),
                static_cast<unsigned char>((int)j["bgB"]),
                static_cast<unsigned char>((int)j["bgA"]));
        }

        // Restore window visibility states. Prefer stable window ids; keep
        // visible-name fallback for prefs written before RuntimeWindow::id.
        if (j.contains("windowVisibility") && j["windowVisibility"].is_object()) {
            m_savedWindowVisibility = j["windowVisibility"].get<std::unordered_map<std::string,bool>>();
            // Apply to any windows already registered (built-ins).
            for (auto& w : m_windows) {
                if (!w.id.empty() && m_savedWindowVisibility.count(w.id))
                    w.visible = m_savedWindowVisibility.at(w.id);
                else if (m_savedWindowVisibility.count(w.name))
                    w.visible = m_savedWindowVisibility.at(w.name);
            }
        }

        // Apply loaded values to OF
        ofSetCircleResolution(m_prefs.circleRes);
        ofSetFrameRate(m_prefs.targetFps);
        ofSetVerticalSync(m_prefs.vsync);
        ofSetBackgroundAuto(m_prefs.backgroundAuto);
        ofSetLineWidth(m_prefs.lineWidth);
        ofSetSmoothLighting(m_prefs.smoothLighting);
        if (m_prefs.depthTest) ofEnableDepthTest(); else ofDisableDepthTest();
        ofSetLogLevel(static_cast<ofLogLevel>(m_prefs.logLevel));
        ofBackground(m_prefs.bgColor);
    } catch (...) { /* corrupt or missing file — use defaults */ }
}

void Runtime::saveAppPrefs()
{
    // Flush imgui.ini so docking state is always in sync with our prefs.
    if (const char* iniPath = ImGui::GetIO().IniFilename) {
        createParentDirectoryIfNeeded(iniPath);
        ImGui::SaveIniSettingsToDisk(iniPath);
    }

    std::string path = ofToDataPath("ofxKit/appPrefs.json", true);
    try {
        of::filesystem::create_directories(
            of::filesystem::path(path).parent_path());
        ofJson j;
        j["circleRes"]      = m_prefs.circleRes;
        j["targetFps"]      = m_prefs.targetFps;
        j["vsync"]          = m_prefs.vsync;
        j["backgroundAuto"] = m_prefs.backgroundAuto;
        j["lineWidth"]      = m_prefs.lineWidth;
        j["smoothLighting"] = m_prefs.smoothLighting;
        j["depthTest"]      = m_prefs.depthTest;
        j["logLevel"]       = m_prefs.logLevel;
        j["rulerScale"]     = m_prefs.rulerScale;
        j["bgR"]            = static_cast<int>(m_prefs.bgColor.r);
        j["bgG"]            = static_cast<int>(m_prefs.bgColor.g);
        j["bgB"]            = static_cast<int>(m_prefs.bgColor.b);
        j["bgA"]            = static_cast<int>(m_prefs.bgColor.a);

        // Persist visibility state for every registered window.
        ofJson vis = ofJson::object();
        for (const auto& w : m_windows)
            vis[w.id.empty() ? w.name : w.id] = w.visible;
        j["windowVisibility"] = vis;

        std::ofstream out(path);
        out << j.dump(2);
    } catch (...) { /* don't crash the app on a save failure */ }
}

// ============================================================================
// Default dock layout
// ============================================================================

void Runtime::buildDefaultDockLayout(ImGuiID dockId)
{
    const ImVec2 size = ImGui::GetMainViewport()->WorkSize;

    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode  (dockId,
        ImGuiDockNodeFlags_DockSpace |
        ImGuiDockNodeFlags_PassthruCentralNode |
        ImGuiDockNodeFlags_NoDockingOverCentralNode);
    ImGui::DockBuilderSetNodeSize(dockId, size);

    // Split off a left panel (22 % of width), then a right panel
    // (28 % of the remaining width, which is ~22 % of the total).
    // The centre node is left as the transparent pass-through viewport.
    ImGuiID left, right, centre = dockId;
    ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left,  0.22f, &left,  &centre);
    ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.28f, &right, &centre);

    ImGui::DockBuilderDockWindow("Scene###ofxkit.window.scene",           left);
    for (const auto& name : m_defaultLayoutExtraLeftDocks) {
        if (!name.empty())
            ImGui::DockBuilderDockWindow(name.c_str(), left);
    }
    ImGui::DockBuilderDockWindow("Properties###ofxkit.window.properties", right);
    // Shortcuts and Preferences start hidden; dock them to the right panel so
    // they appear there if the user enables them.
    ImGui::DockBuilderDockWindow("Shortcuts###ofxkit.window.shortcuts",     right);
    ImGui::DockBuilderDockWindow("Preferences###ofxkit.window.preferences", right);

    ImGui::DockBuilderFinish(dockId);

    // Explicitly mark the central node as passthrough + no-dock after
    // DockBuilderFinish, because split children don't automatically
    // inherit PassthruCentralNode from the root.
    if (ImGuiDockNode* cn = ImGui::DockBuilderGetCentralNode(dockId)) {
        cn->LocalFlags |= ImGuiDockNodeFlags_PassthruCentralNode |
                          ImGuiDockNodeFlags_NoDockingOverCentralNode |
                          ImGuiDockNodeFlags_NoTabBar;
    }
}

// ============================================================================
// File dialog
// ============================================================================

void Runtime::openFileDialog(const std::string& key,
                             const std::string& title,
                             const std::string& filters,
                             std::function<void(const std::string& path)> onConfirm)
{
    m_fileDialogCbs[key] = std::move(onConfirm);
    IGFD::FileDialogConfig cfg;
    cfg.path  = ".";
    cfg.flags = ImGuiFileDialogFlags_Modal;
    ImGuiFileDialog::Instance()->OpenDialog(key, title, filters.c_str(), cfg);
}

void Runtime::saveFileDialog(const std::string& key,
                             const std::string& title,
                             const std::string& filters,
                             const std::string& defaultFileName,
                             std::function<void(const std::string& path)> onConfirm)
{
    m_fileDialogCbs[key] = std::move(onConfirm);
    IGFD::FileDialogConfig cfg;
    cfg.path            = ".";
    cfg.fileName        = defaultFileName;
    cfg.flags           = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog(key, title, filters.c_str(), cfg);
}

void Runtime::processFileDialogs()
{
    const ImVec2 minSz(480.f * m_uiScale, 320.f * m_uiScale);
    const ImVec2 maxSz(FLT_MAX, FLT_MAX);

    // IGFD processes one modal at a time; iterate our callback map and
    // display each key that the library currently wants to show.
    for (auto it = m_fileDialogCbs.begin(); it != m_fileDialogCbs.end(); ) {
        const std::string& key = it->first;
        if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoCollapse, minSz, maxSz)) {
            if (ImGuiFileDialog::Instance()->IsOk() && it->second) {
                it->second(ImGuiFileDialog::Instance()->GetFilePathName());
            }
            ImGuiFileDialog::Instance()->Close();
            it = m_fileDialogCbs.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Gizmo overlay
// ============================================================================

void Runtime::setSceneCamera(ofCamera* cam)
{
    m_sceneCamera   = cam;
    m_sceneEasyCam  = cam ? dynamic_cast<ofEasyCam*>(cam) : nullptr;
    // Reset legacy manual-capture flag so we fall back to the new API path.
    m_sceneViewCaptured = false;
}

void Runtime::clearSceneCamera()
{
    setSceneCamera(nullptr);
}

bool Runtime::isSceneHovered() const
{
    return m_sceneViewport.isHovered() && !isGizmoActive();
}

void Runtime::drawGizmoOverlay()
{
    if (!m_sceneCamera) return;
    if (m_selected == entt::null || !registry().valid(m_selected)) return;
    auto* nc = registry().try_get<ecs::node_component>(m_selected);
    if (!nc) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // Draw into a fullscreen no-input/no-background window that sits behind
    // normal docked panels but in front of the OF scene.
    // NoBringToFrontOnFocus keeps it below panel windows when they are clicked.
    // NoNav / NoInputs let clicks fall through to the gizmo hit-test or OF.
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    constexpr ImGuiWindowFlags kOverlayFlags =
        ImGuiWindowFlags_NoTitleBar        | ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoScrollbar       | ImGuiWindowFlags_NoScrollWithMouse|
        ImGuiWindowFlags_NoCollapse        | ImGuiWindowFlags_NoSavedSettings  |
        ImGuiWindowFlags_NoDocking         | ImGuiWindowFlags_NoNav            |
        ImGuiWindowFlags_NoInputs          |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##gizmo_scene_overlay", nullptr, kOverlayFlags);
    ImGui::PopStyleVar(2);

    // Pass the ImGui viewport rect so SetRect and the pick ray use the same
    // logical-pixel coordinate space as io.MousePos.
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetID(1);
    const ofRectangle sceneRect(vp->Pos.x, vp->Pos.y, vp->Size.x, vp->Size.y);

    auto toIGOp = [](GizmoOperation op) -> ImGuizmo::OPERATION {
        switch (op) {
            case GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
            case GizmoOperation::Scale:     return ImGuizmo::SCALE;
            case GizmoOperation::Universal: return ImGuizmo::UNIVERSAL;
            default:                        return ImGuizmo::TRANSLATE;
        }
    };
    auto toIGMode = [](GizmoMode m) -> ImGuizmo::MODE {
        return m == GizmoMode::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    };

    ImGuizmo::Manipulate(*m_sceneCamera, nc->node,
                         toIGOp(m_gizmoOp), toIGMode(m_gizmoMode),
                         &sceneRect);

    ImGui::End();
}

void Runtime::captureSceneView()
{
    // Legacy manual capture — still works but no longer needed when using
    // ofCamera::begin(), which caches getOrientedProjectionMatrix() automatically.
    m_capturedView      = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
    m_capturedProj      = ofGetCurrentMatrix(OF_MATRIX_PROJECTION);
    m_sceneViewCaptured = true;
}

bool Runtime::isGizmoActive() const
{
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void Runtime::drawGizmoInViewport(ViewportInstance& vp, const ofRectangle& imgScreenRect)
{
    if (m_selected == entt::null || !registry().valid(m_selected)) return;
    auto* nc = registry().try_get<ecs::node_component>(m_selected);
    if (!nc) return;

    auto toIGOp = [](GizmoOperation op) -> ImGuizmo::OPERATION {
        switch (op) {
            case GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
            case GizmoOperation::Scale:     return ImGuizmo::SCALE;
            case GizmoOperation::Universal: return ImGuizmo::UNIVERSAL;
            default:                        return ImGuizmo::TRANSLATE;
        }
    };
    auto toIGMode = [](GizmoMode m) -> ImGuizmo::MODE {
        return m == GizmoMode::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    };

    // Draw into this panel's own draw list.
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetID(static_cast<int>(
        1000 + (reinterpret_cast<uintptr_t>(&vp) & 0x0fffffff)));

    // SetAlternativeWindow ensures CanActivate() accepts hover from this
    // docked window even when FindWindowByName() fails to match g.HoveredWindow
    // (e.g. docking host indirection). Reset immediately after Manipulate.
    ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
    ImGuizmo::Manipulate(vp.cam, nc->node,
                         toIGOp(m_gizmoOp), toIGMode(m_gizmoMode),
                         &imgScreenRect);
    ImGuizmo::SetAlternativeWindow(nullptr);
}

// ============================================================================
// Code Editor window
// ============================================================================

void Runtime::codeEditorSetText(const std::string& text)
{
    m_textEditor.SetText(text);
}

std::string Runtime::codeEditorGetText() const
{
    return m_textEditor.GetText();
}

void Runtime::codeEditorSetLanguage(TextEditor::LanguageDefinitionId lang)
{
    m_textEditor.SetLanguageDefinition(lang);
}

void Runtime::drawCodeEditorWindow(bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(820, 580), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_MenuBar;
    if (!ImGui::Begin("Code Editor###ofxkit.window.code_editor", &visible, winFlags)) {
        ImGui::End(); return;
    }

    // ── Find/Replace bar state (persistent across frames) ────────────────────
    static char  s_findBuf[256]    = {};
    static char  s_replaceBuf[256] = {};
    static bool  s_caseSensitive   = false;
    static bool  s_findVisible     = false;
    static bool  s_replaceVisible  = false;

    // ── Helper lambdas ────────────────────────────────────────────────────────
    auto detectLanguage = [this](const std::string& path) {
        const std::string ext = ofFilePath::getFileExt(path);
        if      (ext == "glsl" || ext == "vert" || ext == "frag")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Glsl);
        else if (ext == "hlsl")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Hlsl);
        else if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cpp);
        else if (ext == "cs")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cs);
        else if (ext == "lua")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Lua);
        else if (ext == "py")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Python);
        else if (ext == "json")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Json);
        else if (ext == "sql")
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Sql);
        else
            m_textEditor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
    };

    auto doOpen = [&] {
        // ".*" in IGFD shows ALL files; named groups give filter presets.
        openFileDialog("code_open", "Open File",
            "Source{.cpp,.h,.hpp,.c,.cs,.py,.lua,.js,.ts},"
            "Shaders{.glsl,.vert,.frag,.hlsl},"
            "Data{.json,.xml,.yaml,.txt,.md},"
            "All Files{.*}",
            [this, detectLanguage](const std::string& path) {
                std::ifstream ifs(path);
                if (ifs) {
                    m_textEditorFilePath = path;
                    m_textEditor.SetText(std::string(
                        std::istreambuf_iterator<char>(ifs),
                        std::istreambuf_iterator<char>()));
                    detectLanguage(path);
                }
            });
    };

    auto doSave = [&] {
        if (m_textEditorFilePath.empty()) {
            saveFileDialog("code_save", "Save As",
                "Source{.cpp,.h,.hpp,.c,.glsl,.vert,.frag,.hlsl,.py,.lua},"
                "Text & Data{.txt,.md,.json,.xml,.yaml},"
                "All Files{.*}",
                "untitled.glsl",
                [this](const std::string& path) {
                    m_textEditorFilePath = path;
                    std::ofstream ofs(path);
                    if (ofs) ofs << m_textEditor.GetText();
                });
        } else {
            std::ofstream ofs(m_textEditorFilePath);
            if (ofs) ofs << m_textEditor.GetText();
        }
    };

    auto doSaveAs = [&] {
        saveFileDialog("code_save_as", "Save As",
            "Source{.cpp,.h,.hpp,.c,.glsl,.vert,.frag,.hlsl,.py,.lua},"
            "Text & Data{.txt,.md,.json,.xml,.yaml},"
            "All Files{.*}",
            m_textEditorFilePath.empty() ? "untitled.glsl"
                                         : ofFilePath::getFileName(m_textEditorFilePath),
            [this](const std::string& path) {
                m_textEditorFilePath = path;
                std::ofstream ofs(path);
                if (ofs) ofs << m_textEditor.GetText();
            });
    };

    // ── Keyboard shortcuts (when window is focused) ───────────────────────────
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
            m_textEditor.SetText(""); m_textEditorFilePath = "";
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) doOpen();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
            if (ImGui::GetIO().KeyShift) doSaveAs(); else doSave();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) {
            s_findVisible = true; s_replaceVisible = false;
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_H)) {
            s_findVisible = true; s_replaceVisible = true;
        }
        if (s_findVisible && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s_findVisible = false;
        }
    }

    // ── Menu bar ─────────────────────────────────────────────────────────────
    if (ImGui::BeginMenuBar()) {

        // File ----------------------------------------------------------------
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New",      "Ctrl+N")) {
                m_textEditor.SetText(""); m_textEditorFilePath = "";
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open...",  "Ctrl+O")) doOpen();
            ImGui::Separator();
            bool noPath = m_textEditorFilePath.empty();
            ImGui::BeginDisabled(m_textEditor.IsReadOnlyEnabled());
            if (ImGui::MenuItem(noPath ? "Save As..." : "Save", "Ctrl+S")) doSave();
            ImGui::EndDisabled();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) doSaveAs();
            ImGui::EndMenu();
        }

        // Edit ----------------------------------------------------------------
        if (ImGui::BeginMenu("Edit")) {
            bool ro = m_textEditor.IsReadOnlyEnabled();
            ImGui::BeginDisabled(!m_textEditor.CanUndo() || ro);
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) m_textEditor.Undo();
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!m_textEditor.CanRedo() || ro);
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) m_textEditor.Redo();
            ImGui::EndDisabled();
            ImGui::Separator();
            if (ImGui::MenuItem("Find",           "Ctrl+F")) { s_findVisible = true;  s_replaceVisible = false; }
            if (ImGui::MenuItem("Find & Replace", "Ctrl+H")) { s_findVisible = true;  s_replaceVisible = true;  }
            ImGui::Separator();
            if (ImGui::MenuItem("Read Only", nullptr, ro)) m_textEditor.SetReadOnlyEnabled(!ro);
            ImGui::EndMenu();
        }

        // View ----------------------------------------------------------------
        if (ImGui::BeginMenu("View")) {
            // Language sub-menu
            if (ImGui::BeginMenu("Language")) {
                const struct { const char* label; TextEditor::LanguageDefinitionId id; } kLangs[] = {
                    {"None",       TextEditor::LanguageDefinitionId::None},
                    {"C++",        TextEditor::LanguageDefinitionId::Cpp},
                    {"C",          TextEditor::LanguageDefinitionId::C},
                    {"C#",         TextEditor::LanguageDefinitionId::Cs},
                    {"Python",     TextEditor::LanguageDefinitionId::Python},
                    {"Lua",        TextEditor::LanguageDefinitionId::Lua},
                    {"JSON",       TextEditor::LanguageDefinitionId::Json},
                    {"SQL",        TextEditor::LanguageDefinitionId::Sql},
                    {"AngelScript",TextEditor::LanguageDefinitionId::AngelScript},
                    {"GLSL",       TextEditor::LanguageDefinitionId::Glsl},
                    {"HLSL",       TextEditor::LanguageDefinitionId::Hlsl},
                };
                auto curLang = m_textEditor.GetLanguageDefinition();
                for (auto& l : kLangs)
                    if (ImGui::MenuItem(l.label, nullptr, curLang == l.id))
                        m_textEditor.SetLanguageDefinition(l.id);
                ImGui::EndMenu();
            }
            // Theme sub-menu
            if (ImGui::BeginMenu("Colour Theme")) {
                const struct { const char* label; TextEditor::PaletteId id; } kPals[] = {
                    {"Dark",      TextEditor::PaletteId::Dark},
                    {"Light",     TextEditor::PaletteId::Light},
                    {"Mariana",   TextEditor::PaletteId::Mariana},
                    {"Retro Blue",TextEditor::PaletteId::RetroBlue},
                };
                auto curPal = m_textEditor.GetPalette();
                for (auto& p : kPals)
                    if (ImGui::MenuItem(p.label, nullptr, curPal == p.id))
                        m_textEditor.SetPalette(p.id);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            // Toggles
            bool lineNums  = m_textEditor.IsShowLineNumbersEnabled();
            if (ImGui::MenuItem("Line Numbers",    nullptr, lineNums))
                m_textEditor.SetShowLineNumbersEnabled(!lineNums);
            bool autoInd   = m_textEditor.IsAutoIndentEnabled();
            if (ImGui::MenuItem("Auto Indent",     nullptr, autoInd))
                m_textEditor.SetAutoIndentEnabled(!autoInd);
            bool showWS    = m_textEditor.IsShowWhitespacesEnabled();
            if (ImGui::MenuItem("Show Whitespace", nullptr, showWS))
                m_textEditor.SetShowWhitespacesEnabled(!showWS);
            bool shortTabs = m_textEditor.IsShortTabsEnabled();
            if (ImGui::MenuItem("Short Tabs",      nullptr, shortTabs))
                m_textEditor.SetShortTabsEnabled(!shortTabs);
            ImGui::Separator();
            // Numeric settings inline
            {
                int tabSz = m_textEditor.GetTabSize();
                ImGui::SetNextItemWidth(60.f);
                if (ImGui::DragInt("Tab Size",    &tabSz, 1.f, 1, 8))
                    m_textEditor.SetTabSize(tabSz);
            }
            {
                float ls = m_textEditor.GetLineSpacing();
                ImGui::SetNextItemWidth(60.f);
                if (ImGui::DragFloat("Line Spacing", &ls, 0.05f, 1.0f, 2.0f, "%.2f"))
                    m_textEditor.SetLineSpacing(ls);
            }
            ImGui::EndMenu();
        }

        // File path breadcrumb (right-aligned) ─────────────────────────────
        if (!m_textEditorFilePath.empty()) {
            const std::string fname = ofFilePath::getFileName(m_textEditorFilePath);
            float fW  = ImGui::CalcTextSize(fname.c_str()).x + 8.f;
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > fW)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - fW);
            ImGui::TextDisabled("%s", fname.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", m_textEditorFilePath.c_str());
        }

        ImGui::EndMenuBar();
    }

    // ── Find / Replace bar ────────────────────────────────────────────────────
    if (s_findVisible) {
        ImGui::Separator();

        // Find row
        ImGui::SetNextItemWidth(280.f);
        bool enterPressed = ImGui::InputText("##find", s_findBuf, sizeof(s_findBuf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ImGui::Checkbox("Aa", &s_caseSensitive);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Case Sensitive");
        ImGui::SameLine();

        auto doFindNext = [&] {
            if (s_findBuf[0] != '\0')
                m_textEditor.SelectNextOccurrenceOf(s_findBuf, (int)strlen(s_findBuf), s_caseSensitive);
        };
        auto doFindAll = [&] {
            if (s_findBuf[0] != '\0')
                m_textEditor.SelectAllOccurrencesOf(s_findBuf, (int)strlen(s_findBuf), s_caseSensitive);
        };

        if (ImGui::SmallButton("Next##code_find") || enterPressed) doFindNext();
        ImGui::SameLine();
        if (ImGui::SmallButton("All##code_find"))  doFindAll();
        ImGui::SameLine();
        if (ImGui::SmallButton("x##code_find")) s_findVisible = false;

        // Hint: how many times the search term appears
        if (s_findBuf[0] != '\0') {
            const std::string full = m_textEditor.GetText();
            const std::string term = s_findBuf;
            int count = 0;
            std::string haystack = s_caseSensitive ? full : [&]{
                std::string lc = full;
                std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
                return lc;
            }();
            std::string needle = s_caseSensitive ? term : [&]{
                std::string lc = term;
                std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
                return lc;
            }();
            for (size_t pos = 0; (pos = haystack.find(needle, pos)) != std::string::npos; pos += needle.size())
                ++count;
            ImGui::SameLine();
            if (count == 0) ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "no matches");
            else            ImGui::TextDisabled("%d match%s", count, count == 1 ? "" : "es");
        }

        // Replace row (shown when s_replaceVisible)
        if (s_replaceVisible) {
            ImGui::SetNextItemWidth(280.f);
            ImGui::InputText("##replace", s_replaceBuf, sizeof(s_replaceBuf));
            ImGui::SameLine();

            // Replace current: only if selected text matches the search term
            bool hasMatch = m_textEditor.AnyCursorHasSelection();
            ImGui::BeginDisabled(!hasMatch || m_textEditor.IsReadOnlyEnabled());
            if (ImGui::SmallButton("Replace##code_find_current")) {
                // Select the next occurrence so the user can keep pressing Replace to step through
                if (s_findBuf[0] != '\0') {
                    std::string src = m_textEditor.GetText();
                    const std::string term = s_findBuf;
                    const std::string repl = s_replaceBuf;
                    // Find the first occurrence from cursor and replace just that one
                    std::string lower_src  = src;
                    std::string lower_term = term;
                    if (!s_caseSensitive) {
                        std::transform(lower_src.begin(),  lower_src.end(),  lower_src.begin(),  ::tolower);
                        std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);
                    }
                    size_t pos = lower_src.find(lower_term);
                    if (pos != std::string::npos) {
                        src.replace(pos, term.size(), repl);
                        m_textEditor.SetText(src);
                        // Move to next occurrence
                        m_textEditor.SelectNextOccurrenceOf(s_findBuf, (int)strlen(s_findBuf), s_caseSensitive);
                    }
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(m_textEditor.IsReadOnlyEnabled());
            if (ImGui::SmallButton("Replace All##code_find_all")) {
                if (s_findBuf[0] != '\0') {
                    std::string src  = m_textEditor.GetText();
                    const std::string term = s_findBuf;
                    const std::string repl = s_replaceBuf;
                    int replCount = 0;
                    if (s_caseSensitive) {
                        for (size_t pos = 0; (pos = src.find(term, pos)) != std::string::npos; ) {
                            src.replace(pos, term.size(), repl);
                            pos += repl.size();
                            ++replCount;
                        }
                    } else {
                        std::string lower = src;
                        std::string lterm = term;
                        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                        std::transform(lterm.begin(), lterm.end(), lterm.begin(), ::tolower);
                        for (size_t pos = 0; (pos = lower.find(lterm, pos)) != std::string::npos; ) {
                            src.replace(pos, term.size(), repl);
                            lower.replace(pos, lterm.size(), repl.size() > 0
                                ? std::string(repl.size(), ' ') : std::string());
                            pos += repl.size();
                            ++replCount;
                        }
                    }
                    if (replCount > 0) m_textEditor.SetText(src);
                    ofLogNotice("ofxKit::CodeEditor") << "Replaced " << replCount << " occurrence(s).";
                }
            }
            ImGui::EndDisabled();
        }
    }

    ImGui::Separator();

    // ── Editor ───────────────────────────────────────────────────────────────
    const float statusH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
    ImVec2 editorSize   = ImGui::GetContentRegionAvail();
    editorSize.y       -= statusH;

    m_textEditor.Render("##code", ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows), editorSize);

    // ── Status bar ────────────────────────────────────────────────────────────
    ImGui::Separator();
    int curLine = 0, curCol = 0;
    m_textEditor.GetCursorPosition(curLine, curCol);
    ImGui::TextDisabled("Ln %d, Col %d   |   %d lines   |   %s%s",
        curLine + 1, curCol + 1,
        m_textEditor.GetLineCount(),
        m_textEditor.GetLanguageDefinitionName(),
        m_textEditor.IsReadOnlyEnabled() ? "   [Read Only]" : "");

    ImGui::End();
}

// ============================================================================
// Path Editor window
// ============================================================================

void Runtime::drawPathEditorWindow(bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Path Editor###ofxkit.window.path_editor", &visible)) { ImGui::End(); return; }

    // Detect selection change — sync from path_component if available.
    if (m_selected != m_pathEditorEntity) {
        m_pathEditorEntity = m_selected;
        m_pathEditorPath.clear();

        if (m_selected != entt::null && registry().valid(m_selected)) {
            if (auto* pc = registry().try_get<ecs::path_component>(m_selected)) {
                // Convert ofPath commands → ImVectorEditor::Path
                // moveTo → new anchor at position; bezierTo → anchor with handles;
                // lineTo → anchor without handles; close → path.closed = true.
                const auto& cmds = pc->path.getCommands();
                for (size_t i = 0; i < cmds.size(); ++i) {
                    const auto& cmd = cmds[i];
                    if (cmd.type == ofPath::Command::moveTo ||
                        cmd.type == ofPath::Command::lineTo) {
                        ImVectorEditor::Anchor a;
                        a.position = ImVec2(cmd.to.x, cmd.to.y);
                        m_pathEditorPath.anchors.push_back(a);
                    } else if (cmd.type == ofPath::Command::bezierTo) {
                        // bezierTo has cp1, cp2, to. cp1 is handle-out of prev anchor.
                        if (!m_pathEditorPath.anchors.empty()) {
                            auto& prev         = m_pathEditorPath.anchors.back();
                            prev.handleOut     = ImVec2(cmd.cp1.x, cmd.cp1.y);
                            prev.hasHandleOut  = true;
                        }
                        ImVectorEditor::Anchor a;
                        a.position    = ImVec2(cmd.to.x, cmd.to.y);
                        a.handleIn    = ImVec2(cmd.cp2.x, cmd.cp2.y);
                        a.hasHandleIn = true;
                        m_pathEditorPath.anchors.push_back(a);
                    } else if (cmd.type == ofPath::Command::close) {
                        m_pathEditorPath.closed = true;
                    }
                }
            }
        }
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────
    bool isPen = (m_pathEditorConfig.tool == ImVectorEditor::Tool::Pen);
    if (ImGui::SmallButton(isPen ? "Pen (active)" : "Pen"))
        m_pathEditorConfig.tool = ImVectorEditor::Tool::Pen;
    ImGui::SameLine();
    if (ImGui::SmallButton(!isPen ? "Select (active)" : "Select"))
        m_pathEditorConfig.tool = ImVectorEditor::Tool::Select;
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) m_pathEditorPath.clear();

    // Apply button — write back to path_component
    ImGui::SameLine();
    bool canApply = (m_pathEditorEntity != entt::null
                     && registry().valid(m_pathEditorEntity)
                     && registry().all_of<ecs::path_component>(m_pathEditorEntity));
    ImGui::BeginDisabled(!canApply);
    if (ImGui::SmallButton("Apply to Entity")) {
        auto& pc = registry().get<ecs::path_component>(m_pathEditorEntity);
        pc.path.clear();
        bool first = true;
        for (auto& anchor : m_pathEditorPath.anchors) {
            glm::vec3 pos(anchor.position.x, anchor.position.y, 0.f);
            if (first) {
                pc.path.moveTo(pos);
                first = false;
            } else if (anchor.hasHandleIn || anchor.hasHandleOut) {
                glm::vec3 cp1(anchor.handleIn.x, anchor.handleIn.y, 0.f);
                glm::vec3 cp2(anchor.handleOut.x, anchor.handleOut.y, 0.f);
                pc.path.bezierTo(cp1, cp2, pos);
            } else {
                pc.path.lineTo(pos);
            }
        }
        if (m_pathEditorPath.closed) pc.path.close();
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // ── Canvas ────────────────────────────────────────────────────────────────
    m_pathEditorConfig.canvasSize = ImGui::GetContentRegionAvail();
    m_pathEditorWidget.Draw("##pathEditor", m_pathEditorPath, m_pathEditorConfig);

    ImGui::End();
}

// ============================================================================
// Viewport windows — multi-instance FBO-based scene views
// ============================================================================

void Runtime::setViewportRenderer(ViewportRenderer fn)
{
    m_viewportRenderer = std::move(fn);
}

void Runtime::clearViewportRenderer()
{
    m_viewportRenderer = nullptr;
}

Runtime::ViewportInstance* Runtime::addViewportWindow(std::string title)
{
    // Auto-name: "Scene View", "Scene View 2", "Scene View 3", …
    if (title.empty()) {
        title = "Scene View";
        int n = 2;
        while (std::any_of(m_viewportInstances.begin(), m_viewportInstances.end(),
                           [&](const std::unique_ptr<ViewportInstance>& p) {
                               return p->title == title; })) {
            title = "Scene View " + std::to_string(n++);
        }
    }

    // Guard against duplicate titles — ImGui requires unique window names.
    for (auto& inst : m_viewportInstances) {
        if (inst->title == title) {
            ofLogWarning("ofxKit") << "addViewportWindow: title '" << title
                                   << "' already exists; returning existing instance.";
            return inst.get();
        }
    }

    auto& inst = m_viewportInstances.emplace_back(std::make_unique<ViewportInstance>());
    inst->title = title;

    // Capture raw pointer — stable because ViewportInstance lives on the heap
    // and unique_ptr moves never invalidate the pointed-to object.
    ViewportInstance* raw = inst.get();

    registerWindow({title, "View", true, true,
        [this, raw](bool& visible) { drawViewportWindow(*raw, visible); }});

    return raw;
}

void Runtime::removeViewportWindow(const std::string& title)
{
    // Remove from the RuntimeWindow list
    m_windows.erase(
        std::remove_if(m_windows.begin(), m_windows.end(),
            [&](const RuntimeWindow& w) { return w.name == title; }),
        m_windows.end());

    // Destroy the instance (releases the FBO / camera)
    m_viewportInstances.erase(
        std::remove_if(m_viewportInstances.begin(), m_viewportInstances.end(),
            [&](const std::unique_ptr<ViewportInstance>& p) { return p->title == title; }),
        m_viewportInstances.end());
}

// Position the camera from spherical coords and point it at vp.target.
void Runtime::updateViewportCamera(ViewportInstance& vp)
{
    float az = glm::radians(vp.azimuth);
    float el = glm::radians(vp.elevation);
    glm::vec3 offset(
        vp.distance * std::cos(el) * std::sin(az),
        vp.distance * std::sin(el),
        vp.distance * std::cos(el) * std::cos(az)
    );
    vp.cam.setPosition(vp.target + offset);
    vp.cam.lookAt(vp.target);
}

void Runtime::drawViewportWindow(ViewportInstance& vp, bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(480, 320), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(vp.title.c_str(), &visible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // --- Menu bar ---------------------------------------------------------------
    struct Preset { const char* label; float az; float el; };
    static const Preset presets[] = {
        { "Perspective",  30.f,  20.f  },
        { "Front",         0.f,   0.f  },
        { "Back",        180.f,   0.f  },
        { "Top",           0.f,  89.9f },
        { "Bottom",        0.f, -89.9f },
        { "Right",        90.f,   0.f  },
        { "Left",        270.f,   0.f  },
    };
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::SeparatorText("Camera Preset");
            for (auto& p : presets) {
                if (ImGui::MenuItem(p.label)) {
                    vp.azimuth   = p.az;
                    vp.elevation = p.el;
                }
            }
            ImGui::Separator();
            ImGui::SetNextItemWidth(110.f);
            ImGui::DragFloat("Distance", &vp.distance, 5.f, 10.f, 5000.f, "%.0f");
            ImGui::Separator();
            ImGui::MenuItem("Show Gizmo", nullptr, &vp.showGizmo);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (!m_viewportRenderer) {
        ImGui::TextDisabled("No renderer registered.");
        ImGui::TextWrapped("Call runtime().setViewportRenderer([this]{ /* draw scene */ }) in your ofApp::setup().");
        ImGui::End();
        return;
    }

    // --- FBO management -------------------------------------------------------
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 4.f) avail.x = 4.f;
    if (avail.y < 4.f) avail.y = 4.f;

    bool needsAlloc = !vp.fbo.isAllocated()
        || std::fabs(vp.lastPanelSize.x - avail.x) > 0.5f
        || std::fabs(vp.lastPanelSize.y - avail.y) > 0.5f;

    if (needsAlloc) {
        // ofFboSettings captures ofGetUsingArbTex() in its constructor, so
        // ARB must be disabled before the settings object is created.
        ofDisableArbTex();
        ofFboSettings s;
        s.width                 = static_cast<int>(avail.x);
        s.height                = static_cast<int>(avail.y);
        s.internalformat        = GL_RGBA;
        s.useDepth              = true;
        s.depthStencilAsTexture = false;
        vp.fbo.allocate(s);
        ofEnableArbTex();
        vp.lastPanelSize = { avail.x, avail.y };
        vp.cam.setNearClip(1.f);
        vp.cam.setFarClip(10000.f);
    }

    // --- Render into FBO -------------------------------------------------------
    vp.fbo.begin();
    ofClear(18, 18, 24, 255);
    ofEnableDepthTest();
    updateViewportCamera(vp);
    vp.cam.begin(ofRectangle(0, 0,
        static_cast<float>(vp.fbo.getWidth()),
        static_cast<float>(vp.fbo.getHeight())));
    m_viewportRenderer();
    // ofCamera::begin() caches the oriented projection in m_orientedProjection;
    // the gizmo reads it via vp.cam.getOrientedProjectionMatrix() — no manual
    // matrix capture needed.
    vp.cam.end();
    ofDisableDepthTest();
    vp.fbo.end();

    // --- Display ---------------------------------------------------------------
    const auto& tex = vp.fbo.getTexture();
    ImVec2 imgPosScreen = ImGui::GetCursorScreenPos();  // screen coords (for AddImage)
    if (tex.getTextureData().textureTarget == GL_TEXTURE_2D) {
        ofxImGui::AddImage(tex, glm::vec2(avail.x, avail.y));
    } else {
        ImGui::TextDisabled("Viewport texture is not GL_TEXTURE_2D.");
        ImGui::TextDisabled("textureTarget: 0x%X", tex.getTextureData().textureTarget);
    }

    // --- Gizmo overlay --------------------------------------------------------
    if (m_editMode && vp.showGizmo) {
        const ofRectangle imgScreenRect(imgPosScreen.x, imgPosScreen.y, avail.x, avail.y);
        drawGizmoInViewport(vp, imgScreenRect);
    }

    // --- Camera input ---------------------------------------------------------
    // We intentionally do NOT use an InvisibleButton here.
    // ImGuizmo::CanActivate() checks !IsAnyItemHovered(). An InvisibleButton
    // that was hovered in the previous frame keeps IsAnyItemHovered()=true at
    // the start of the next frame, so gizmo manipulation can never start.
    // Driving input via IsMouseHoveringRect + io.MouseDelta avoids creating any
    // item, so CanActivate() sees a clean state.
    const bool gizmoWantsInput = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    const ImVec2 p1 = { imgPosScreen.x + avail.x, imgPosScreen.y + avail.y };
    const bool vpHovered = ImGui::IsMouseHoveringRect(imgPosScreen, p1);

    if (vpHovered && !gizmoWantsInput) {
        ImGuiIO& io = ImGui::GetIO();

        // Orbit — left drag
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            vp.azimuth   -= io.MouseDelta.x * 0.4f;
            vp.elevation += io.MouseDelta.y * 0.4f;
            vp.elevation  = glm::clamp(vp.elevation, -89.f, 89.f);
        }

        // Pan — middle drag
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            float az = glm::radians(vp.azimuth);
            glm::vec3 right( std::cos(az), 0.f, -std::sin(az));
            glm::vec3 up   ( 0.f,          1.f,  0.f         );
            float panSpeed = vp.distance * 0.001f;
            vp.target -= right * (io.MouseDelta.x * panSpeed);
            vp.target += up    * (io.MouseDelta.y * panSpeed);
        }

        // Zoom — scroll wheel
        if (io.MouseWheel != 0.f) {
            vp.distance -= io.MouseWheel * vp.distance * 0.1f;
            vp.distance  = glm::clamp(vp.distance, 10.f, 5000.f);
        }

        ImGui::SetTooltip("Drag: orbit   Middle-drag: pan   Scroll: zoom");
    }

    ImGui::End();
}

} // namespace ofkitty
