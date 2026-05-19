# Keyboard Shortcuts

`ShortcutManager` (accessible via `runtime().keys()`) is a single dispatch table for all keyboard shortcuts in the application. The Runtime routes `keyPressed` events through it after ImGui has had first refusal.

## Named actions (persistent, remappable)

Named actions are saved to `bin/data/shortcuts.json` (or `bin/data/<Runtime::dataSubdir()>/shortcuts.json` if a subdir is configured) and can be remapped by the user in the **Shortcuts** window (Edit mode).

```cpp
ofkitty::runtime().keys().registerAction(
    "myapp.save",      // stable dot-namespaced id
    's',               // default key (OF key code or ASCII)
    OF_KEY_CONTROL,    // modifier bitmask
    "Save project",    // human-readable description shown in the Shortcuts window
    [&]{ saveProject(); }
);
```

Replace an existing registration with the same id — the most recent call wins.

### Modifier constants

| Constant | Meaning |
|---|---|
| `OF_KEY_CONTROL` | Ctrl (Cmd on macOS when using `OF_KEY_COMMAND`) |
| `OF_KEY_COMMAND` | Cmd (macOS only) |
| `OF_KEY_SHIFT` | Shift |
| `OF_KEY_ALT` | Alt / Option |

Combine with `|`: `OF_KEY_CONTROL | OF_KEY_SHIFT`.

**Matching rule**: all modifiers in the bitmask must be held; any *extra* modifiers the user holds are ignored.

## Anonymous shortcuts (not persisted, not remappable)

```cpp
ofkitty::runtime().keys().bind(
    OF_KEY_ESC, 0,
    "Cancel current action",
    [&]{ cancel(); }
);
```

Anonymous shortcuts are not listed in the JSON file and appear as read-only rows in the Shortcuts window.

## Removing a shortcut

```cpp
ofkitty::runtime().keys().unbind('s', OF_KEY_CONTROL);
```

Removes all bindings (named or anonymous) that match the key + modifier combo.

## Loading / saving JSON bindings

Bindings load automatically from the default path (see above) at startup and save automatically whenever a named action is remapped via the UI.

```cpp
// Manual reload (e.g. after the user edits the file externally)
runtime().keys().loadBindingsFromFile(ShortcutManager::defaultBindingsPath());

// Manual save
runtime().keys().saveBindingsToFile(ShortcutManager::defaultBindingsPath());
```

## Built-in shortcuts

| Action id | Default | Description |
|---|---|---|
| `ofkitty.toggle_edit` | Ctrl-E / Cmd-E | Toggle Edit mode |
