# Windows

`Runtime` manages a list of named ImGui windows. Any addon or sketch can register its own window and it will be drawn automatically each frame (inside the ImGui context that the Runtime owns). Registered windows also appear as toggle items in the **View** menu of the main menu bar.

These are **ImGui / dockable** windows (what Dear ImGui calls a “window”), not an extra `ofAppBaseWindow` / GLFW window.

**Implementation note:** Class-based registration (`KitRegisteredWindow`, `KitPropertyBag`, optional `bridges/Runtime_register_bapp.h`) ships in the **window-class milestone** in the ofxKit runtime sources. If those headers are not in your tree yet, use the **lambda** registration form in the next section only.

## Registering a window (lambda)

```cpp
ofkitty::runtime().registerWindow({
    "My Panel",          // name — must be unique; also used as ImGui window title
    "View",              // menuGroup — which top-level menu the toggle lives under
    true,                // visible — initial visibility
    false,               // editModeOnly — false = always drawn, true = only in Edit mode
    [](bool& visible) {  // draw callback
        if (ImGui::Begin("My Panel", &visible)) {
            ImGui::Text("Hello from my panel");
        }
        ImGui::End();
    }
});
```

`registerWindow` returns a `RuntimeWindow*` you can keep to change `visible` programmatically, or `nullptr` if the name was already taken.

## Class-based windows (`KitRegisteredWindow`)

For the same contract as **ofxBapp**’s `bapp::baseWindow`—a **name**, **visibility**, a **virtual `draw()`**, and a small **property list** compatible with `baseProp`-style serialization—you can subclass **`ofkitty::KitRegisteredWindow`** (which carries a **`KitPropertyBag`**: the same `propTypes` / `sProp` / `addProperty` / `getProperty` pattern as `bapp::baseProp`).

```cpp
struct MyPanel : public ofkitty::KitRegisteredWindow {
    MyPanel() {
        setName("My Panel");
        setVisible(true);
        // "Visible" is registered automatically like bapp::baseWindow
    }
    void draw() override {
        if (ImGui::Begin(getName().c_str(), &getVisible())) {
            ImGui::Text("Hello from KitRegisteredWindow");
        }
        ImGui::End();
    }
};

// Own the panel for as long as it is registered:
auto panel = std::make_shared<MyPanel>();
ofkitty::runtime().registerWindow(panel, "View", false);
```

**Semantics** match **ofxBapp**’s bridge in `myGui::registerWithRuntime`: each frame the runtime syncs the `bool& visible` used by ImGui with your object’s visibility, then calls **`draw()`** only when the window should be shown.

Prefer **`std::shared_ptr<KitRegisteredWindow>`** overloads so `Runtime` can keep the instance alive. Raw-pointer overloads, if provided, require you to **unregister or outlive** the registration (same lifetime rules as stack-allocated **`baseWindow`** subclasses inside **ofxBapp**).

## ofxBapp (`bapp::baseWindow`) bridge

Sketch code that already has **`ofxBapp`** on the include path can use the thin header **`src/bridges/Runtime_register_bapp.h`** (shipped inside **ofxKit**). It wraps a **`bapp::baseWindow*`** exactly like the lambdas in `myGui::registerWithRuntime`—your project must **not** include this header unless **ofxBapp** is a dependency.

```cpp
#include "bridges/Runtime_register_bapp.h"

void ofApp::setup() {
    ofkitty::registerBappWindow(ofkitty::runtime(), &m_gui.whateverWin, "View", false);
}
```

(Exact helper name matches the declaration in that header.)

## RuntimeWindow fields

| Field | Type | Description |
|---|---|---|
| `name` | `std::string` | Unique window name and ImGui title |
| `menuGroup` | `std::string` | Menu group the visibility toggle appears under (default `"View"`) |
| `visible` | `bool` | Current visibility; toggled by the menu item |
| `editModeOnly` | `bool` | When `true`, the window is only drawn while Edit mode is active |
| `draw` | `function<void(bool&)>` | Called each frame when `visible` is true |

For **`KitRegisteredWindow`** registrations, the runtime may fill `draw` with an internal thunk; the public API is **`registerWindow(shared_ptr<KitRegisteredWindow>, …)`**.

## Property bag / serialization parity

`KitRegisteredWindow` inherits the **property-bag** API modeled on **`bapp::baseProp`**. Addons or save/load code can walk **`getProperties()`** the same way they would for a **`bapp::baseWindow`**, without pulling **ofxBapp** into pure-**ofxKit** projects.

## Programmatic visibility control

```cpp
ofkitty::runtime().setWindowVisible("My Panel", false); // hide
ofkitty::runtime().setWindowVisible("My Panel", true);  // show

// Direct pointer — valid as long as the Runtime is alive
auto* win = ofkitty::runtime().findWindow("My Panel");
if (win) win->visible = !win->visible;
```

## Built-in windows

Built-ins are implemented on top of the same registration pipeline (internally they use **`KitRegisteredWindow`**-style types so behaviour stays aligned with **ofxBapp** window classes).

| Name | editModeOnly | Description |
|---|---|---|
| `Toolbar` | false | Floating tool-picker panel (see [toolbar.md](toolbar.md)) |
| `Scene` | true | Entity list for the attached EnTT registry |
| `Properties` | true | Component inspector for the selected entity |
| `Shortcuts` | true | Editable keyboard shortcut list |
| `Preferences` | true | App / editor preferences (see [preferences.md](preferences.md)) |
| `Code Editor` | true | Text editor panel (ofxImGuiTextEdit) |
| `Path Editor` | true | Vector path editor panel |

The Toolbar is always drawn because tools need to be accessible outside Edit mode.
