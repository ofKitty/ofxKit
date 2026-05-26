# Layers Panel

`ofkitty::LayersPanel` is a generic ImGui layer-management panel for any
`entt::registry` where entities carry `ecs::layer_component` +
`ecs::Relationship`.

It renders a collapsible **tree** (rather than a flat list), supports
**drag-and-drop reparenting** with three drop zones, and exposes clean
callback hooks so domain logic stays outside the widget.

---

## Minimum setup

```cpp
// ofApp.h
ofkitty::LayersPanel m_layers;

// ofApp::setup()
m_layers.setup(&m_registry, &m_activeLayer);

// Inside a registerWindow lambda:
m_layers.draw("Layers###myapp.layers", visible);
```

`LayersPanel` reads and writes `ecs::Relationship` links directly; no other
state is required from the caller.

---

## Binding to a domain engine

If your engine keeps its own flat DFS cache (e.g. `layerOrder`) or needs to
rebuild derived data after a structural change, override the default
behaviour with callbacks:

```cpp
m_layers.setup(&m_engine.registry, &m_engine.activeLayer);

// Called when "+" is clicked — create an entity with domain components
m_layers.setOnAddLayer([this] {
    m_engine.addLayer();
});

// Called when the trash button is clicked — remove with cleanup
m_layers.setOnRemoveLayer([this](entt::entity e) {
    m_engine.removeLayer(e);
    m_engine.rebuildFlatPaths();
});

// Called after DnD / Up / Down — rewires Relationship AND rebuilds cache
m_layers.setOnReparent([this](entt::entity child,
                               entt::entity newParent,
                               entt::entity insertBefore) {
    m_engine.reparentLayer(child, newParent, insertBefore);
    m_engine.rebuildFlatPaths();
});

// Called after visibility, lock, colour, or rename changes
m_layers.setOnLayerChanged([this] {
    m_engine.rebuildFlatPaths();
});
```

When `setOnReparent` is **not** set, the panel performs the Relationship
surgery itself (suitable for standalone use without a flat cache).

---

## Optional row customisation

```cpp
// Right-aligned badge text per row (return "" to hide)
m_layers.setGetBadge([this](entt::entity e) -> std::string {
    auto& sc = reg.get<plotter::toolpath_stats_component>(e);
    return std::to_string(sc.totalPaths) + " paths";
});

// Extra items injected into the per-row right-click context menu
m_layers.setDrawContextMenu([this](entt::entity e) {
    if (ImGui::MenuItem("Generate this layer"))
        m_engine.generateLayer(e);
});
```

---

## Hierarchy — how it works

Layers are stored as entities in an `entt::registry`.  Structure is encoded
via `ecs::Relationship` (a zero-allocation doubly-linked sibling list):

```
parent (entt::null = root)
first_child  ←→  prev/next_sibling chain
children_count
```

The panel finds the **first root** (entity with
`Relationship.parent == null && Relationship.prev_sibling == null`), then
walks the sibling chain and recurses into children for a full DFS traversal.

Visibility is **inherited**: if a parent is hidden, all its descendants are
skipped by `LayerSystem::visibleLayers()` and
`ImageToPath::isEffectivelyVisible()` regardless of their own `visible` flag.

Example tree:

```
Layer 1          root
  Fill           child of Layer 1
  Mask           child of Layer 1
Layer Group      root
  Sub-layer A    child of Layer Group
Layer 2          root
```

---

## Drag-and-drop zones

Each row is split into three vertical hit zones:

| Mouse position | Zone     | Effect                                 |
|---------------|----------|----------------------------------------|
| Top third      | `Before` | Insert dragged as previous sibling     |
| Middle third   | `Into`   | Reparent dragged as child of this row  |
| Bottom third   | `After`  | Insert dragged as next sibling         |

Visual feedback: a **blue line** for `Before`/`After`, a **highlight rect**
for `Into`.

The Up ▲ / Down ▼ toolbar buttons are equivalent to
`Before(prev_sibling)` / `After(next_sibling)`.

---

## `ecs::LayerSystem`

`LayerSystem` (in `ofxEnTTKit`) does the same DFS traversal once per frame
and exposes two pre-built lists:

```cpp
auto* ls = systems.addSystem<ecs::LayerSystem>();

// In draw / render:
for (entt::entity e : ls->visibleLayers()) {
    // e and all its ancestors are visible
}

for (entt::entity e : ls->allLayers()) {
    // every layer entity in DFS order
}
```

Register it **first** in `SystemManager` so other systems can consume the
lists without re-traversing.

---

## `ofkitty::ReorderDragDrop`

The three-zone drop logic is also available as a standalone helper for any
reorderable tree or list:

```cpp
#include "ReorderDragDrop.h"

// Call once per row, immediately after the row's selectable / tree node:
auto r = ofkitty::ReorderDragDropRow(
    "MY_PAYLOAD",          // unique tag — scopes this drag-drop group
    entity,                // entity this row represents
    lc.name.c_str(),       // text shown in the drag tooltip
    rowMinY, rowMaxY);     // screen-space Y extents of the row

if (r.accepted) {
    switch (r.zone) {
    case ofkitty::DropZone::Before:
        reorder(r.dragged, r.target, /*insertBefore=*/r.target);
        break;
    case ofkitty::DropZone::Into:
        reparent(r.dragged, /*newParent=*/r.target);
        break;
    case ofkitty::DropZone::After:
        reorder(r.dragged, r.target, /*insertBefore=*/nextSiblingOf(r.target));
        break;
    }
}
```

`rowMinY` / `rowMaxY` are typically obtained by saving
`ImGui::GetCursorScreenPos().y` before the item and adding
`ImGui::GetFrameHeight()`.

For **flat index lists** (pipeline steps, effect chains, SendFx slots), use
`ReorderDragDropIndexRow` via [`ChainEditor`](chain-editor.md) instead.

---

## `ImageToPath` — layer management API

When using `ofxPlotter`, call these helpers instead of manipulating
`ecs::Relationship` directly.  Each one wires Relationship links **and**
rebuilds the internal `layerOrder` DFS cache.

```cpp
// Add a new root layer (or a child if parent is given)
entt::entity e = engine.addLayer("Fill",  entt::null);
entt::entity c = engine.addLayer("Mask",  e);          // child of Fill

// Remove a layer and all its descendants
engine.removeLayer(e);

// Move / reparent a layer
// insertBefore == entt::null  →  append as last child of newParent
engine.reparentLayer(child, newParent, insertBefore);
```
