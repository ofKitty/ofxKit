#include "ShortcutManager.h"

#include "ofEvents.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofLog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace ofkitty {

namespace {

/// GLFW / Windows often deliver Ctrl+A..Z as ASCII SOH..SUB (1–26) in `ofKeyEventArgs::key`.
bool shortcutKeyMatches(int eventKey, int boundKey)
{
    if (eventKey == boundKey) {
        return true;
    }
    if (boundKey >= 'A' && boundKey <= 'Z') {
        const int lower = boundKey + ('a' - 'A');
        if (eventKey == lower) {
            return true;
        }
    }
    if (boundKey >= 'a' && boundKey <= 'z') {
        const int upper = boundKey - ('a' - 'A');
        if (eventKey == upper) {
            return true;
        }
    }
    if (boundKey >= 'a' && boundKey <= 'z') {
        const int ctrlCode = boundKey - 'a' + 1;
        if (eventKey == ctrlCode) {
            return true;
        }
    }
    if (boundKey >= 'A' && boundKey <= 'Z') {
        const int ctrlCode = boundKey - 'A' + 1;
        if (eventKey == ctrlCode) {
            return true;
        }
    }
    return false;
}

bool modifiersMatchShortcut(int heldMods, int requiredMods)
{
    if ((requiredMods & OF_KEY_CONTROL) && !(heldMods & OF_KEY_CONTROL)) {
        return false;
    }
    if ((requiredMods & OF_KEY_SHIFT) && !(heldMods & OF_KEY_SHIFT)) {
        return false;
    }
    if ((requiredMods & OF_KEY_ALT) && !(heldMods & OF_KEY_ALT)) {
        return false;
    }
    if ((requiredMods & OF_KEY_COMMAND) && !(heldMods & OF_KEY_COMMAND)) {
        return false;
    }
    return true;
}

bool isStandaloneModifierKey(int key)
{
    switch (key) {
    case OF_KEY_SHIFT:
    case OF_KEY_CONTROL:
    case OF_KEY_ALT:
    case OF_KEY_COMMAND:
    case OF_KEY_LEFT_SHIFT:
    case OF_KEY_RIGHT_SHIFT:
    case OF_KEY_LEFT_CONTROL:
    case OF_KEY_RIGHT_CONTROL:
    case OF_KEY_LEFT_ALT:
    case OF_KEY_RIGHT_ALT:
    case OF_KEY_LEFT_COMMAND:
    case OF_KEY_RIGHT_COMMAND:
        return true;
    default:
        return false;
    }
}

} // namespace

void ShortcutManager::bind(int key, int modifiers,
                           const std::string& description,
                           std::function<void()> action)
{
    m_shortcuts.push_back({"", key, modifiers, description, std::move(action)});
}

void ShortcutManager::registerAction(const std::string& actionId,
                                     int key, int modifiers,
                                     const std::string& description,
                                     std::function<void()> action)
{
    if (actionId.empty()) return;

    eraseActionId(actionId);
    m_shortcuts.push_back({actionId, key, modifiers, description, std::move(action)});
}

void ShortcutManager::eraseActionId(const std::string& actionId)
{
    m_shortcuts.erase(
        std::remove_if(m_shortcuts.begin(), m_shortcuts.end(),
            [&actionId](const Shortcut& s) { return s.actionId == actionId; }),
        m_shortcuts.end());
}

void ShortcutManager::unbind(int key, int modifiers)
{
    m_shortcuts.erase(
        std::remove_if(m_shortcuts.begin(), m_shortcuts.end(),
            [key, modifiers](const Shortcut& s) {
                return s.key == key && s.modifiers == modifiers;
            }),
        m_shortcuts.end());
}

bool ShortcutManager::conflictsExcept(const std::string& exceptId, int key, int modifiers) const
{
    for (const auto& s : m_shortcuts) {
        if (s.actionId == exceptId) continue;
        if (s.key == key && s.modifiers == modifiers) return true;
    }
    return false;
}

bool ShortcutManager::applyBinding(const std::string& actionId, int key, int modifiers)
{
    if (actionId.empty()) return false;

    Shortcut* target = nullptr;
    for (auto& s : m_shortcuts) {
        if (s.actionId == actionId) {
            target = &s;
            break;
        }
    }
    if (!target) return false;

    if (conflictsExcept(actionId, key, modifiers)) {
        return false;
    }

    target->key        = key;
    target->modifiers  = modifiers;

    if (!m_loadingBatch) notifyChanged();
    return true;
}

bool ShortcutManager::dispatch(int key)
{
    bool fired = false;
    for (auto& s : m_shortcuts) {
        if (!shortcutKeyMatches(key, s.key)) {
            continue;
        }

        bool ok = true;
        if ((s.modifiers & OF_KEY_CONTROL) && !ofGetKeyPressed(OF_KEY_CONTROL)) ok = false;
        if ((s.modifiers & OF_KEY_SHIFT)   && !ofGetKeyPressed(OF_KEY_SHIFT))   ok = false;
        if ((s.modifiers & OF_KEY_ALT)     && !ofGetKeyPressed(OF_KEY_ALT))     ok = false;
        if ((s.modifiers & OF_KEY_COMMAND) && !ofGetKeyPressed(OF_KEY_COMMAND)) ok = false;

        if (ok) {
            s.action();
            fired = true;
        }
    }
    return fired;
}

bool ShortcutManager::dispatch(const ofKeyEventArgs& e)
{
    if (e.type != ofKeyEventArgs::Pressed) {
        return false;
    }
    // GLFW sends auto-repeat as Pressed + isRepeat; otherwise toggles (e.g. Edit mode)
    // fire every repeat tick while the keys are held.
    if (e.isRepeat) {
        return false;
    }

    const int held = e.modifiers != 0
        ? e.modifiers
        : modifiersFromKeyboard();

    bool fired = false;
    for (auto& s : m_shortcuts) {
        if (!shortcutKeyMatches(e.key, s.key)) {
            continue;
        }
        if (!modifiersMatchShortcut(held, s.modifiers)) {
            continue;
        }
        s.action();
        fired = true;
    }
    return fired;
}

std::string ShortcutManager::formatBindingLabel(int key, int modifiers)
{
    std::string prefix;
    if (modifiers & OF_KEY_CONTROL) prefix += "Ctrl-";
    if (modifiers & OF_KEY_COMMAND) prefix += "Cmd-";
    if (modifiers & OF_KEY_SHIFT) prefix += "Shift-";
    if (modifiers & OF_KEY_ALT) prefix += "Alt-";

    char keyChar[16] = {};
    if (key >= 32 && key < 127)
        std::snprintf(keyChar, sizeof(keyChar), "%c", (char)std::toupper(key));
    else
        std::snprintf(keyChar, sizeof(keyChar), "#%d", key);

    return prefix + keyChar;
}

std::string ShortcutManager::defaultBindingsPath()
{
    return ofToDataPath("ofxKit/shortcuts.json", true);
}

bool ShortcutManager::loadBindingsFromFile(const std::string& path)
{
    if (!of::filesystem::exists(of::filesystem::path(path))) {
        return false;
    }

    ofJson j = ofLoadJson(of::filesystem::path(path));
    if (j.is_null() || j.empty()) return false;

    if (!j.contains("bindings") || !j["bindings"].is_array()) {
        ofLogWarning("ofxKit") << "shortcuts.json: missing \"bindings\" array";
        return false;
    }

    m_loadingBatch = true;
    int applied = 0;
    for (const auto& b : j["bindings"]) {
        if (!b.contains("id") || !b["id"].is_string()) continue;
        std::string id = b["id"].get<std::string>();
        if (!b.contains("key")) continue;
        int key = b["key"].get<int>();
        int mod = b.value("modifiers", 0);
        if (applyBinding(id, key, mod)) ++applied;
    }
    m_loadingBatch = false;

    notifyChanged();
    return true;
}

bool ShortcutManager::saveBindingsToFile(const std::string& path) const
{
    ofJson arr = ofJson::array();
    for (const auto& s : m_shortcuts) {
        if (s.actionId.empty()) continue;
        ofJson row;
        row["id"]         = s.actionId;
        row["key"]        = s.key;
        row["modifiers"]  = s.modifiers;
        row["description"] = s.description;
        arr.push_back(std::move(row));
    }

    ofJson root;
    root["version"]   = 1;
    root["bindings"]  = std::move(arr);

    return ofSavePrettyJson(of::filesystem::path(path), root);
}

void ShortcutManager::notifyChanged()
{
    if (onBindingsChanged) onBindingsChanged();
    if (m_autoSave && !m_loadingBatch) {
        saveBindingsToFile(defaultBindingsPath());
    }
}

bool ShortcutManager::beginCapture(const std::string& actionId)
{
    for (const auto& s : m_shortcuts) {
        if (s.actionId == actionId) {
            m_captureActionId   = actionId;
            m_lastCaptureError.clear();
            return true;
        }
    }
    return false;
}

void ShortcutManager::cancelCapture()
{
    m_captureActionId.clear();
    m_lastCaptureError.clear();
}

int ShortcutManager::modifiersFromKeyboard()
{
    int mod = 0;
    if (ofGetKeyPressed(OF_KEY_SHIFT)) mod |= OF_KEY_SHIFT;
    if (ofGetKeyPressed(OF_KEY_CONTROL)) mod |= OF_KEY_CONTROL;
    if (ofGetKeyPressed(OF_KEY_ALT)) mod |= OF_KEY_ALT;
    if (ofGetKeyPressed(OF_KEY_COMMAND)) mod |= OF_KEY_COMMAND;
    return mod;
}

bool ShortcutManager::handleCaptureKey(const ofKeyEventArgs& e)
{
    if (m_captureActionId.empty()) return false;

    if (e.key == OF_KEY_ESC) {
        cancelCapture();
        return true;
    }

    if (isStandaloneModifierKey(e.key)) {
        return false;
    }

    int mod = modifiersFromKeyboard();

    if (conflictsExcept(m_captureActionId, e.key, mod)) {
        m_lastCaptureError = "That combo is already in use.";
        return true;
    }

    if (!applyBinding(m_captureActionId, e.key, mod)) {
        m_lastCaptureError = "Could not apply binding (unknown action?).";
        return true;
    }

    m_captureActionId.clear();
    m_lastCaptureError.clear();
    return true;
}

} // namespace ofkitty
