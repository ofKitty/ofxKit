# Component Registry (ofxKit consumer)

The **component picker registry is owned by ofxEnTTKit**, not by ofxKit. ofxKit is the ofKitty runtime shell — it finalises and **reads** the registry to drive the Properties panel **"+ Add Component"** picker.

**Canonical API and addon registration:** [ofxEnTTKit/docs/component-registry.md](../../ofxEnTTKit/docs/component-registry.md)

---

## What ofxKit does

On attach, `Runtime::registerBuiltInComponents()` calls `ecs::finalizeComponentMenu()`. That merges:

1. Shipped `ecs::`* rows from ofxEnTTKit (`component_editor_registration.cpp`)
2. Addon rows queued via `ecs::registerComponent()` (e.g. ofxJoystick, ofxGrblKit)

The Properties panel iterates `ecs::componentMenuEntries()` — see `[Runtime_panels.cpp](../src/Runtime_panels.cpp)`.

```cpp
// main.cpp — typical app
ofkitty::Runtime::attach(window, app);  // triggers ecs::finalizeComponentMenu()
```

ofxKit does **not** expose `runtime().registerComponent()`. Addons register through `ecs::registerComponent<T>()` in ofxEnTTKit.

---

## The "Add Component" picker

When an entity is selected in the Properties panel, a **+ Add Component** button appears below the inspectors. Clicking it opens a categorised popup:

- Components the entity **already has** are shown dimmed and non-clickable.
- Clicking a name calls the row's `add` lambda immediately.
- Non-empty `description` appears as a tooltip on hover.

No app code is required — the picker is driven entirely by `ecs::componentMenuEntries()`.

Component **property** inspectors are separate: type-driven via **ofxEnTTInspector**.

---

## Addon checklist

1. `#include "component_editor_registration.h"` (or `ofxEnTTKit.h`)
2. Call `ecs::registerComponent<YourType>("Label", "Category")` before attach, or use static init in your addon
3. Add your addon to the app's `addons.make`
4. Optional: poll hooks (`ecs::InputSystem::setJoystickPollHook`) for input backends — see ofxEnTTKit docs

---

## Related registries (ofxKit)

ofxKit owns other runtime tables that are **not** the component picker:


| Registry         | Owner                    | Purpose                    |
| ---------------- | ------------------------ | -------------------------- |
| Component picker | **ofxEnTTKit** (`ecs::`) | Add Component menu rows    |
| Window registry  | ofxKit                   | Dockable panels, View menu |
| Preference pages | ofxKit                   | Preferences window tabs    |
| Status bar items | ofxKit                   | Status bar widgets         |


System lifecycle registration (`registerSystem`) is planned separately — not part of the component picker registry.