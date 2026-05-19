# ofxKit

> Part of the [ofKitty](https://github.com/ofkitty/openFrameworks) distribution of openFrameworks.

ofKit is a composition-first runtime addon. It injects the **Edit Inspector** overlay into `ofBaseApp` sketches with one explicit attach call in `main.cpp`.

Press **Cmd-E** at runtime to toggle the overlay (`Ctrl-E` is also registered as a fallback).

---

## What it does

- Provides a default `entt::registry`, or attaches to an app-owned registry
- Registers itself with OF's event system so it draws *after* the app's `draw()`
- Provides a live ImGui inspector overlay (powered by `ofxEnTTInspector`) in Edit Mode
- Exposes entity selection state (`ofkitty::runtime().selected()`)

### Registered ImGui windows

Sketches register dockable Inspector panels via `Runtime::registerWindow` — either with a **lambda** or (when the window-class headers are present in your ofxKit checkout) by subclassing **`KitRegisteredWindow`**, the same typed shape as **ofxBapp**'s **`bapp::baseWindow`**. See **[docs/windows.md](docs/windows.md)** for lambdas, **`KitPropertyBag`** / **baseProp** parity, and the optional **ofxBapp** bridge include.

Built-in windows (Scene, Properties, Toolbar, Shortcuts, Preferences, Code Editor, Path Editor) are **opt-in by default** — none are registered unless you ask for them. Call any of these before `ofRunApp()`:

```cpp
runtime().enableBuiltInWindows();           // Scene + Properties (standard set)
runtime().enableBuiltInWindow("Toolbar");   // one at a time (additive)
runtime().enableAllBuiltInWindows();        // all built-in panels
runtime().disableBuiltInWindows();          // reset to default (none)
```

---

## How attach works

The project template includes `ofxKit.h`. In `main.cpp` it creates the app as a `std::shared_ptr` and attached it to the runtime, then we pass the app to `ofRunApp()`. Meaning we can push improvements up and run ofKitty from Addons in Vanilla OF.

```
ofApp.h  →  #include "ofxKit.h"
main.cpp →  ofkitty::Runtime::attach(window, app[, registry])
         →  ofRunApp(window, std::move(app))
```

`main.cpp` stays close to the standard openFrameworks shape:

```cpp
#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLWindowSettings settings;
    settings.setSize(1024, 768);
    auto window = ofCreateWindow(settings);
    auto app = std::make_shared<ofApp>();
    ofkitty::Runtime::attach(window, app);
    ofRunApp(window, std::move(app));
    ofRunMainLoop();
}
```

---

## Usage in a sketch

### Runtime-owned registry

```cpp
// ofApp.h  (generated template — nothing to add)
#pragma once
#include "ofMain.h"
#include "ofxKit.h"   // already here from the template

class ofApp : public ofBaseApp { ... };
```

```cpp
// ofApp.cpp — access the registry anywhere
#include "ofApp.h"

void ofApp::setup() {
    auto& reg = ofkitty::runtime().registry();

    auto e = reg.create();
    reg.emplace<ecs::node_component>(e, "My Node");
    // ...
}
```

### App-owned registry

When your app owns the ECS state, pass it to `attach` so the Runtime's inspector, status bar, and Scene panel all see the same entities:

```cpp
// ofApp.h
class ofApp : public ofBaseApp {
public:
    entt::registry& registry() { return m_registry; }
    void setup() override;
private:
    entt::registry m_registry;
};
```

```cpp
// main.cpp
auto app = std::make_shared<ofApp>();
ofkitty::Runtime::attach(window, app, app->registry());
ofRunApp(window, std::move(app));
```

If you omit the registry argument, `runtime().registry()` returns Runtime's own internal registry — only use that path if your app creates all entities via `runtime().registry()` directly.

---

## Runtime registries

`ofxKit` is the composition runtime for ofKitty. Core addons should stay small and headless where possible; optional kit addons can register the UI, ECS components, and systems that make sense for a specific app.

The first concrete registry is the window registry:

```cpp
ofkitty::runtime().registerWindow({
    "Machine",
    "View",
    true,
    [](bool& visible) {
        if (ImGui::Begin("Machine", &visible)) {
            ImGui::TextUnformatted("Machine status");
        }
        ImGui::End();
    },
});
```

Built-in windows such as `Properties`, and `Shortcuts` use the same path. The `View` menu is generated from the registered windows, so addon-owned windows appear beside the built-in ones.

### Kit addon pattern

Use a companion kit addon when a core addon needs optional ofKitty integration. For GRBL, the split is:

```text
ofxGrbl       core/headless serial + machine-control code
ofxGrblKit    optional ofKitty integration
```

`ofxGrblKit` can contain windows, components, systems, and ofKitty registration helpers:

```text
ofxGrblKit/src/windows/
ofxGrblKit/src/components/
ofxGrblKit/src/systems/
ofxGrblKit/src/kit/
```

Example window registration from an app or kit helper:

```cpp
#include "ofxGrblKit.h"

grbl::GrblSender sender;
grbl::PlotterPrefs prefs;
grbl::kit::PlotterSerialConnectionWindow serialWindow;

void ofApp::setup() {
    serialWindow.setSender(&sender);
    serialWindow.setPrefs(&prefs);
    serialWindow.refreshDeviceList();
    serialWindow.syncSelectionFromPrefs();

    grbl::kit::registerPlotterSerialWindow(ofkitty::runtime(), serialWindow);
}
```

### Components and systems

Built-in **"+ Add Component"** picker rows for shipped **`ecs::*`** types come from **ofxEnTTKit** (`ecs::registerKitComponentMenu`), which `ofxKit` forwards into `runtime().registerComponent(...)` at startup. Component **property** panels stay type-driven via **ofxEnTTInspector**.

Addons extend the picker by registering their own types:

```cpp
// minimal — has / remove generated automatically from T
ofkitty::runtime().registerComponent<grbl::MachineStateComponent>(
    "Machine State", "Machines");

// custom add — when default emplace<T>(entity) needs extra work
ofkitty::runtime().registerComponent<grbl::MachineStateComponent>(
    "Machine State", "Machines",
    [](entt::registry& r, entt::entity e) {
        r.emplace<grbl::MachineStateComponent>(e).connect("/dev/ttyUSB0");
    });
```

See [`docs/component-registry.md`](docs/component-registry.md) for the full API including full-control registration, query helpers, and the built-in category list.

The intended system API should let kit addons register lifecycle systems against the active registry:

```cpp
ofkitty::runtime().registerSystem<grbl::MachineSystem>("Machine");
```

Those APIs should live beside the window registry, not inside it:

```text
WindowRegistry       UI panels and menu visibility
ComponentRegistry    component labels, add/remove hooks, inspector callbacks
SystemRegistry       setup/update/draw/cleanup lifecycle orchestration
```

---

## API reference

### `ofkitty::Runtime`


| Member                                           | Description                                                              |
| ------------------------------------------------ | ------------------------------------------------------------------------ |
| `Runtime::instance()`                            | Returns the singleton                                                    |
| `runtime()`                                      | Free-function shorthand for `Runtime::instance()`                        |
| `runtime().registry()`                           | The active `entt::registry` (owned by runtime or supplied by the app)    |
| `runtime().selected()`                           | Currently selected entity (`entt::null` if none)                         |
| `runtime().select(e)`                            | Programmatically select an entity                                        |
| `runtime().isEditMode()`                         | Whether the overlay is currently visible                                 |
| `runtime().toggleEditMode()`                     | Toggle Edit mode on/off                                                  |
| `runtime().setAppName(name)`                     | Sets the name shown in the menu bar and status bar                       |
| `runtime().registerWindow(w)`                    | Register a dockable UI panel (appears in the View menu)                  |
| `runtime().disableBuiltInWindows()`              | Reset to default: no built-in windows registered                         |
| `runtime().enableBuiltInWindow(nameOrId)`        | Opt in to one specific built-in window (additive)                        |
| `runtime().enableBuiltInWindows()`               | Register the standard set: Scene + Properties                            |
| `runtime().enableAllBuiltInWindows()`            | Register all built-in windows                                            |
| `runtime().addMenuBarGroup(name, cb)`            | Add a top-level menu group to the main menu bar                          |
| `runtime().registerComponent<T>(name, cat)`      | Register a component type in the Add Component picker (template form)    |
| `runtime().registerComponent(desc)`              | Register a component with explicit has/add/remove lambdas                |
| `runtime().componentDescriptors()`               | All registered `ComponentDescriptor` entries in registration order       |
| `runtime().componentCategories()`               | Unique category names in registration order                              |
| `runtime().showRulers()`                         | Whether pixel rulers are visible                                         |
| `runtime().setShowRulers(bool)`                  | Show/hide the pixel ruler overlay                                        |
| `runtime().toggleRulers()`                       | Toggle ruler visibility (also bound to F2)                               |
| `runtime().setViewportRenderer(fn)`              | Register the scene-draw callback for the secondary Viewport panel        |
| `runtime().clearViewportRenderer()`              | Remove the viewport renderer (panel shows a placeholder)                 |
| `runtime().registerPreferencePage(page)`         | Add a page to the Preferences window (built-ins + addon pages)           |
| `runtime().unregisterPreferencePage(id)`         | Remove a preference page by id                                           |
| `runtime().registerStatusItem(item)`             | Push an item into the status bar (compact draw callback)                 |
| `runtime().unregisterStatusItem(id)`             | Remove a status bar item by id                                           |
| `runtime().openFileDialog(key,title,flt,cb)`     | Open a file-open dialog; `cb` fires with the chosen path on confirm      |
| `runtime().saveFileDialog(key,title,flt,fn,cb)`  | Open a file-save dialog; `cb` fires with the chosen path on confirm      |
| `runtime().setSceneCamera(cam)`                  | Provide the main-scene camera so the gizmo can be drawn on it            |
| `runtime().clearSceneCamera()`                   | Remove the scene camera reference                                        |
| `runtime().setGizmoOperation(op)`                | Set gizmo mode: Translate / Rotate / Scale / Universal (W/E/R shortcuts) |
| `runtime().setGizmoMode(mode)`                   | Set gizmo space: World / Local (X shortcut)                              |
| `runtime().codeEditorSetText(src)`               | Seed the Code Editor with a source string                                |
| `runtime().codeEditorGetText()`                  | Read back the current Code Editor contents                               |
| `runtime().codeEditorSetLanguage(lang)`          | Set syntax-highlighting language (GLSL, C++, Lua, Python, …)            |


### `ofkitty::Runtime::attach(window, app)`

Call this once before `ofRunApp()`:

```cpp
auto app = std::make_shared<ofApp>();
ofkitty::Runtime::attach(window, app);
ofRunApp(window, std::move(app));
```

### `ofkitty::Runtime::attach(window, app, registry)`

Pass your app's registry when the app owns ECS state. The Runtime's inspector, status bar, and Scene panel all read from this registry:

```cpp
ofkitty::Runtime::attach(window, app, app->registry());
```

---

## Dependencies

For a vanilla openFrameworks project using `addons.make`, add:

```make
ofxEnTT
ofxImGui
ofxImGuiStyle
ofxEnTTKit
ofxEnTTInspector
ofxKit
```


| Addon              | Role                                                        |
| ------------------ | ----------------------------------------------------------- |
| `ofxEnTT`          | EnTT ECS library                                            |
| `ofxEnTTKit`       | ECS components for OF types + `ofxNode` + `TransformSystem` |
| `ofxEnTTInspector` | ImGui inspector panels for ECS components                   |
| `ofxImGui`         | ImGui integration for openFrameworks                        |
| `ofxImGuiStyle`    | Theme registry (`ImTheme`) + bundled fonts/icons (`ImFonts`) |


In the ofKitty distribution, install/update these dependencies with:

```bash
./scripts/install_ofkitty_spine.sh
```

---

## Keyboard shortcuts


| Key    | Action                   |
| ------ | ------------------------ |
| Cmd-E  | Toggle Edit mode overlay |
| Ctrl-E | Toggle Edit mode overlay |
| F2     | Toggle Rulers            |
| TAB    | Toggle Edit mode (built-in alias; suppressed when ImGui has keyboard focus) |
| W      | Gizmo: Translate (while in Edit mode) |
| E      | Gizmo: Rotate (while in Edit mode)    |
| R      | Gizmo: Scale (while in Edit mode)     |
| X      | Gizmo: Toggle World / Local space     |


---

## Docs

| File                                                          | Topic                                               |
|---------------------------------------------------------------|-----------------------------------------------------|
| [`docs/component-registry.md`](docs/component-registry.md)   | ComponentRegistry API, addon registration, picker   |
| [`docs/scene-window.md`](docs/scene-window.md)               | Scene hierarchy tree, ofxNode traversal, selection  |
| [`docs/layers-panel.md`](docs/layers-panel.md)               | LayersPanel tree, ReorderDragDrop, LayerSystem      |
| [`docs/status-bar.md`](docs/status-bar.md)                   | Status bar layout and implementation notes          |
| [`docs/preferences.md`](docs/preferences.md)                 | App Preferences window — all OF settings            |
| [`docs/appearance.md`](docs/appearance.md)                   | Theme/font integration with `ofxImGuiStyle` (`ImTheme` / `ImFonts`) |
| [`docs/rulers.md`](docs/rulers.md)                           | Rulers overlay — pixel coordinates + mouse tracking |
| [`docs/viewport-window.md`](docs/viewport-window.md)         | Secondary Viewport panel — FBO, camera controls     |
| [`docs/tools.md`](docs/tools.md)                             | File dialog, Gizmo, Code Editor, Path Editor        |
| [`docs/dockspace.md`](docs/dockspace.md)                     | Dockspace central node — passthrough vs opaque      |


---

## Architecture notes

- `ofkitty::Runtime` is a singleton that lives for the duration of the process
- The runtime creates a default registry, but can inspect an app-owned registry passed at attach time
- The overlay is rendered at `OF_EVENT_ORDER_AFTER_APP` so it always draws on top
- `ofxNode` (in `ofxEnTTKit`) is the ECS-native node handle; `ecs::TransformSystem` propagates global transforms once per frame

---

## Architecture

`ofxKit` is intended to stay addon-owned rather than requiring openFrameworks core patches:

```
openFrameworks app
  ├── main.cpp            — calls ofkitty::Runtime::attach(...)
  ├── ofxEnTTKit          — Relationship, LocalTransform, GlobalTransform, ofxNode, TransformSystem
  ├── ofxEnTTInspector    — per-component ImGui panels
  ├── ofxImGui            — ImGui window management
  └── ofxKit              — runtime singleton + Edit mode
```
