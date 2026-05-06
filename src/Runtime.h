#pragma once

#include "ofMain.h"
#include "ShortcutManager.h"
#include <ofxImGui/src/ofxImGui.h>
#include <entt.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ofkitty {

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
        bool editModeOnly {false}; // false = always drawn; true = only drawn in Edit mode
        std::function<void(bool& visible)> draw;
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
    void toggleEditMode()      { m_editMode = !m_editMode;
                                 ofLogNotice("ofxKit") << "Edit mode "
                                     << (m_editMode ? "ON" : "OFF"); }
    void setEditMode(bool on)  { m_editMode = on; }

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
        EnhancedDark,  ///< Doug Binks' dark theme (default).
        EnhancedLight, ///< Doug Binks' light theme.
        Dark,          ///< ImGui::StyleColorsDark()
        Light,         ///< ImGui::StyleColorsLight()
        Classic,       ///< ImGui::StyleColorsClassic()
    };

    /// Set the active ImGui theme. Persists in bin/data/ofxKit/theme.json
    /// and is restored on next launch. Re-applies the current uiScale on
    /// top so the two settings compose cleanly.
    void setTheme(Theme theme);
    Theme theme() const { return m_theme; }

    // -------------------------------------------------------------------------
    // Windows
    // -------------------------------------------------------------------------
    RuntimeWindow* registerWindow(RuntimeWindow window);
    RuntimeWindow* findWindow(const std::string& name);
    const RuntimeWindow* findWindow(const std::string& name) const;
    bool setWindowVisible(const std::string& name, bool visible);
    std::vector<RuntimeWindow>& windows() { return m_windows; }
    const std::vector<RuntimeWindow>& windows() const { return m_windows; }

private:
    Runtime() = default;

    void attachInternal(shared_ptr<ofAppBaseWindow> window,
                        shared_ptr<ofBaseApp>       app,
                        entt::registry&             registry);

    void onSetup(ofEventArgs&);
    void onUpdate(ofEventArgs&);
    void onDraw(ofEventArgs&);
    void onShortcutCaptureBeforeApp(ofKeyEventArgs&);
    void onKeyPressed(ofKeyEventArgs&);

    void drawOverlay();
    void renderMainMenuBar();
    void registerBuiltInWindows();
    void drawSceneWindow(bool& visible);
    void drawPropertiesWindow(bool& visible);
    void drawShortcutsWindow(bool& visible);
    void ensureAppName();
    void applyUIScale();
    void loadUIScalePref();
    void saveUIScalePref();

    void applyTheme();
    void loadThemePref();
    void saveThemePref();

    bool             m_editMode  {false};
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
    std::vector<std::pair<std::string, MenuBarCallback>> m_menuGroups;
    std::vector<MenuBarCallback> m_menuBarRawCallbacks;
    std::vector<PostSetupHook>  m_postSetupHooks;

    // UI scale & theme state. m_baseStyle holds the un-scaled colours of the
    // current theme; setUIScale() restores from this baseline before
    // re-applying ScaleAllSizes so it never compounds. Setting a theme
    // refreshes m_baseStyle and re-applies the current uiScale on top.
    float            m_uiScale     {1.0f};
    bool             m_uiScaleSet  {false};
    bool             m_baseStyleCaptured {false};
    bool             m_themeSet    {false};
    Theme            m_theme       {Theme::EnhancedDark};
    ImGuiStyle       m_baseStyle;

    ofxImGui::Gui    m_gui;
};

// Convenience free function
inline Runtime& runtime() { return Runtime::instance(); }

} // namespace ofkitty
