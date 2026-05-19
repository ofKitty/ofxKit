#pragma once

#include "ofMain.h"
#include "RulerUtil.h"
#include "ShortcutManager.h"
#include <ofxImGui/src/ofxImGui.h>
#include <ofxImGui/src/GuiEventHelper.h>
#include <ofxImGuiStyle/src/ImTheme.h>
#include <ofxImGuiStyle/src/ImThemeRegistry.h>
#include <ofxImGuiStyle/src/ImFonts.h>
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

    // Toggle only the edit-mode windows, leaving the menu bar and status bar
    // untouched. Bound to Tab by default. Equivalent to the old toggleEditMode().
    void toggleEditMode();

    // Toggle the entire UI at once: edit-mode windows + menu bar + status bar.
    // Bound to Ctrl/Cmd+E by default.  When turning off, chrome is hidden too;
    // when turning on, chrome is restored together with the windows.
    void toggleAllUI();

    void setEditMode(bool on)  { m_editMode = on; }

    // -------------------------------------------------------------------------
    // Chrome visibility (menu bar + status bar)
    // -------------------------------------------------------------------------
    // Normally controlled together with edit mode via toggleAllUI() / Ctrl+E.
    // Call directly when you need to hide or show the chrome independently.
    bool isChromeHidden()      const { return m_hideChrome; }
    void setHideChrome(bool v)       { m_hideChrome = v; }

    // -------------------------------------------------------------------------
    // Built-in panel registration
    // -------------------------------------------------------------------------
    // By default Runtime registers NO built-in panels (opt-in).  Call any of
    // these before or after Runtime::attach() to add the panels you need:
    //
    //   setAutoRegisterBuiltIns(true)        // legacy: register everything
    //   runtime().enableBuiltInWindow("Scene")   // add one panel
    //   runtime().enableBuiltInWindows()         // Scene + Properties
    //   runtime().enableAllBuiltInWindows()      // Toolbar, Scene, Properties, …
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

        // Controls what Ctrl/Cmd+E does (configurable in Appearance preferences).
        // true  → Ctrl+E hides the entire UI: windows + menu bar + status bar.
        // false → Ctrl+E hides windows only (same as Tab).
        // Tab always hides windows only regardless of this setting.
        bool    hideAllUI      {true};

        // ---- Audio device / stream format ----
        // Master volume is intentionally NOT stored here: it belongs to whatever
        // addon owns the audio graph (e.g. ofxBapp::AppSettings). Register a
        // PrefSerializer if you want it to share appPrefs.json.
        std::string audioOutputDevice  {};   ///< empty = system default
        std::string audioInputDevice   {};   ///< empty = no input
        int         audioSampleRate    {44100};
        int         audioBufferSize    {512};

        // Margin overlay and MIDI port routing used to live here. They are now
        // owned by their respective addons (see addons/ofxMidiKit/src/ofxMidiKit.h
        // and any ofxRulerKit / ofxPlotter consumer). Addons that want their
        // fields persisted into appPrefs.json should call
        // Runtime::registerPrefSerializer(...).
    };

    const AppPrefs& appPrefs() const { return m_prefs; }

    // ---- Audio preferences ----
    // Device / sample-rate / buffer changes require a stream restart; register
    // a callback so the owning app (or an addon like ofxAcidBox) can rebuild
    // its ofSoundStream from the saved prefs. Master volume is intentionally
    // not exposed here — it belongs to whichever addon owns the audio graph.
    int         audioSampleRate() const { return m_prefs.audioSampleRate; }
    int         audioBufferSize() const { return m_prefs.audioBufferSize; }
    const std::string& audioOutputDevice() const { return m_prefs.audioOutputDevice; }
    const std::string& audioInputDevice()  const { return m_prefs.audioInputDevice; }

    // Call this once at startup so the Audio prefs page can restart your stream.
    void setAudioRestartCallback(std::function<void()> cb) { m_audioRestartCallback = std::move(cb); }

    // ---- Test signal ---------------------------------------------------------
    // Call this from your ofApp::audioOut() to mix in the tone/noise test signal
    // that can be toggled in Audio Preferences. Safe to call every frame — it
    // is a no-op when neither signal type is active.
    //
    //   void ofApp::audioOut(ofSoundBuffer& buf) {
    //       // ... your synth fill ...
    //       runtime().mixTestSignal(buf);
    //   }
    void mixTestSignal(ofSoundBuffer& buf);

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
    // The Runtime owns the lifetime; addViewportWindow() / addViewportWindow2D()
    // return a raw pointer so you can configure the instance before the first frame.
    struct ViewportInstance {

        // ---- Mode -----------------------------------------------------------
        enum class Mode { Orbit3D, Ortho2D };
        Mode mode = Mode::Orbit3D;

        // ---- Shared ---------------------------------------------------------
        std::string title        = "Scene View";
        bool        showRulers   = false;
        bool        editModeOnly = true;  // set false before addViewportWindow[2D]()

        // ---- Orbit3D (Mode::Orbit3D only) -----------------------------------
        float       azimuth   = 30.f;
        float       elevation = 20.f;
        float       distance  = 500.f;
        glm::vec3   target    = {0.f, 0.f, 0.f};
        bool        showGizmo = true;

        // ---- Ortho2D (Mode::Ortho2D only) -----------------------------------
        // Content size in the units used by your renderer (mm, px, etc.).
        glm::vec2   contentSize = {200.f, 200.f};
        std::string contentUnit = "px";    // shown on ruler tick labels
        glm::vec2   pan2D       = {};      // canvas-pixel offset from fitted centre
        float       zoom2D      = 1.f;     // multiplier on the computed fit zoom

        // Optional guide set — pass a pointer so the ruler strip creates and
        // moves draggable guides.  nullptr = no guides.
        GuideSet*   guides = nullptr;

        // Ortho2D renderer callbacks:
        //
        //   headerDraw()         ImGui context, runs before the canvas area.
        //                        Return true to suppress the canvas entirely
        //                        (use for progress bars, "no content" messages).
        //
        //   renderer2D()         OF context, called inside fbo.begin()…end().
        //                        Origin = content top-left, 1 unit = 1 content
        //                        unit, Y-DOWN.  Draw paths, images, etc. here.
        //
        //   overlayDraw(vp)      ImGui context, called after the FBO image is
        //                        displayed.  Use toScreen() / toContent() for
        //                        hit-testing and ImDrawList overlays.
        // menuBarDraw()       ImGui context, called inside BeginMenuBar / EndMenuBar
        //                        before the built-in View menu.  Add BeginMenu /
        //                        SmallButton widgets here for app-specific controls.
        std::function<void()>                    menuBarDraw;
        std::function<bool()>                    headerDraw;
        std::function<void()>                    renderer2D;
        std::function<void(ViewportInstance&)>   overlayDraw;

        // ---- Coordinate converters (Ortho2D — updated each frame) ----------
        // screen = (ox + content.x * zoom,  oy + content.y * zoom)
        ImVec2    toScreen (float cx, float cy) const noexcept
            { return { _ox + cx * _zoom, _oy + cy * _zoom }; }
        glm::vec2 toContent(float sx, float sy) const noexcept
            { return { (_zoom > 0.f) ? (sx - _ox) / _zoom : 0.f,
                       (_zoom > 0.f) ? (sy - _oy) / _zoom : 0.f }; }
        float     contentZoom()     const noexcept { return _zoom; }
        ImVec2    canvasOriginPx()  const noexcept { return { _canvasOx, _canvasOy }; }
        float     canvasW()         const noexcept { return _canvasW; }
        float     canvasH()         const noexcept { return _canvasH; }
        // True when the mouse is inside the canvas area and ImGui is not capturing it.
        bool      isCanvasHovered() const noexcept { return _canvasHovered; }

        // ---- Internal (managed by Runtime — do not modify) -----------------
        ofFbo     fbo;
        ofCamera  cam;
        glm::vec2 lastPanelSize = {};
        float _ox = 0.f, _oy = 0.f, _zoom = 1.f;
        float _canvasOx = 0.f, _canvasOy = 0.f;
        float _canvasW  = 0.f, _canvasH  = 0.f;
        bool  _canvasHovered = false;
    };

    using ViewportRenderer = std::function<void()>;
    void setViewportRenderer(ViewportRenderer fn);
    void clearViewportRenderer();

    /// Create a named 3-D orbit viewport panel.
    ViewportInstance* addViewportWindow(std::string title = "");

    /// Create a named 2-D orthographic canvas panel.
    ///
    ///   auto* vp = runtime().addViewportWindow2D("Preview", {210.f, 297.f}, "mm");
    ///   vp->renderer2D  = [this]{ drawContent(); };
    ///   vp->overlayDraw = [this](auto& vp){ drawHandles(vp); };
    ///   vp->guides      = &m_guides;
    ///
    /// Provides scroll-to-zoom around cursor, middle-mouse / Alt+LMB pan,
    /// double-click-to-fit, optional rulers, and toScreen() / toContent()
    /// coordinate converters.  editModeOnly defaults to false — the panel
    /// remains visible outside edit mode.
    ViewportInstance* addViewportWindow2D(std::string  title,
                                          glm::vec2    contentSize,
                                          std::string  contentUnit  = "px",
                                          bool         editModeOnly = false);

    /// Remove a viewport panel by title. No-op if not found.
    void removeViewportWindow(const std::string& title);

    // -------------------------------------------------------------------------
    // Built-in window registration
    // -------------------------------------------------------------------------
    // Built-in windows are opt-in by default — none are registered unless you
    // explicitly ask for them.  Call any of these in setup() or main.cpp:
    //
    //   runtime().enableBuiltInWindow("Toolbar");   // one at a time (additive)
    //   runtime().enableBuiltInWindow("Scene");
    //   runtime().enableBuiltInWindows();           // Scene + Properties only
    //   runtime().enableAllBuiltInWindows();        // all built-in panels
    //   runtime().disableBuiltInWindows();          // reset to default (none)
    //
    // Accepted names: "Toolbar", "Scene", "Properties", "Shortcuts",
    //                 "Preferences", "Code Editor", "Path Editor"
    // Stable IDs (e.g. "ofxkit.window.scene") are also accepted.
    // -------------------------------------------------------------------------

    /// Register none of the built-in windows (same as the default).
    void disableBuiltInWindows();

    /// Register one specific built-in window by display name or stable ID.
    void enableBuiltInWindow(const std::string& nameOrId);

    /// Register the standard set: Scene (left) and Properties (right).
    void enableBuiltInWindows();

    /// Register all built-in windows.
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
    // Persisted-data location
    // -------------------------------------------------------------------------
    /// Built-in JSON files (appPrefs / theme / uiScale / shortcuts /
    /// imgui.ini) live under `ofToDataPath(dataSubdir() + "<file>")`.
    /// Default subdir is empty, so they sit at the top of `bin/data/`.
    /// Set a non-empty subdir before `Runtime::attach()` (e.g. `"ofKitty"`)
    /// to group them in a folder. Leading/trailing slashes are normalised.
    void               setDataSubdir(const std::string& subdir);
    const std::string& dataSubdir() const { return m_dataSubdir; }

    /// Build a `bin/data/...` path that respects `setDataSubdir()`. Use
    /// this if your addon wants to drop a JSON next to ofxKit's own files.
    std::string        dataPath(const std::string& filename) const;

    // -------------------------------------------------------------------------
    // UI scale (HiDPI / 4K)
    // -------------------------------------------------------------------------
    /// Set the global UI scale factor. 1.0 = native, 2.0 = double size for
    /// 4K screens, etc. Applies live: rescales all widgets and the global
    /// font scale. Auto-detected from the primary monitor at startup; users
    /// can override via the View ▸ UI Scale menu and the value persists in
    /// `bin/data/uiScale.json` (or `dataSubdir()/uiScale.json`).
    void setUIScale(float scale);
    float uiScale() const { return m_uiScale; }

    /// Auto-detect the OS UI scale factor for the primary monitor (delegates
    /// to `ImTheme::DetectOsScale()`). Returns 1.0 if detection fails
    /// (non-GLFW backend, headless, etc.).
    static float detectUIScale();

    // -------------------------------------------------------------------------
    // Theme
    // -------------------------------------------------------------------------
    //
    // ofxKit does not own the theme registry. Theme presets live in
    // ImTheme / ImThemeRegistry (ofxImGuiStyle). The Runtime only:
    //   - persists the selected theme id to data/theme.json
    //     (or `dataSubdir()/theme.json` if a subdir is configured),
    //   - re-applies the theme after a UI-scale change, and
    //   - exposes a Theme submenu in the View menu (full selector lives in
    //     Preferences > Appearance via ImTheme::ShowSelector).
    //
    // To register a new theme from an addon, call:
    //
    //     ImTheme::RegisterCustom({"myaddon", "Pretty Name", &myApplyFn});
    //     runtime().setTheme("myaddon");
    //
    // Valid ids include the upstream ImTheme built-ins ("Darcula",
    // "DarculaDarker", "ImGuiColorsDark", "MaterialFlat", ...) and any id
    // an addon has passed to ImTheme::RegisterCustom (e.g. "tb303").

    /// Set the active theme by its string id. Dispatches through
    /// ImTheme::ApplyByName which searches the custom registry first, then
    /// the vendored built-ins. Unknown ids fall back to kDefaultThemeId
    /// with a warning.
    void setTheme(const std::string& id);

    /// Currently selected theme id.
    const std::string& themeId() const { return m_themeId; }

    /// Default theme id used when no preference is saved and when
    /// `setTheme(id)` is called with an unknown id. Matches the upstream
    /// ImTheme name for Theme_DarculaDarker.
    static constexpr const char* kDefaultThemeId = "DarculaDarker";

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
    // Pref serializers — extension hook for addons to round-trip JSON into
    // appPrefs.json without ofxKit having to know about their fields.
    //
    // Each registered serializer contributes a save() lambda (called inside
    // saveAppPrefs after the built-in block) and a load() lambda (called
    // inside loadAppPrefs after the built-in block). Use a stable, unique id
    // so duplicate registrations are detected and re-registration replaces
    // the previous entry.
    //
    // Typical addon pattern — keep your settings in a struct on your addon
    // object and ferry it through ofJson:
    //
    //   runtime().registerPrefSerializer(
    //       "myaddon",
    //       [this](ofJson& j) {
    //           j["myaddon"] = {
    //               {"someInt",  m_settings.someInt},
    //               {"someStr",  m_settings.someStr},
    //           };
    //       },
    //       [this](const ofJson& j) {
    //           if (!j.contains("myaddon")) return;
    //           const auto& s = j["myaddon"];
    //           if (s.contains("someInt")) m_settings.someInt = s["someInt"];
    //           if (s.contains("someStr")) m_settings.someStr = s["someStr"].get<std::string>();
    //       });
    //
    // Addons that prefer a separate file (e.g. ofxMidiKit's midiPrefs.json)
    // do not need this API; it exists for addons that want to live inside
    // the main appPrefs.json alongside the built-in keys.
    // -------------------------------------------------------------------------
    struct PrefSerializer {
        std::string                        id;     // unique id (e.g. "myaddon")
        std::function<void(ofJson&)>       save;   // called inside saveAppPrefs
        std::function<void(const ofJson&)> load;   // called inside loadAppPrefs
    };

    void registerPrefSerializer(std::string id,
                                std::function<void(ofJson&)>       save,
                                std::function<void(const ofJson&)> load);
    bool unregisterPrefSerializer(const std::string& id);
    const std::vector<PrefSerializer>& prefSerializers() const { return m_prefSerializers; }

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
    void renderGizmoMenu();
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
    void drawPrefsAudio();

    void drawViewportWindow(ViewportInstance& vp, bool& visible);
    void drawViewportWindow2D(ViewportInstance& vp, bool& visible);
    void updateViewportCamera(ViewportInstance& vp);

    // Tool windows
    void drawCodeEditorWindow(bool& visible);
    void drawPathEditorWindow(bool& visible);
    void processFileDialogs();
    void drawGizmoOverlay();
    void drawGizmoInViewport(ViewportInstance& vp, const ofRectangle& imgScreenRect);

    bool             m_editMode   {false};
    bool             m_hideChrome {false};  ///< hides menu bar + status bar (set by toggleAllUI)
    bool             m_autoRegisterBuiltIns {false};
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
    std::string      m_dataSubdir;   ///< prefix under bin/data/ for ofxKit JSONs (empty = root)
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

    // UI scale & theme state. ImTheme (ofxImGuiStyle) owns the unscaled
    // baseline through ImTheme::CaptureBaseStyle; ofxKit only persists the
    // chosen scale/theme id and applies editor-window policy on top.
    float            m_uiScale     {1.0f};
    bool             m_uiScaleSet  {false};
    bool             m_themeSet    {false};
    std::string      m_themeId     {kDefaultThemeId};

    ofxImGui::Gui    m_gui;

    std::vector<ComponentDescriptor> m_components;
    bool             m_builtInComponentsRegistered {false};

    AppPrefs         m_prefs;
    bool             m_showRulers         {false};
    bool             m_defaultLayoutBuilt {false};
    bool             m_layoutResetPending {false};

    // Audio device cache (ASIO probing is slow — only scan once, refresh on demand)
    struct AudioDeviceInfo { std::string name; int inputChannels{0}; int outputChannels{0}; };
    std::vector<AudioDeviceInfo> m_cachedAudioDevices;
    bool                         m_audioDeviceListDirty {true};
    std::function<void()>        m_audioRestartCallback;

    // Test signal (mixed by mixTestSignal(), toggled from Audio Preferences UI)
    bool   m_testToneActive   {false};
    bool   m_testNoiseActive  {false};
    float  m_testToneFreq     {440.f};
    float  m_testSignalLevel  {0.1f};
    double m_testTonePhase    {0.0};

    std::vector<PreferencePage> m_preferencePages;
    bool             m_builtInPreferencePagesRegistered {false};
    std::string      m_selectedPreferencePage;          // id of selected page

    // Addon-supplied pref serializers, invoked by load/saveAppPrefs.
    std::vector<PrefSerializer> m_prefSerializers;

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
