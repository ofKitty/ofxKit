# Main Menu Bar

The Runtime owns and renders a `BeginMainMenuBar` each frame. The bar is always present and has this fixed structure (left to right):

```
[App name ▾]  [custom groups...]  [raw callbacks]  [View ▾]
```

- **App name** — set by `runtime().setAppName("My App")` or auto-detected from the executable name. Contains Edit mode toggle, Theme, UI Scale, and Quit.
- **Custom groups** — registered with `addMenuBarGroup`.
- **Raw callbacks** — registered with `addMenuBarRawCallback`.
- **View** — built-in menu; lists every registered `RuntimeWindow` as a visibility toggle.

## Adding a named menu group

`addMenuBarGroup` wraps your callback in its own `BeginMenu / EndMenu` call:

```cpp
ofkitty::runtime().addMenuBarGroup("File", [&]{
    if (ImGui::MenuItem("New",  "Ctrl+N")) newDocument();
    if (ImGui::MenuItem("Open", "Ctrl+O")) openDocument();
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S")) saveDocument();
});
```

Each group name must be unique — a warning is logged if you try to register the same name twice.

## Adding a raw callback

`addMenuBarRawCallback` calls your function directly inside `BeginMainMenuBar` without wrapping it in a menu. Use this when you want full control — for example, to add multiple menus at once or to insert custom widgets into the bar:

```cpp
ofkitty::runtime().addMenuBarRawCallback([&]{
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) redo();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Object")) {
        // ...
        ImGui::EndMenu();
    }
});
```

## Ordering

Groups are drawn in the order they were registered. If you need a specific order relative to other addons, register earlier (e.g. in `main.cpp` before `ofRunApp`).

## View menu entries

Windows registered with `menuGroup == "View"` (the default) automatically appear as checkable items in the **View** menu. To put a window under a different top-level menu, set `menuGroup` to its name when calling `registerWindow` and make sure that menu group is registered via `addMenuBarGroup`.

```cpp
ofkitty::runtime().registerWindow({
    "Audio Mixer",
    "Window",    // will appear under a "Window" menu group
    true, false,
    [](bool& v){ /* ... */ }
});

ofkitty::runtime().addMenuBarGroup("Window", []{
    // any extra Window menu items here; the window toggles are injected automatically
    // if you iterate runtime().windows() yourself
});
```
