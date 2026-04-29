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

Use this when your app already owns the ECS state and you want ofxKit to inspect it.

```cpp
// ofApp.h
#pragma once
#include "ofMain.h"
#include "ofxKit.h"

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
auto window = ofCreateWindow(settings);
auto app = std::make_shared<ofApp>();
ofkitty::Runtime::attach(window, app, app->registry());
ofRunApp(window, std::move(app));
```

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

Component and system registration should follow the same rule: addons opt in, `ofxKit` orchestrates, and core addons do not gain UI/runtime dependencies by accident.

The intended component API should replace closed inspector lists with registered component metadata:

```cpp
ofkitty::runtime().registerComponent<grbl::MachineStateComponent>({
    .name = "Machine State",
    .inspect = [](entt::registry& registry, entt::entity entity) {
        auto& state = registry.get<grbl::MachineStateComponent>(entity);
        ImGui::Text("State: %s", state.label.c_str());
    },
});
```

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


| Member                   | Description                                                           |
| ------------------------ | --------------------------------------------------------------------- |
| `Runtime::instance()`    | Returns the singleton                                                 |
| `runtime()`              | Free-function shorthand for `Runtime::instance()`                     |
| `runtime().registry()`   | The active `entt::registry` (owned by runtime or supplied by the app) |
| `runtime().selected()`   | Currently selected entity (`entt::null` if none)                      |
| `runtime().select(e)`    | Programmatically select an entity                                     |
| `runtime().isEditMode()` | Whether the overlay is currently visible                              |


### `ofkitty::Runtime::attach(window, app)`

Call this once before `ofRunApp()`:

```cpp
auto app = std::make_shared<ofApp>();
ofkitty::Runtime::attach(window, app);
ofRunApp(window, std::move(app));
```

### `ofkitty::Runtime::attach(window, app, registry)`

Call this when the app owns the ECS registry:

```cpp
ofkitty::Runtime::attach(window, app, app->registry());
```

---

## Dependencies

For a vanilla openFrameworks project using `addons.make`, add:

```make
ofxEnTT
ofxImGui
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

