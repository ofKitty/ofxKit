#pragma once

#include "ofMain.h"
#include "ShortcutManager.h"
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
        bool visible {true};
        std::function<void(bool& visible)> draw;
    };

    // -------------------------------------------------------------------------
    // Singleton access
    // -------------------------------------------------------------------------
    static Runtime& instance();

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

    bool             m_editMode  {false};
    bool             m_attached  {false};
    bool             m_skipShortcutDispatch {false};
    bool             m_builtInWindowsRegistered {false};
    entt::entity     m_selected  {entt::null};
    entt::registry   m_registry;
    entt::registry*  m_registryView {&m_registry};
    ShortcutManager  m_shortcuts;
    std::string      m_appName;
    std::vector<RuntimeWindow> m_windows;
};

// Convenience free function
inline Runtime& runtime() { return Runtime::instance(); }

} // namespace ofkitty
