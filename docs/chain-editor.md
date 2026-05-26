# Chain Editor

`ofkitty::ChainEditor` is a generic ImGui widget for **ordered lists of steps**
with drag-reorder, optional enable toggles, collapsing parameter bodies, and an
Add combo. It is domain-agnostic — the host supplies all data and behaviour via
callbacks.

Typical uses:

| App / addon        | Chain contents                          |
|--------------------|-----------------------------------------|
| ofxPlotter         | Image filters, draw filters, pipeline   |
| ofxSendFx (future) | `SendFxChain` effect slots              |
| Any kit sketch     | Modulators, post-FX, automation steps   |

Include via `ofxKit.h` or `#include "ChainEditor.h"`.

---

## Row layout

Each step renders as one row:

```
[≡] [✓]  ▼ step label
           … parameter widgets from setDrawStepBody …
           [Remove]
```

- **≡** — drag handle (Font Awesome grip icon); reorder via drag-drop
- **✓** — optional enabled checkbox (omit `setIsEnabled` / `setSetEnabled` to hide)
- **▼** — `CollapsingHeader` for the step label; body drawn when open

When `setSectionTitle` is set, the whole list is wrapped in an outer
collapsing header showing the title and step count, e.g. `Image filters (3)`.

---

## Minimum setup

```cpp
#include "ofxKit.h"

std::vector<MyStep> steps;

ofkitty::ChainEditor chain;
chain.setPayloadTag("MYAPP_CHAIN");   // unique per list on screen
chain.setStepCount((int)steps.size());
chain.setStepLabel([&](int i) { return steps[i].name; });
chain.setDrawStepBody([&](int i) {
    ImGui::SliderFloat("Amount", &steps[i].amount, 0.f, 1.f);
});
chain.setOnMove([&](int from, int toInsert) {
    MyStep tmp = std::move(steps[from]);
    steps.erase(steps.begin() + from);
    if (toInsert > from) --toInsert;
    steps.insert(steps.begin() + toInsert, std::move(tmp));
});
chain.setOnRemove([&](int i) { steps.erase(steps.begin() + i); });
chain.draw();
```

Call `draw()` inside a registered window lambda or any ImGui panel.

---

## Add combo

Provide type labels with a placeholder at index 0; `onAdd` receives the
selected index (≥ 1) when the user clicks **Add**:

```cpp
chain.setAddTypes({ "(add step)", "blur", "threshold", "invert" });
chain.setOnAdd([&](int typeIndex) {
    if (typeIndex <= 0) return;
    steps.push_back(MyStep::create(types[typeIndex]));
    chain.setStepCount((int)steps.size());  // refresh count each frame or after mutation
});
chain.setAddButtonLabel("Add");           // optional — default "Add"
```

---

## Optional settings

| Setter | Purpose |
|--------|---------|
| `setSectionTitle("Image filters")` | Outer collapsing section with count |
| `setFooterHint("Top runs first.")` | Muted hint text below the list |
| `setShowRemoveButton(false)` | Hide per-step Remove buttons |
| `setPayloadTag("UNIQUE_TAG")` | **Required** when multiple editors share one panel |

Refresh `setStepCount` whenever the backing vector changes (after add, remove,
or reorder).

---

## Drag-and-drop

Reorder uses `ofkitty::ReorderDragDropIndexRow` (flat index lists). Each row
exposes **Before** and **After** drop zones (top/bottom thirds of the header).

`setOnMove(from, toInsert)` receives:

- `from` — dragged step index
- `toInsert` — index where the step should land **after** removal of `from`
  (same convention as `std::vector` insert)

Example move helper:

```cpp
chain.setOnMove([&](int from, int toInsert) {
    if (from < 0 || from >= (int)steps.size()) return;
    toInsert = std::clamp(toInsert, 0, (int)steps.size());
    auto tmp = std::move(steps[from]);
    steps.erase(steps.begin() + from);
    if (toInsert > from) --toInsert;
    steps.insert(steps.begin() + toInsert, std::move(tmp));
});
```

For **entity trees** (layers, scene hierarchy), use `ReorderDragDropRow` with
`entt::entity` payloads instead — see [layers-panel.md](layers-panel.md).

---

## Multiple chains in one panel

Use a **separate `ChainEditor` instance** (or reconfigure one editor) per chain,
each with its own `setPayloadTag`:

```cpp
ofkitty::ChainEditor imageChain;
ofkitty::ChainEditor drawChain;

imageChain.setPayloadTag("PLOTTER_IMG_FX");
drawChain.setPayloadTag("PLOTTER_DRAW_FX");
// … wire each to its vector, then draw both in the same tab
```

Duplicate payload tags on the same screen will cross-contaminate drag-drop.

---

## Example: ofxSendFx

`ofxsendfx::SendFxChain` holds `std::vector<std::unique_ptr<ISendFx>>`.
A SendFx UI panel would mirror the plotter pattern:

```cpp
void drawSendFxChain(std::vector<std::string>& fxIds,
                     const std::function<void(int,int)>& move,
                     const std::function<void(int)>& remove,
                     const std::function<void(const std::string&)>& add) {
    static ofkitty::ChainEditor ui;
    ui.setPayloadTag("SENDFX_CHAIN");
    ui.setSectionTitle("Send effects");
    ui.setFooterHint("Serial order — first effect receives the send bus.");
    ui.setStepCount((int)fxIds.size());
    ui.setStepLabel([&](int i) { return fxIds[i]; });
    ui.setDrawStepBody([&](int i) { drawSendFxParams(fxIds[i]); });
    ui.setOnMove(move);
    ui.setOnRemove(remove);

    auto ids = ofxsendfx::FxFactory::registeredIds();
    std::vector<std::string> addTypes = { "(add effect)" };
    for (const auto& id : ids) addTypes.push_back(id);
    ui.setAddTypes(addTypes);
    ui.setOnAdd([&](int idx) {
        if (idx > 0 && idx <= (int)ids.size()) add(ids[idx - 1]);
    });
    ui.draw();
}
```

Domain logic (`setup`, `process`, parameter keys) stays in ofxSendFx; ChainEditor
only renders and reorders.

---

## Example: ofxPlotter

The plotter example-kit uses three editors on the G-code Generator panel:

1. **Image filters** — pixel ops before plot finder
2. **Draw filters** — GL redraw at canvas size
3. **Pipeline steps** — post-path processors (`line_merge`, `line_sort`, …)

See `GcodeGeneratorPanel::drawEffectChainUI` and `drawPipelineTab` in
`addons/ofxPlotter/example-kit/src/GcodeGeneratorPanel.cpp`.

---

## Related

- [`ReorderDragDrop.h`](../src/ReorderDragDrop.h) — `ReorderDragDropIndexRow` (flat lists) and `ReorderDragDropRow` (entity trees)
- [layers-panel.md](layers-panel.md) — tree reorder with `ReorderDragDropRow`
