# Scene Window

The Scene panel shows every entity in the active registry as a **live hierarchy tree**. It is registered by the Runtime at startup and visible by default when Edit mode is on.

---

## Entity display

The panel draws entities in two passes:

1. **ofxNode hierarchy** — entities that carry `ecs::Relationship` are drawn as a recursive tree. Root nodes (whose `Relationship.parent == entt::null`) are the top-level entries; children are indented beneath them.
2. **Legacy flat nodes** — entities with `ecs::node_component` but *without* `ecs::Relationship` appear as leaf entries below the tree. These are compatible with the older `node.setPosition()` / `ofNode`-style workflow.

Entities with neither component type are internal ECS state and are intentionally hidden from the panel.

---

## ofxNode traversal

When a node has all three ofxNode components (`Relationship`, `LocalTransform`, `GlobalTransform`), the panel uses `ofxNode::fromEntity` + `forEachChild` for traversal — the canonical ECS-native approach. For entities that carry only a `Relationship` (e.g. constructed manually), the panel falls back to walking the linked-list (`first_child → next_sibling`) directly.

```
// in Runtime.cpp — drawEntityNode() anonymous-namespace helper
if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e)) {
    ofxNode node = ofxNode::fromEntity(reg, e);
    node.forEachChild([&](ofxNode child) {
        drawEntityNode(reg, child.entity(), selected);
    });
} else {
    // Relationship linked-list fallback
    entt::entity child = rel->first_child;
    while (child != entt::null) { … }
}
```

---

## Entity names

Display names are resolved in priority order:

1. `ofxNode::getName()` — set via `node.setName("My Object")`
2. `ecs::node_component::getName()` — set in the component constructor or `nd.setName(…)`
3. Fallback string `"Entity <id>"`

---

## Selection

Clicking an entity in the Scene panel selects it. The selection is shared with the Properties panel via `ofkitty::runtime().selected()`. Programmatic selection works the same way:

```cpp
ofkitty::runtime().select(myEntity);
```

---

## Creating ofxNode hierarchy at runtime

Use `ofxNode::createNode` to create hierarchy-aware entities. The Scene panel will pick them up automatically:

```cpp
auto& reg  = ofkitty::runtime().registry();
auto  root = ofxNode::createNode(reg, "Root");
auto  arm  = ofxNode::createNode(reg, "Arm");
arm.setParent(root);
arm.setPosition({100, 0, 0});
```

---

## Planned additions

- Right-click context menu (rename, duplicate, delete, add child)
- Drag-and-drop reparenting
- Visibility toggle per entity
- Filter / search bar
