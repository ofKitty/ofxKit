# Component Registry

`ofxKit` ships with a **ComponentRegistry** — a runtime table of every ECS component type that the editor knows about. The registry drives the **"+ Add Component"** picker in the Properties panel and gives addons a single, consistent place to announce their components to the UI.

---

## How it works

When `ofxKit` initialises it calls `registerBuiltInComponents()`, which pre-populates the registry with every `ofxEnTTKit` component type (~50 entries, 10 categories). Each entry carries:

| Field         | Purpose                                                        |
|---------------|----------------------------------------------------------------|
| `name`        | Display label in the picker                                    |
| `category`    | Group header (`"3D"`, `"2D"`, `"Media"`, …)                  |
| `description` | Tooltip text (optional)                                        |
| `has`         | `bool(registry, entity)` — is the component already attached? |
| `add`         | `void(registry, entity)` — attach / initialise the component  |
| `remove`      | `void(registry, entity)` — detach the component               |

---

## Template shorthand  *(recommended for addon authors)*

`has` and `remove` are generated automatically from `T`. Supply `add` only when the default `emplace<T>(entity)` isn't enough (e.g. a component that needs a post-construction `rebuild()` call).

```cpp
// minimal — default emplace<T>(entity) is fine
ofkitty::runtime().registerComponent<grbl::MachineStateComponent>(
    "Machine State", "Machines");

// custom add — extra initialisation needed
ofkitty::runtime().registerComponent<grbl::MachineStateComponent>(
    "Machine State", "Machines",
    [](entt::registry& r, entt::entity e) {
        r.emplace<grbl::MachineStateComponent>(e).connect("/dev/ttyUSB0");
    });
```

The template is defined in `Runtime.h` and only needs `<entt.hpp>` — your addon header does not need to pull in `ofxEnTTKit`.

---

## Full-control registration

For components with non-trivial lifecycle, supply all three lambdas explicitly:

```cpp
ofkitty::runtime().registerComponent({
    .name        = "Machine State",
    .category    = "Machines",
    .description = "GRBL controller connection state",
    .has    = [](entt::registry& r, entt::entity e) {
                  return r.all_of<grbl::MachineStateComponent>(e); },
    .add    = [](entt::registry& r, entt::entity e) {
                  r.emplace<grbl::MachineStateComponent>(e); },
    .remove = [](entt::registry& r, entt::entity e) {
                  r.remove<grbl::MachineStateComponent>(e); },
});
```

---

## Where to register

Register in your addon's `setup()` or in a **kit-init helper** that app authors call from their `ofApp::setup()`:

```cpp
// ofxGrblKit/src/kit/grbl_kit.cpp
void grbl::kit::registerComponents() {
    auto& rt = ofkitty::runtime();
    rt.registerComponent<grbl::MachineStateComponent>("Machine State", "Machines");
    rt.registerComponent<grbl::MotionPlannerComponent>("Motion Planner","Machines");
    rt.registerComponent<grbl::CoordinateSystemComponent>("WCS", "Machines");
}
```

```cpp
// ofApp.cpp
void ofApp::setup() {
    grbl::kit::registerComponents();   // registers into ofkitty::runtime()
    // …
}
```

---

## Querying the registry

```cpp
// all descriptors in registration order
const auto& descs = ofkitty::runtime().componentDescriptors();

// unique category names (in registration order)
std::vector<std::string> cats = ofkitty::runtime().componentCategories();

// check / add / remove manually
for (auto& d : descs) {
    if (d.name == "Mesh" && !d.has(reg, e))
        d.add(reg, e);
}
```

---

## Built-in categories

| Category    | Examples                                               |
|-------------|--------------------------------------------------------|
| Transform   | Node, Tag, Selectable, File Path                       |
| 3D          | Mesh, Render, Light, Material, Shader, Trail, Cubemap… |
| 2D          | Circle, Rectangle, Bezier, Spline, Text, Sprite…      |
| Rendering   | Post FX, Canvas FX                                     |
| Media       | Image, Video, FBO, FBO Reference                       |
| Camera      | Camera                                                 |
| Animation   | Tween, Particles                                       |
| Modulation  | Modulator, Mod Binding                                 |
| Color       | Color Swatches, Color Gradient                         |
| Hardware    | Serial, OSC, Audio Source, MIDI                        |

---

## The "Add Component" picker

When an entity is selected in the Properties panel a **`+ Add Component`** button appears below the inspectors. Clicking it opens a categorised popup:

- Components the entity **already has** are shown dimmed and non-clickable.
- Clicking a component name calls its registered `add` lambda immediately.
- If `description` is non-empty it appears as a tooltip on hover.

No extra code is needed — the picker is driven entirely by the registered descriptors.
