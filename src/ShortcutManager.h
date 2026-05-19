#pragma once

#include <functional>
#include <string>
#include <vector>

class ofKeyEventArgs;

namespace ofkitty {

// ============================================================================
// ShortcutManager
// ============================================================================
// Register key + modifier combinations with descriptions and callbacks.
// Runtime::onKeyPressed() dispatches through this — one place for all
// keyboard shortcuts in the application.
//
// Named actions (registerAction):
//   - Stable id for JSON load/save and the Shortcuts editor (Edit mode).
//   - Persisted under data/shortcuts.json by default (auto-save on change),
//     or `data/<dataSubdir>/shortcuts.json` when Runtime::setDataSubdir() is set.
//
//   ofkitty::runtime().keys().registerAction(
//       "my.save", 's', OF_KEY_CONTROL, "Save",
//       [&]{ saveProject(); });
//
// Anonymous shortcuts (bind):
//   - Same dispatch behavior; not listed in JSON; Shortcuts window is read-only
//     for those rows (no remapping UI).
//
// Modifier matching: all modifiers listed in the bitmask MUST be held.
// Extra modifiers held by the user are ignored.
// ============================================================================

struct Shortcut {
    std::string           actionId;    // empty for anonymous bind()
    int                   key;         // OF key code ('e', OF_KEY_F1, etc.)
    int                   modifiers;   // bitmask OF_KEY_CONTROL / SHIFT / ALT / COMMAND
    std::string           description; // shown in the Edit mode cheatsheet
    std::function<void()> action;
};

class ShortcutManager {
public:
    /// Anonymous shortcut — not persisted, not editable in the Shortcuts window.
    void bind(int key, int modifiers,
              const std::string& description,
              std::function<void()> action);

    /// Named shortcut — one row per id; replaces any previous registration with the same id.
    void registerAction(const std::string& actionId,
                        int key, int modifiers,
                        const std::string& description,
                        std::function<void()> action);

    /// Remove all bindings with the given key + modifiers combination.
    void unbind(int key, int modifiers);

    /// Dispatch a key event. Returns true if at least one shortcut fired.
    bool dispatch(int key);

    /// Prefer this overload: uses event modifiers and resolves Ctrl+letter codes
    /// (Windows/GLFW often reports \x01–\x1A instead of 'a'–'z').
    bool dispatch(const ofKeyEventArgs& e);

    const std::vector<Shortcut>& all() const { return m_shortcuts; }

    /// Human-readable combo, e.g. "Ctrl-Shift-S"
    static std::string formatBindingLabel(int key, int modifiers);

    /// Default persistence path: `<bin/data>/shortcuts.json` (or
    /// `<bin/data>/<Runtime::dataSubdir()>/shortcuts.json` if a subdir is set).
    /// Directory created on first save if needed.
    static std::string defaultBindingsPath();

    /// Merge bindings from file into registered actions (unknown ids skipped).
    /// Returns false if the file was missing or invalid.
    bool loadBindingsFromFile(const std::string& path);

    /// Save named actions only. Pretty-printed JSON with "version" + "bindings".
    bool saveBindingsToFile(const std::string& path) const;

    /// Apply a combo at runtime (conflict-checked). Usually used after UI capture or manual prefs.
    bool applyBinding(const std::string& actionId, int key, int modifiers);

    bool beginCapture(const std::string& actionId);
    void cancelCapture();

    bool isCapturing() const { return !m_captureActionId.empty(); }
    const std::string& captureActionId() const { return m_captureActionId; }
    const std::string& lastCaptureError() const { return m_lastCaptureError; }

    /// Install on BEFORE_APP keyPressed — consumes capture / cancel so shortcuts do not also fire.
    bool handleCaptureKey(const ofKeyEventArgs& e);

    /// Optional app notification after load, successful remap, or save.
    std::function<void()> onBindingsChanged;

    void setAutoSaveEnabled(bool enabled) { m_autoSave = enabled; }
    bool autoSaveEnabled() const { return m_autoSave; }

private:
    bool conflictsExcept(const std::string& exceptId, int key, int modifiers) const;
    void eraseActionId(const std::string& actionId);
    void notifyChanged();

    static int modifiersFromKeyboard();

    std::vector<Shortcut> m_shortcuts;
    std::string           m_captureActionId;
    std::string           m_lastCaptureError;
    bool                  m_autoSave {true};
    bool                  m_loadingBatch {false};
};

} // namespace ofkitty
