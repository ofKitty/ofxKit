# Dockspace and Central Node

When Edit mode is active, the Runtime creates a full-screen **ImGui dockspace** via `DockSpaceOverViewport`. The dockspace divides the window into docked panels on the sides and a **central node** — the unfilled area in the middle.

---

## Default behaviour — passthrough central node

By default the central node is **transparent** (`PassthruCentralNode`). The raw OpenGL scene rendered in `ofApp::draw()` shows through the gap between docked panels. This is correct for sketches that use the OF canvas as their primary view (3-D scenes, 2-D drawing, generative graphics, etc.).

```
┌──────────────────────────────────────────────┐
│  Menu bar                                    │
├─────────┬──────────────────────┬─────────────┤
│  Scene  │                      │  Properties │
│  panel  │  Central node        │  panel      │
│  (left) │  (transparent hole)  │  (right)    │
│         │  OF canvas shows     │             │
│         │  through here        │             │
└─────────┴──────────────────────┴─────────────┘
│  Status bar                                  │
└──────────────────────────────────────────────┘
```

---

## Opaque central node — for panel-only apps

Apps that render **entirely inside ImGui panels** (e.g. a plotter tool whose Preview window is a self-contained ImGui widget) do not need the passthrough. Enabling `PassthruCentralNode` in those apps leaves the centre as a blank gap.

Call `setPassthruCentralNode(false)` in `setup()` to make the central node **opaque and dockable**:

```cpp
void ofApp::setup()
{
    ofkitty::runtime().setPassthruCentralNode(false);
    // ... rest of setup
}
```

With this flag off:

- The central area gets an opaque background (drawn by ImGui / the current theme).
- Windows can be docked **into** the central node.
- `NoDockingOverCentralNode` is also cleared, so a panel dragged to the centre will dock normally rather than being rejected.

```
┌──────────────────────────────────────────────┐
│  Menu bar                                    │
├─────────┬──────────────────────┬─────────────┤
│  Serial │                      │  Preview    │
│  panel  │  Central node        │  panel      │
│  (left) │  (opaque, dockable)  │  (right)    │
│  Image  │  Windows can dock    │             │
│  panel  │  here too            │             │
└─────────┴──────────────────────┴─────────────┘
│  Status bar                                  │
└──────────────────────────────────────────────┘
```

---

## API

```cpp
// Read current state (default: true)
bool passthru = ofkitty::runtime().passthruCentralNode();

// Disable passthrough — call before or in setup()
ofkitty::runtime().setPassthruCentralNode(false);

// Re-enable (e.g. when switching to a 3-D scene mode)
ofkitty::runtime().setPassthruCentralNode(true);
```

The flag takes effect from the next rendered frame. If the dockspace layout has already been built (ini file present), you may need to call **View → Reset Layout** to re-run `buildDefaultDockLayout` with the updated flag.

---

## Default layout seeding

`buildDefaultDockLayout` (run when no `imgui.ini` exists, or after **View → Reset Layout**) respects the same flag:

- `passthruCentralNode == true` — `DockBuilderAddNode` includes `PassthruCentralNode | NoDockingOverCentralNode`.
- `passthruCentralNode == false` — only `ImGuiDockNodeFlags_DockSpace` is used; the centre is a plain dockable node with no special flags.

Use `addDefaultLayoutLeftDock` / `addDefaultLayoutRightDock` in `setup()` to seed which windows land in the left/right splits on first launch.

---

## Implementation notes

The flag is checked in three places in the Runtime:

| Location | What it controls |
|---|---|
| `Runtime::drawOverlay` — `DockSpaceOverViewport` call | Per-frame dockspace flags |
| `Runtime::drawOverlay` — central node `LocalFlags` patch | Per-frame central node flags |
| `Runtime::buildDefaultDockLayout` — `DockBuilderAddNode` + central node patch | First-run / reset layout flags |
