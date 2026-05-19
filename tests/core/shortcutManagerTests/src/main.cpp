#include "ofxUnitTests.h"
#include "ShortcutManager.h"
#include "ofMain.h"

using ofkitty::ShortcutManager;

// ---------------------------------------------------------------------------
// Minimal key constants that mirror OF values so we don't pull in the full
// OF headers just for the constants (they're already included via ofMain.h).
// ---------------------------------------------------------------------------
static constexpr int MOD_NONE  = 0;
static constexpr int MOD_CTRL  = OF_KEY_CONTROL;
static constexpr int MOD_SHIFT = OF_KEY_SHIFT;
static constexpr int MOD_ALT   = OF_KEY_ALT;

class ofApp : public ofxUnitTestsApp {
    void run() override {

        // ------------------------------------------------------------------
        // formatBindingLabel
        // ------------------------------------------------------------------
        ofLogNotice() << "--- formatBindingLabel ---";

        ofxTest(ShortcutManager::formatBindingLabel('s', MOD_NONE) == "S",
                "single letter, no modifier");

        ofxTest(ShortcutManager::formatBindingLabel('s', MOD_CTRL) == "Ctrl-S",
                "Ctrl+S");

        ofxTest(ShortcutManager::formatBindingLabel('s', MOD_CTRL | MOD_SHIFT) == "Ctrl-Shift-S",
                "Ctrl+Shift+S");

        ofxTest(ShortcutManager::formatBindingLabel('z', MOD_CTRL | MOD_ALT) == "Ctrl-Alt-Z",
                "Ctrl+Alt+Z");

        ofxTest(!ShortcutManager::formatBindingLabel('a', MOD_NONE).empty(),
                "formatBindingLabel always returns non-empty string");

        // ------------------------------------------------------------------
        // bind / all / unbind
        // ------------------------------------------------------------------
        ofLogNotice() << "--- bind / all / unbind ---";

        ShortcutManager mgr;

        ofxTest(mgr.all().empty(), "freshly constructed — no shortcuts");

        int callCount = 0;
        mgr.bind('a', MOD_NONE, "Test A", [&]{ ++callCount; });
        mgr.bind('b', MOD_CTRL, "Test Ctrl-B", [&]{ ++callCount; });

        ofxTest(mgr.all().size() == 2, "two anonymous shortcuts registered");

        // anonymous shortcuts have empty actionId
        ofxTest(mgr.all()[0].actionId.empty(), "anonymous bind has empty actionId");

        mgr.unbind('a', MOD_NONE);
        ofxTest(mgr.all().size() == 1, "unbind removes the matching shortcut");

        mgr.unbind('b', MOD_CTRL);
        ofxTest(mgr.all().empty(), "unbind last shortcut — list is empty again");

        // ------------------------------------------------------------------
        // registerAction / applyBinding
        // ------------------------------------------------------------------
        ofLogNotice() << "--- registerAction / applyBinding ---";

        ShortcutManager mgr2;

        mgr2.registerAction("file.save", 's', MOD_CTRL, "Save", []{});
        mgr2.registerAction("file.open", 'o', MOD_CTRL, "Open", []{});

        ofxTest(mgr2.all().size() == 2, "two named actions registered");
        ofxTest(mgr2.all()[0].actionId == "file.save", "first action id is file.save");
        ofxTest(mgr2.all()[0].key == 's',             "first action key is 's'");
        ofxTest(mgr2.all()[0].modifiers == MOD_CTRL,  "first action modifier is Ctrl");
        ofxTest(mgr2.all()[0].description == "Save",  "first action description is Save");

        // Re-registering same id replaces the old entry
        mgr2.registerAction("file.save", 's', MOD_CTRL | MOD_SHIFT, "Save As", []{});
        ofxTest(mgr2.all().size() == 2, "re-registering same id does not grow the list");

        // applyBinding — remap to a free key
        bool ok = mgr2.applyBinding("file.save", 'w', MOD_CTRL);
        ofxTest(ok, "applyBinding to free key succeeds");
        const auto& updated = mgr2.all();
        auto it = std::find_if(updated.begin(), updated.end(),
                               [](const ofkitty::Shortcut& s){ return s.actionId == "file.save"; });
        ofxTest(it != updated.end() && it->key == 'w', "applyBinding updated the key");

        // applyBinding to a conflicting combo should fail
        bool conflict = mgr2.applyBinding("file.save", 'o', MOD_CTRL); // 'o'+Ctrl is file.open
        ofxTest(!conflict, "applyBinding to conflicting combo returns false");

        // ------------------------------------------------------------------
        // captureActionId / isCapturing / cancelCapture
        // ------------------------------------------------------------------
        ofLogNotice() << "--- capture state ---";

        ShortcutManager mgr3;
        mgr3.registerAction("edit.undo", 'z', MOD_CTRL, "Undo", []{});

        ofxTest(!mgr3.isCapturing(), "not capturing initially");

        bool started = mgr3.beginCapture("edit.undo");
        ofxTest(started, "beginCapture returns true for existing action");
        ofxTest(mgr3.isCapturing(), "isCapturing() true after beginCapture");
        ofxTest(mgr3.captureActionId() == "edit.undo", "captureActionId matches");

        mgr3.cancelCapture();
        ofxTest(!mgr3.isCapturing(), "not capturing after cancelCapture");

        bool badStart = mgr3.beginCapture("nonexistent.action");
        ofxTest(!badStart, "beginCapture returns false for unknown action");
    }
};

#include "app/ofAppNoWindow.h"
#include "app/ofAppRunner.h"

int main() {
    ofInit();
    auto window = std::make_shared<ofAppNoWindow>();
    auto app    = std::make_shared<ofApp>();
    ofRunApp(window, app);
    return ofRunMainLoop();
}
