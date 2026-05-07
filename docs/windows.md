# Windows

`Runtime` manages a list of named ImGui windows. Any addon or sketch can register its own window and it will be drawn automatically each frame (inside the ImGui context that the Runtime owns). Registered windows also appear as toggle items in the **View** menu of the main menu bar.

## Registering a window

```cpp
ofkitty::runtime().registerWindow({
    "My Panel",          // name — must be unique; also used as ImGui window title
    "View",              // menuGroup — which top-level menu the toggle lives under
    true,                // visible — initial visibility
    false,               // editModeOnly — false = always drawn, true = only in Edit mode
    [](bool& visible) {  // draw callback
        if (ImGui::Begin("My Panel", &visible)) {
            ImGui::Text("Hello from my panel");
        }
        ImGui::End();
    }
});
```

`registerWindow` returns a `RuntimeWindow*` you can keep to change `visible` programmatically, or `nullptr` if the name was already taken.

## RuntimeWindow fields

| Field | Type | Description |
|---|---|---|
| `name` | `std::string` | Unique window name and ImGui title |
| `menuGroup` | `std::string` | Menu group the visibility toggle appears under (default `"View"`) |
| `visible` | `bool` | Current visibility; toggled by the menu item |
| `editModeOnly` | `bool` | When `true`, the window is only drawn while Edit mode is active |
| `draw` | `function<void(bool&)>` | Called each frame when `visible` is true |

## Programmatic visibility control

```cpp
ofkitty::runtime().setWindowVisible("My Panel", false); // hide
ofkitty::runtime().setWindowVisible("My Panel", true);  // show

// Direct pointer — valid as long as the Runtime is alive
auto* win = ofkitty::runtime().findWindow("My Panel");
if (win) win->visible = !win->visible;
```

## Built-in windows

| Name | editModeOnly | Description |
|---|---|---|
| `Toolbar` | false | Floating tool-picker panel (see [toolbar.md](toolbar.md)) |
| `Scene` | true | Entity list for the attached EnTT registry |
| `Properties` | true | Component inspector for the selected entity |
| `Shortcuts` | true | Editable keyboard shortcut list |

The Toolbar is always drawn because tools need to be accessible outside Edit mode.
