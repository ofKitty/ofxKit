#pragma once

#include "ofMain.h"
#include "ShortcutManager.h"
#include <ofxImGui/src/ofxImGui.h>
#include <ofxImGui/src/GuiEventHelper.h>
#include <ofxImGuiStyle/src/ofxImGuiStyle.h>
#include <ofxImGuiFileDialog/src/ofxImGuiFileDialog.h>
#include <ofxImGuiTextEdit/src/ofxImGuiTextEdit.h>
#include <ofxImGuizmo/src/ofxImGuizmo.h>
#include <entt.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ofkitty {

class CodeEditorPanel;
class PathEditorPanel;

// ============================================================================
// Runtime — ofKitty edit-mode singleton
// ============================================================================
// Provides the active entt::registry and injects an ImGui inspector overlay
// into an ofBaseApp sketch via OF's event system. No inheritance required.
//
// The overlay is toggled with Cmd-E (registered via ShortcutManager).
// Sketches can register additional shortcuts through runtime().keys().
// ============================================================================

class Runtime {
public:
    struct RuntimeWindow {
        std::string name;
        std::string menuGroup {"View"};
        bool visible      {true};
        bool editModeOnly {true};  // true = only drawn in Edit mode (default); false = always drawn
        std::function<void(bool& visible)> draw;
        std::string id;   // stable ImGui id, assigned by registerWindow() if empty
    };

    // -------------------------------------------------------------------------
    // Menu bar injection
    // -------------------------------------------------------------------------
    using MenuBarCallback = std::function<void()>;
    // Register a top-level menu group (e.g. "File", "Edit") drawn in the main
    // menu bar between the app menu and the View menu, wrapped in BeginMenu/EndMenu.
    void addMenuBarGroup(const std::string& groupName, MenuBarCallback cb);
    const std::vector<std::pair<std::string, MenuBarCallback>>& menuBarGroups() const { return m_menuGroups; }

    // Register a raw menu bar callback — called directly inside BeginMainMenuBar,
    // not wrapped in its own BeginMenu. Use when the callback handles its own menus.
    void addMenuBarRawCallback(MenuBarCallback cb);

    // -------------------------------------------------------------------------
    // Post-setup hooks — called after ImGui is initialised (s_gui.setup),
    // before the first frame. Use to load fonts and apply styles.
    // -------------------------------------------------------------------------
    using PostSetupHook = std::function<void(ofxImGui::Gui&)>;
    void addPostSetupHook(PostSetupHook hook);

    // -------------------------------------------------------------------------
    // ImGui Gui accessor — needed by addons that load fonts / styles
    // -------------------------------------------------------------------------
    ofxImGui::Gui& gui() { return m_gui; }

    // -------------------------------------------------------------------------
    // Singleton access
    // -------------------------------------------------------------------------
    static Runtime& instance();
    bool isAttached() const { return m_attached; }

    Runtime(const Runtime&)            = delete;
    Runtime& operator=(const Runtime&) = delete;

    // -------------------------------------------------------------------------
    // Attach — call once in main.cpp after creating the window and app, before
    // ofRunApp(window, app).
    // -------------------------------------------------------------------------
    static void attach(shared_ptr<ofAppBaseWindow> window,
                       shared_ptr<ofBaseApp>       app);
    static void attach(shared_ptr<ofAppBaseWindow> window,
                       shared_ptr<ofBaseApp>       app,
                       entt::registry&             registry);

    // -------------------------------------------------------------------------
    // Registry
    // -------------------------------------------------------------------------
    entt::registry& registry() { return *m_registryView; }
    const entt::registry& registry() const { return *m_registryView; }

    // -------------------------------------------------------------------------
    // Keyboard shortcuts
    // -------------------------------------------------------------------------
    ShortcutManager&       keys()       { return m_shortcuts; }
    const ShortcutManager& keys() const { return m_shortcuts; }

    // -------------------------------------------------------------------------
    // Edit mode
    // -------------------------------------------------------------------------
    bool isEditMode()    const { return m_editMode; }
    void toggleEditMode();
    void setEditMode(bool on)  { m_editMode = on; }

    // -------------------------------------------------------------------------
    // Built-in panel registration
    // -------------------------------------------------------------------------
    // By default Runtime::onSetup automatically registers all built-in panels
    // (Toolbar, Scene, Properties, Code Editor, etc.).  Apps that want explicit
    // control over which panels are present can call
    //   setAutoRegisterBuiltIns(false)
    // before Runtime::attach(), then call registerBuiltInWindows() themselves
    // at the appropriate point in setup().
    bool autoRegisterBuiltIns() const { return m_autoRegisterBuiltIns; }
    void setAutoRegisterBuiltIns(bool v) { m_autoRegisterBuiltIns = v; }
    void registerBuiltInWindows();

    // -------------------------------------------------------------------------
    // Dockspace central node — passthrough vs opaque
    // -------------------------------------------------------------------------
    // By default the central dockspace node is transparent (PassthruCentralNode),
    // which lets the raw OpenGL scene show through the gap between docked panels.
    // Apps that render exclusively inside ImGui windows (no raw 3-D scene) should
    // call setPassthruCentralNode(false) in setup() so the central area is an
    // opaque, dockable surface instead of a hole.
    bool passthruCentralNode() const { return m_passthruCentralNode; }
    void setPassthruCentralNode(bool passthru) { m_passthruCentralNode = passthru; }

    // -------------------------------------------------------------------------
    // Rulers
    // -------------------------------------------------------------------------
    bool showRulers()         const { return m_showRulers; }
    void setShowRulers(bool v)      { m_showRulers = v; }
    void toggleRulers()             { m_showRulers = !m_showRulers; }

    // -------------------------------------------------------------------------
    // App preferences (read-only access for callers such as ruler overlays)
    // -------------------------------------------------------------------------
    // App-wide openFrameworks preferences (circle resolution, FPS, bg colour, …)
    struct AppPrefs {
        bool    initialized    {false};
        int     circleRes      {22};
        int     targetFps      {60};
        bool    vsync          {true};
        bool    backgroundAuto {true};
        ofColor bgColor        {18, 18, 24, 255};
        float   lineWidth      {1.f};
        bool    smoothLighting {true};
        bool    depthTest      {false};
        int     logLevel       {1};   // 0=Verbose 1=Notice 2=Warning 3=Error 4=Fatal 5=Silent
        float   rulerScale     {1.0f};

        // Unit shown on per-panel rulers (full-window rulers always show pixels).
        // Custom lets callers override pixPerUnit / unitLabel without touching prefs.
        enum class RulerUnit { Pixels, Millimetres, Custom };
        RulerUnit   rulerUnit       {RulerUnit::Pixels};

        // Margin overlay (drawn by panels via ofkitty::drawMarginRect).
        // Colour and visibility are global so every example/panel using the
        // helper renders margins consistently. Sizes (e.g. paper margin in mm)
        // remain per-document and live with the engine, not here.
        bool    showMarginRect {true};
        ofColor marginColor    {180, 90, 220, 200};   ///< default purple

        // When true, Tab / Ctrl+E also hides the menu bar and status bar
        // (not just the edit-mode windows) when leaving edit mode.
        bool    hideAllUI      {false};
    };

    const AppPrefs& appPrefs() const { return m_prefs; }

    // ---- Margin overlay (AppPrefs-backed; setters persist) ----
    bool           showMarginRect()                     const { return m_prefs.showMarginRect; }
    void           setShowMarginRect(bool v)                  { m_prefs.showMarginRect = v; saveAppPrefs(); }
    const ofColor& marginColor()                        const { return m_prefs.marginColor; }
    void           setMarginColor(const ofColor& c)           { m_prefs.marginColor = c;    saveAppPrefs(); }

    // -------------------------------------------------------------------------
    // Viewport windows — independent FBO-based scene views
    // -------------------------------------------------------------------------
    // Each panel has its own camera (orbit/pan/zoom via ImGui mouse input) and
    // its own FBO. All panels share a single renderer callback that draws the
    // scene without setting up a camera — the Runtime wraps it.
    //
    // Example — quad-view 3-D editor:
    //
    //   runtime().setViewportRenderer([this] {
    //       if (m_showGrid) drawGrid();
    //       for (auto [e, nd, mesh, render] :
    //            m_registry.view<ecs::node_component,
    //                            ecs::mesh_component,
    //                            ecs::render_component>().each()) {
    //           if (!render.visible) continue;
    //           ofPushMatrix();
    //           ofMultMatrix(nd.node.getGlobalTransformMatrix());
    //           ofSetColor(mesh.color);
    //           mesh.m_mesh.draw();
    //           ofPopMatrix();
    //       }
    //   });
    //
    //   runtime().addViewportWindow("Perspective");          // az=30, el=20 (default)
    //   auto* top = runtime().addViewportWindow("Top");
    //   top->elevation = 89.9f;  top->azimuth = 0.f;
    //   auto* front = runtime().addViewportWindow("Front");
    //   front->azimuth = 0.f;    front->elevation = 0.f;
    //   auto* right = runtime().addViewportWindow("Right");
    //   right->azimuth = 90.f;   right->elevation = 0.f;
    //

    // Self-contained state for one secondary viewport panel.
    // The Runtime owns the lifetime; addViewportWindow() returns a raw pointer
    // for adjusting the initial camera preset before the first frame.
    struct ViewportInstance {
        std::string title     = "Scene View";
        float       azimuth   = 30.f;
        float       elevation = 20.f;
        float       distance  = 500.f;
        glm::vec3   target    = {0.f, 0.f, 0.f};
        bool        showGizmo = true;
        bool        showRulers = false;
        ofFbo       fbo;
        ofCamera    cam;
        glm::vec2   lastPanelSize = {0.f, 0.f};
    };

    using ViewportRenderer = std::function<void()>;
    void setViewportRenderer(ViewportRenderer fn);
    void clearViewportRenderer();

    /// Create a named viewport panel. Pass an empty string (default) to
    /// auto-name: "Scene View", "Scene View 2", "Scene View 3", …
    /// Returns a pointer to set the initial camera angle. Each title must be
    /// unique — it becomes the ImGui window title, shown under View menu.
    ViewportInstance* addViewportWindow(std::string title = "");

    /// Remove a viewport panel by title. No-op if not found.
    void removeViewportWindow(const std::string& title);

    // -------------------------------------------------------------------------
    // Built-in window registration
    // -------------------------------------------------------------------------
    // By default all built-in windows are registered automatically at startup.
    // Call any of these in setup() or main.cpp before ofRunApp() to override.
    //
    //   runtime().disableBuiltInWindows();          // register none
    //   runtime().enableBuiltInWindows();           // Scene + Properties only
    //   runtime().enableBuiltInWindow("Scene");     // one at a time (additive)
    //   runtime().enableBuiltInWindow("Toolbar");
    //   runtime().enableAllBuiltInWindows();        // all (explicit default)
    //
    // Accepted names: "Toolbar", "Scene", "Properties", "Shortcuts",
    //                 "Preferences", "Code Editor", "Path Editor"
    // Stable IDs (e.g. "ofxkit.window.scene") are also accepted.
    // -------------------------------------------------------------------------

    /// Register none of the built-in windows.
    void disableBuiltInWindows();

    /// Register one specific built-in window by display name or stable ID.
    /// Implicitly disables auto-registration of all others.
    void enableBuiltInWindow(const std::string& nameOrId);

    /// Register the standard set: Scene (left) and Properties (right).
    void enableBuiltInWindows();

    /// Register all built-in windows (same as the default behaviour).
    void enableAllBuiltInWindows();

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------
    entt::entity selected() const       { return m_selected; }
    void         select(entt::entity e) { m_selected = e; }

    // -------------------------------------------------------------------------
    // UI identity
    // -------------------------------------------------------------------------
    const std::string& appName() const { return m_appName; }
    void setAppName(std::string name);

    // -------------------------------------------------------------------------
    // ImGui layout file
    // -------------------------------------------------------------------------
    /// Set a custom path for ImGui's layout ini file. The Runtime owns the
    /// string so its lifetime safely outlives the ImGui context, which
    /// otherwise dereferences a dangling pointer at shutdown — producing
    /// garbled-named layout files in the working directory.
    void setImGuiIniPath(std::string path);

    // -------------------------------------------------------------------------
    // UI scale (HiDPI / 4K)
    // -------------------------------------------------------------------------
    /// Set the global UI scale factor. 1.0 = native, 2.0 = double size for
    /// 4K screens, etc. Applies live: rescales all widgets and the global
    /// font scale. Auto-detected from the primary monitor at startup; users
    /// can override via the View ▸ UI Scale menu and the value persists in
    /// `bin/data/ofxKit/uiScale.json`.
    void setUIScale(float scale);
    float uiScale() const { return m_uiScale; }

    /// Auto-detect the OS UI scale factor for the primary monitor. Returns
    /// 1.0 if detection fails (non-GLFW backend, headless, etc.).
    static float detectUIScale();

    // -------------------------------------------------------------------------
    // Theme
    // -------------------------------------------------------------------------
    enum class Theme {
        Dark,          ///< ofxImGuiStyle dark preset (default)
        Light,         ///< ofxImGuiStyle light preset
        Classic,       ///< ofxImGuiStyle classic preset
    };

    /// Set the active ImGui theme. ofxImGuiStyle owns the reusable style
    /// mechanics; ofxKit persists this editor preference and reapplies scale.
    void setTheme(Theme theme);
    Theme theme() const { return m_theme; }

    // -------------------------------------------------------------------------
    // Toolbar items
    // -------------------------------------------------------------------------
    // A single entry in the floating Toolbar window. Tools, addons and sketches
    // register items here so they appear as icon buttons in a shared panel.
    //
    // Example (with Font Awesome icons loaded):
    //
    //   runtime().registerToolbarItem({
    //       "myapp.select",
    //       ICON_FA_MOUSE_POINTER,
    //       "Select (V)",
    //       "tools",                                 // group
    //       [&]{ currentTool = SELECT; },            // onSelect
    //       [&]{ return currentTool == SELECT; }     // isActive
    //   });
    //
    // Items are drawn top-to-bottom. Those with the same non-empty `group`
    // string are clustered together; a separator is inserted between groups.
    struct ToolbarItem {
        std::string           id;          // unique id (e.g. "myaddon.pan")
        const char*           icon;        // FA glyph literal or short text label
        std::string           tooltip;     // shown on hover
        std::string           group;       // group name for separator clustering
        std::function<void()> onSelect;    // called on click
        std::function<bool()> isActive;    // returns true → button is highlighted
    };

    void registerToolbarItem(ToolbarItem item);
    bool unregisterToolbarItem(const std::string& id);
    std::vector<ToolbarItem>& toolbarItems() { return m_toolbarItems; }
    const std::vector<ToolbarItem>& toolbarItems() const { return m_toolbarItems; }

    // -------------------------------------------------------------------------
    // File dialogs — powered by ofxImGuiFileDialog
    // -------------------------------------------------------------------------
    // Open a file-open or file-save dialog. The dialog is rendered every frame
    // inside the ImGui block; `onConfirm` is called with the chosen path when
    // the user clicks OK. Multiple dialogs may be queued with distinct keys.
    //
    // Example:
    //   runtime().openFileDialog("open_img", "Open Image", ".png,.jpg,.gif",
    //       [this](const std::string& path) { loadImage(path); });
    //
    //   runtime().saveFileDialog("save_txt", "Save Script", ".glsl",
    //       "untitled.glsl",
    //       [this](const std::string& path) { saveScript(path); });
    // -------------------------------------------------------------------------
    void openFileDialog(const std::string& key,
                        const std::string& title,
                        const std::string& filters,
                        std::function<void(const std::string& path)> onConfirm);

    void saveFileDialog(const std::string& key,
                        const std::string& title,
                        const std::string& filters,
                        const std::string& defaultFileName,
                        std::function<void(const std::string& path)> onConfirm);

    // -------------------------------------------------------------------------
    // 3-D transform gizmo — powered by ofxImGuizmo
    // -------------------------------------------------------------------------
    // Register the camera used for the gizmo overlay over the main OF scene.
    // The gizmo is drawn over the selected entity whenever edit mode is active.
    // Pass nullptr to disable the main-scene gizmo.
    //
    // If the camera is an ofEasyCam (detected automatically via RTTI), the
    // Runtime also manages its mouse input so it doesn't fight the gizmo or
    // ImGui windows.  No captureSceneView() call is needed — the oriented
    // projection matrix is read from ofCamera::getOrientedProjectionMatrix()
    // which is cached automatically by ofCamera::begin().
    //
    // Minimal setup:
    //   void ofApp::setup() { runtime().setSceneCamera(&m_cam); }
    //   void ofApp::draw()  { m_cam.begin(); drawScene(); m_cam.end(); }
    // -------------------------------------------------------------------------
    enum class GizmoOperation { Translate, Rotate, Scale, Universal };
    enum class GizmoMode      { World, Local };

    void setSceneCamera(ofCamera* cam);
    void clearSceneCamera();
    ofCamera* sceneCamera() const       { return m_sceneCamera; }

    // Returns true when the mouse is over the main OF scene and ImGui is not
    // consuming it (no panel focused/hovered, gizmo not active).
    // Useful for apps that want to do their own picking without clashing with
    // ImGui or the gizmo.
    bool isSceneHovered() const;

    // Captures the current OpenGL view + projection matrices.
    // No longer needed when using ofCamera::begin() — the oriented projection
    // is cached automatically.  Kept for backward compatibility.
    void captureSceneView();

    // Returns true while ImGuizmo is hovered or actively being dragged.
    bool isGizmoActive() const;

    void          setGizmoOperation(GizmoOperation op)   { m_gizmoOp   = op;   }
    GizmoOperation gizmoOperation()  const               { return m_gizmoOp;   }
    void          setGizmoMode(GizmoMode mode)           { m_gizmoMode = mode; }
    GizmoMode     gizmoMode()        const               { return m_gizmoMode; }

    // -------------------------------------------------------------------------
    // Code Editor — powered by ofxImGuiTextEdit
    // -------------------------------------------------------------------------
    // The Code Editor is a built-in dockable window (View > Code Editor).
    // It supports syntax highlighting for C++, GLSL, Lua, Python, G-code and more.
    //
    // Seed the editor with text — optionally set language in one call:
    //   runtime().codeEditorSetText(gcodeStr, TextEditor::LanguageDefinitionId::Gcode);
    //   runtime().codeEditorSetLanguage(TextEditor::LanguageDefinitionId::Glsl);
    // -------------------------------------------------------------------------
    void          codeEditorSetText(const std::string& text,
                                    TextEditor::LanguageDefinitionId lang
                                        = TextEditor::LanguageDefinitionId::None);
    std::string   codeEditorGetText() const;
    void          codeEditorSetLanguage(TextEditor::LanguageDefinitionId lang);

    // -------------------------------------------------------------------------
    // Path Editor — powered by ofxImGuiVectorEditor
    // -------------------------------------------------------------------------
    // The Path Editor is a built-in dockable window (View > Path Editor) that
    // edits bezier paths. When the selected entity has a path_component the
    // editor loads it automatically. Changes are written back on Apply.
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Windows
    // -------------------------------------------------------------------------
    RuntimeWindow* registerWindow(RuntimeWindow window);
    RuntimeWindow* findWindow(const std::string& name);
    const RuntimeWindow* findWindow(const std::string& name) const;
    bool setWindowVisible(const std::string& name, bool visible);
    std::vector<RuntimeWindow>& windows() { return m_windows; }
    const std::vector<RuntimeWindow>& windows() const { return m_windows; }

    // -------------------------------------------------------------------------
    // Default dock layout — optional extras
    // -------------------------------------------------------------------------
    /// ImGui window titles as passed to ImGui::Begin (including any ###id
    /// suffix), docked into the left split of buildDefaultDockLayout() next to
    /// the Scene panel. Call from ofApp::setup() after registerWindow(). Only
    /// applied when the default layout is built (no imgui.ini on first run, or
    /// after View ▸ Reset Layout).
    void addDefaultLayoutLeftDock(std::string imguiWindowTitle);

    /// Same as addDefaultLayoutLeftDock, but for the right split (Properties /
    /// Shortcuts / Preferences stack). Call after registerWindow().
    void addDefaultLayoutRightDock(std::string imguiWindowTitle);

    /// Same as addDefaultLayoutLeftDock, but docks the window into the central
    /// node (the area between the left and right splits). When at least one
    /// window is seeded here the central node's NoTabBar flag is cleared so
    /// the tab bar / title bar is visible. Call after registerWindow().
    void addDefaultLayoutCenterDock(std::string imguiWindowTitle);

    // -------------------------------------------------------------------------
    // Preference pages — two-pane category/page Preferences window
    // -------------------------------------------------------------------------
    // Register a page that appears in the Preferences window. Pages are grouped
    // by a slash-separated category path (e.g. "openFrameworks/Rendering").
    // The draw callback is called when the page is selected; it should emit
    // ImGui widgets directly, no Begin/End needed.
    //
    // Built-in pages ("openFrameworks/General", …) are always registered first.
    // Addons should register in their setup() or kit-init helper:
    //
    //   ofkitty::runtime().registerPreferencePage({
    //       "MyAddon/Settings",                         // category path
    //       "MyAddon",                                  // display name
    //       "myapp.prefs.myaddon",                      // unique id
    //       [&]() {                                     // draw
    //           ImGui::Checkbox("Enable feature", &myFeature);
    //       }
    //   });
    // -------------------------------------------------------------------------
    struct PreferencePage {
        std::string           category;    // category path, e.g. "openFrameworks"
        std::string           name;        // leaf label shown in the tree
        std::string           id;          // unique id (e.g. "ofxkit.prefs.general")
        std::function<void()> draw;        // called when the page is selected
    };

    void registerPreferencePage(PreferencePage page);
    bool unregisterPreferencePage(const std::string& id);
    const std::vector<PreferencePage>& preferencePages() const { return m_preferencePages; }

    // -------------------------------------------------------------------------
    // Status bar items
    // -------------------------------------------------------------------------
    // Register a widget rendered inside the bottom status bar.
    // Items with the same non-empty `group` are clustered with a separator.
    // The draw callback should emit compact ImGui widgets (text, small buttons);
    // it is called inside an already-begun menu-bar context.
    //
    // Example:
    //
    //   ofkitty::runtime().registerStatusItem({
    //       "myaddon.statusbar.fps",               // unique id
    //       "My Addon",                            // group
    //       true,                                  // visible by default
    //       [&]() {                                // draw
    //           ImGui::Text("FPS: %.0f", ofGetFrameRate());
    //       }
    //   });
    // -------------------------------------------------------------------------
    struct StatusItem {
        std::string           id;          // unique id (e.g. "ofxkit.status.fps")
        std::string           group;       // separator clustering label
        bool                  visible {true};
        std::function<void()> draw;        // called inside the status bar
    };

    void registerStatusItem(StatusItem item);
    bool unregisterStatusItem(const std::string& id);
    std::vector<StatusItem>& statusItems() { return m_statusItems; }
    const std::vector<StatusItem>& statusItems() const { return m_statusItems; }

    // -------------------------------------------------------------------------
    // Component registry
    // -------------------------------------------------------------------------
    // Registers a component type so it appears in the "Add Component" picker
    // in the Properties panel.  The template shorthand generates has / remove
    // automatically from T; supply a custom add only when the default
    // emplace<T>(entity) needs extra initialisation (e.g. mesh_component needs
    // rebuild()).
    //
    // Addons register their own components in setup() or a kit-init helper:
    //
    //   ofkitty::runtime().registerComponent<MyComp>(
    //       "My Component", "My Category",
    //       [](entt::registry& r, entt::entity e) {
    //           r.emplace<MyComp>(e).initialise();
    //       });
    //
    // Shipped-kit picker rows come from ecs::registerKitComponentMenu(...)
    // (Runtime::registerBuiltInComponents() in onSetup() forwards each row here).
    // -------------------------------------------------------------------------

    struct ComponentDescriptor {
        std::string  name;
        std::string  category;     // groups entries in the picker UI
        std::string  description;  // tooltip (optional)

        std::function<bool(entt::registry&, entt::entity)> has;
        std::function<void(entt::registry&, entt::entity)> add;
        std::function<void(entt::registry&, entt::entity)> remove;
    };

    /// Full-control registration — supply all three lambdas explicitly.
    void registerComponent(ComponentDescriptor desc);

    /// Template shorthand.  has() and remove() are generated from T.
    /// Provide add only when default emplace<T>(entity) is not sufficient.
    template<typename T>
    void registerComponent(
            const std::string& name,
            const std::string& category,
            std::function<void(entt::registry&, entt::entity)> add = {})
    {
        ComponentDescriptor d;
        d.name     = name;
        d.category = category;
        d.has    = [](entt::registry& r, entt::entity e) { return r.all_of<T>(e); };
        d.add    = add ? std::move(add)
                       : [](entt::registry& r, entt::entity e) {
                             if (!r.all_of<T>(e)) r.emplace<T>(e);
                         };
        d.remove = [](entt::registry& r, entt::entity e) { r.remove<T>(e); };
        registerComponent(std::move(d));
    }

    const std::vector<ComponentDescriptor>& componentDescriptors() const;
    /// Returns unique category names in registration order.
    std::vector<std::string>                componentCategories()   const;

private:
    Runtime() = default;

    void attachInternal(shared_ptr<ofAppBaseWindow> window,
                        shared_ptr<ofBaseApp>       app,
                        entt::registry&             registry);

    void onSetup(ofEventArgs&);
    void onUpdate(ofEventArgs&);
    void onDraw(ofEventArgs&);
    void onExit(ofEventArgs&);
    void onShortcutCaptureBeforeApp(ofKeyEventArgs&);
    void onKeyPressed(ofKeyEventArgs&);

    void drawOverlay();
    void renderMainMenuBar();
    void drawSceneWindow(bool& visible);
    void drawPropertiesWindow(bool& visible);
    void drawShortcutsWindow(bool& visible);
    void drawToolbarWindow(bool& visible);
    void ensureAppName();
    void applyUIScale();
    void loadUIScalePref();
    void saveUIScalePref();

    void applyTheme();
    void loadThemePref();
    void saveThemePref();

    void registerBuiltInComponents();
    void drawStatusBar();
    void buildDefaultDockLayout(ImGuiID dockId);

    void drawPreferencesWindow(bool& visible);
    void drawRulers();
    void loadAppPrefs();
    void saveAppPrefs();

    void registerBuiltInPreferencePages();
    void drawPreferencePageList();
    void drawPreferencePageContent();
    void registerBuiltInStatusItems();

    // Preference sub-pages
    void drawPrefsAppearance();
    void drawPrefsGeneral();
    void drawPrefsRendering();
    void drawPrefsLogging();
    void drawPrefsStatusBar();

    void drawViewportWindow(ViewportInstance& vp, bool& visible);
    void updateViewportCamera(ViewportInstance& vp);

    // Tool windows
    void drawCodeEditorWindow(bool& visible);
    void drawPathEditorWindow(bool& visible);
    void processFileDialogs();
    void drawGizmoOverlay();
    void drawGizmoInViewport(ViewportInstance& vp, const ofRectangle& imgScreenRect);

    bool             m_editMode  {false};
    bool             m_autoRegisterBuiltIns {true};
    std::unordered_set<std::string> m_requestedBuiltInWindows;
    bool             m_passthruCentralNode {true};
    int              m_toggleEditLastFrame {-1};
    bool             m_attached  {false};
    bool             m_skipShortcutDispatch {false};
    bool             m_builtInWindowsRegistered {false};
    entt::entity     m_selected  {entt::null};
    entt::registry   m_registry;
    entt::registry*  m_registryView {&m_registry};
    ShortcutManager  m_shortcuts;
    std::string      m_appName;
    // m_imguiIniPath MUST be declared before m_gui so its destructor runs
    // after m_gui's — ImGui flushes its layout on shutdown and would
    // otherwise dereference a dangling const char*.
    std::string      m_imguiIniPath;
    std::vector<RuntimeWindow> m_windows;
    std::vector<std::string>   m_defaultLayoutExtraLeftDocks;
    std::vector<std::string>   m_defaultLayoutExtraRightDocks;
    std::vector<std::string>   m_defaultLayoutExtraCenterDocks;
    // Visibility states loaded from disk before windows are registered.
    // registerWindow() applies the saved state so addon windows also restore.
    std::unordered_map<std::string, bool> m_savedWindowVisibility;
    std::vector<ToolbarItem>   m_toolbarItems;
    std::vector<std::pair<std::string, MenuBarCallback>> m_menuGroups;
    std::vector<MenuBarCallback> m_menuBarRawCallbacks;
    std::vector<PostSetupHook>  m_postSetupHooks;

    // UI scale & theme state. ofxImGuiStyle owns the unscaled baseline; ofxKit
    // only persists the chosen scale/theme and applies editor-window policy.
    float            m_uiScale     {1.0f};
    bool             m_uiScaleSet  {false};
    bool             m_themeSet    {false};
    Theme            m_theme       {Theme::Dark};

    ofxImGuiStyle    m_style;                           // fonts, themes, and style scaling
    ofxImGui::Gui    m_gui;

    std::vector<ComponentDescriptor> m_components;
    bool             m_builtInComponentsRegistered {false};

    AppPrefs         m_prefs;
    bool             m_showRulers         {false};
    bool             m_defaultLayoutBuilt {false};
    bool             m_layoutResetPending {false};

    std::vector<PreferencePage> m_preferencePages;
    bool             m_builtInPreferencePagesRegistered {false};
    std::string      m_selectedPreferencePage;          // id of selected page

    std::vector<StatusItem> m_statusItems;
    bool             m_builtInStatusItemsRegistered {false};

    // Viewport panels — one entry per addViewportWindow() call.
    // Stored as unique_ptr so the heap address is stable even if the vector
    // reallocates; lambdas capture the raw pointer safely.
    ViewportRenderer m_viewportRenderer;
    std::vector<std::unique_ptr<ViewportInstance>> m_viewportInstances;

    // ── Tool state ────────────────────────────────────────────────────────────
    // File dialogs
    std::unordered_map<std::string, std::function<void(const std::string&)>> m_fileDialogCbs;

    // Gizmo — main scene camera
    ofCamera*          m_sceneCamera    {nullptr};
    ofEasyCam*         m_sceneEasyCam  {nullptr};  // non-null when camera is an ofEasyCam (RTTI)
    ofxImGui::Viewport m_sceneViewport;             // tracks WantCaptureMouse for the scene
    GizmoOperation     m_gizmoOp        {GizmoOperation::Translate};
    GizmoMode          m_gizmoMode      {GizmoMode::World};
    // Legacy manual capture (kept for backward compat — prefer getOrientedProjectionMatrix()).
    glm::mat4          m_capturedView       {1.f};
    glm::mat4          m_capturedProj       {1.f};
    bool               m_sceneViewCaptured  {false};

    // Editors (implementations under `src/panels/`)
    std::unique_ptr<CodeEditorPanel> m_codeEditor;
    std::unique_ptr<PathEditorPanel> m_pathEditor;

    ofxImGui::GuiEventHelper m_eventHelper;
};

// Convenience free function
inline Runtime& runtime() { return Runtime::instance(); }

} // namespace ofkitty
