# Status Bar

The **status bar** is a thin strip pinned to the bottom of the main viewport. It appears when Edit mode is active and disappears when Edit mode is off.

---

## Layout

Items are rendered left-to-right. A `|` separator is inserted between items belonging to different groups:

```
  Edit Mode  |  ofKitty  |  entities: 12  |  60 fps  |  CTRL+E  toggle edit mode
```

Built-in items (all in group `ofxkit` or `ofxkit.stats`/`ofxkit.hint`) are registered first; addon items follow in registration order.

---

## Built-in items

| Id                         | Group          | Content                                              |
|----------------------------|----------------|------------------------------------------------------|
| `ofxkit.status.editmode`   | `ofxkit`       | Green **"Edit Mode"** badge                          |
| `ofxkit.status.appname`    | `ofxkit`       | App name (from `setAppName()`)                       |
| `ofxkit.status.entities`   | `ofxkit.stats` | Live entity count from the active registry           |
| `ofxkit.status.fps`        | `ofxkit.stats` | Rolling average FPS from `ImGui::GetIO().Framerate`  |
| `ofxkit.status.hint`       | `ofxkit.hint`  | `CTRL+E  toggle edit mode`                           |

Visibility of each built-in item can be toggled from **Preferences → openFrameworks → Status Bar**.

---

## Implementation notes

- Rendered via `ImGui::BeginViewportSideBar("##StatusBar", ..., ImGuiDir_Down, ...)` from `imgui_internal.h`. This pins a window to the bottom of the main viewport and **claims that strip from the work area** before the dockspace sees it.
- **Call order matters:** `drawStatusBar()` is called before `DockSpaceOverViewport` in `drawOverlay()`, so the dockspace fills only the area above the bar and `PassthruCentralNode` is never obscured by the bar.
- Implemented as a `MenuBar` window (`ImGuiWindowFlags_MenuBar` + `BeginMenuBar`) for correct text baseline and theme-consistent background.
- `ImGui::End()` is called unconditionally (outside the `if` block), following the standard ImGui contract.
- Height is `ImGui::GetFrameHeight()` logical pixels, which scales with `UIScale`.

---

## Addon integration — `registerStatusItem()`

Addons can push their own items into the status bar:

```cpp
// In setup() or a kit-init helper
ofkitty::runtime().registerStatusItem({
    "mygrbl.status.connection",   // unique id
    "mygrbl",                     // group (separator before a new group)
    true,                         // visible by default
    [&]() {                       // draw callback — emit compact ImGui widgets
        if (sender.isConnected())
            ImGui::TextColored({0.4f, 0.9f, 0.5f, 1.f}, "GRBL connected");
        else
            ImGui::TextDisabled("GRBL offline");
    }
});
```

### `StatusItem` descriptor

| Field     | Type                    | Description                                             |
|-----------|-------------------------|---------------------------------------------------------|
| `id`      | `std::string`           | Globally unique id (e.g. `"mygrbl.status.connection"`)  |
| `group`   | `std::string`           | Group label; a `|` separator appears between groups     |
| `visible` | `bool`                  | Whether the item is shown (user-configurable)           |
| `draw`    | `std::function<void()>` | Widget draw callback — called inside the menu-bar scope |

### API

```cpp
// Register
ofkitty::runtime().registerStatusItem(item);

// Unregister — returns false if id not found
bool removed = ofkitty::runtime().unregisterStatusItem("mygrbl.status.connection");

// Access all items (for custom iteration or rendering)
std::vector<Runtime::StatusItem>& items = ofkitty::runtime().statusItems();
```

### Visibility

Each item's `visible` bool is controlled by the user via **Preferences → openFrameworks → Status Bar**. Your addon should respect it — the built-in render loop skips items where `visible == false`.

### Groups and separators

Items sharing the same non-empty `group` string are rendered consecutively without a separator. When the group changes, a `|` is inserted before the first item of the new group:

```
[group A item 1] [group A item 2] | [group B item 1] | [group C item 1]
```

Items with an empty `group` are treated as their own unique group (separator always added after the previous non-empty group).
