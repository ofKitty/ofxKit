# Rulers

Rulers are a pair of thin measurement strips (horizontal + vertical) drawn along the top and left edges of the viewport. They show screen-pixel coordinates relative to the work area origin (top-left = 0, 0), and track the live mouse position with a red cursor marker.

**Toggle:** `View → Rulers`  or  **F2**

---

## Layout

```
 [corner] ──────── horizontal ruler ────────────────────────────────────►  X
    │
    │   vertical ruler
    │
    │
    ▼
    Y
```

| Element         | Description                                                |
|-----------------|------------------------------------------------------------|
| Horizontal strip | Top edge, 20 px tall. Tick marks at 10 / 50 / 100 px intervals, labels at 100 px. |
| Vertical strip   | Left edge, 20 px wide. Same tick intervals. Labels at 100 px. |
| Corner square    | Top-left 20 × 20 px block. Shows `x, y` coordinates when the mouse is in the work area. |
| Red cursor line  | Live vertical line on H ruler + horizontal line on V ruler at the current mouse position. |

---

## Coordinate system

The origin (0, 0) is the **top-left corner of the viewport work area** (below the menu bar, to the right of the ruler strip). This matches openFrameworks' default 2D coordinate system.

Pixel values are **logical ImGui pixels** — they match CSS / HiDPI-unscaled coordinates on most platforms and correspond 1 : 1 to `ofGetMouseX()` / `ofGetMouseY()` when the window has no custom viewport transform.

---

## Programmatic control

```cpp
// Toggle rulers on
ofkitty::runtime().setShowRulers(true);

// Read current state
bool on = ofkitty::runtime().showRulers();

// Toggle
ofkitty::runtime().toggleRulers();
```

---

## Implementation notes

- Rendered via `ImGui::GetForegroundDrawList()` so rulers are always on top of docked panels (but they are 20 px strips at the screen edge and should never obscure content).
- Drawn after `ImGui::DockSpaceOverViewport` and before registered windows, so it doesn't interfere with any ImGui layout.
- No inputs are captured by the ruler strips — mouse events pass through.

---

## Planned additions

- Zoom / scale factor so rulers can show world units (e.g. millimetres for a plotter sketch)
- Draggable guide lines (click on ruler and drag into the viewport)
- Unit selector (px / mm / cm / in)
- Origin relocation (Cmd+click corner to reset to current mouse pos)
