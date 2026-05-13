# Exporting to HTML (Emscripten / WebAssembly)

Build any ofxKit or ofxImGui project as a self-contained web page using
[Emscripten](https://emscripten.org). The result is a `.html` + `.js` + `.wasm`
triple that runs in any modern browser with no installation.

## What works out of the box

| Feature | Status |
|---|---|
| ofxImGui (all widgets, docking) | Works — auto-switches to GLES3 + GLFW backend |
| `ofxImGui::Viewport` input helper | Works — OF wraps browser pointer events |
| `ofxImGui::Canvas` DrawList widgets | Works |
| Mouse scroll / keyboard events | Works |
| ofxKit `ShortcutManager` | Works — keys fire via browser keyboard events |
| ofxKit `ProgressWindow` (threaded) | Works with `-pthread` (default OF emscripten flags) |
| ImGui multi-viewports (tearoff) | Automatically disabled on Emscripten |
| HiDPI scale detection | Returns 1.0f (no GLFW monitor query on web) |
| Preferences / shortcuts JSON | Works via Emscripten virtual FS; see persistence note below |

## Prerequisites

### 1. Emscripten SDK

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh        # add emcc / em++ to PATH
```

On Windows use `emsdk_env.bat` from a Developer PowerShell, or the MSYS2 shell
that ships with openFrameworks.

### 2. openFrameworks Emscripten support

OF's Emscripten makefiles live at:

```
libs/openFrameworksCompiled/project/emscripten/
```

Make sure you have a reasonably recent OF nightly or release that includes
`config.emscripten.default.mk`. Verify with:

```bash
ls $OF_ROOT/libs/openFrameworksCompiled/project/emscripten/
```

## Project setup

### main.cpp — conditional window creation

Replace the standard desktop `main.cpp` with the guarded form:

```cpp
#include "ofMain.h"
#include "ofApp.h"

#ifndef __EMSCRIPTEN__
#include "ofAppGLFWWindow.h"
#endif

int main() {
#ifndef __EMSCRIPTEN__
    // Desktop — OpenGL 3.2 core profile
    ofGLWindowSettings settings;
    settings.setGLVersion(3, 2);
    settings.setSize(1280, 800);
    settings.title = "My App";
    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
#else
    // Web — WebGL2 / GLES3
    ofSetupOpenGL(1280, 800, OF_WINDOW);
    ofRunApp(new ofApp());
#endif
}
```

All three examples in this addon (`example-beziereditor`, `example-pixeleditor`,
`example-Conway`) already include this guard.

### addons.make

No changes needed. `ofxImGui` auto-detects `__EMSCRIPTEN__` via
`ofxImGuiConstants.h` and selects the correct renderer + backend.

## Building

From your project directory (where `Makefile` lives):

```bash
source /path/to/emsdk/emsdk_env.sh   # if not already in PATH

make -j$(nproc) PLATFORM_OS=Emscripten
```

Output lands in `bin/`:

```
bin/
  myapp.html      ← open this in a browser
  myapp.js
  myapp.wasm
  myapp.data      ← virtual FS (bin/data contents)
```

### Debug build

```bash
make -j$(nproc) PLATFORM_OS=Emscripten Debug
```

### Clean

```bash
make clean PLATFORM_OS=Emscripten
```

## Serving locally

Browsers block `file://` for WebAssembly. Serve over HTTP:

```bash
# Python (simplest)
cd bin
python3 -m http.server 8080
# open http://localhost:8080/myapp.html

# OR use emrun (ships with emsdk, handles COOP/COEP headers for threads)
emrun --port 8080 bin/myapp.html
```

> **Threading note:** if your app uses `std::thread` or `ofxKit::ProgressWindow`
> from a worker thread, the server must send the COOP/COEP headers that enable
> `SharedArrayBuffer`. `emrun` does this automatically; `python3 -m http.server`
> does not. Use `emrun` or a custom server for threaded builds.

## Persisting preferences across reloads

By default Emscripten's virtual FS is in-memory and resets on every page load.
`ShortcutManager` and the UI-scale pref file will work during a session but won't
survive a refresh. To persist them, mount IDBFS in your `ofApp::setup()`:

```cpp
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void ofApp::setup() {
    // Mount IndexedDB-backed FS at the data directory
    EM_ASM(
        FS.mkdir('/data');
        FS.mount(IDBFS, {}, '/data');
        FS.syncfs(true, function(err) {});   // pull from IndexedDB on load
    );
    // ... rest of setup
}

void ofApp::exit() {
    EM_ASM( FS.syncfs(false, function(err) {}); );  // push to IndexedDB on quit
}
#endif
```

For most demos (Conway, bezier editor, pixel editor) in-session state is fine and
no IDBFS setup is needed.

## Known differences from desktop

| Behaviour | Desktop | Emscripten |
|---|---|---|
| ImGui tearoff windows | Available | Disabled (no multi-viewports) |
| HiDPI scale | Detected from monitor | Always 1.0f |
| `ofExit()` | Closes window | No-op / stops loop |
| File persistence | Native FS | Emscripten virtual FS (IDBFS for persistence) |
| Scroll events | Mouse wheel | Browser wheel events (same OF callback) |
| Right-click context | Native | Browser may intercept on some platforms |

## Troubleshooting

**Black screen / nothing renders**
Check the browser console for WebGL errors. Usually a GL version mismatch —
make sure `ofSetupOpenGL` is used (not `ofGLWindowSettings.setGLVersion(3,2)`).

**`SharedArrayBuffer` not available**
Add COOP/COEP headers to your server (use `emrun`) or disable threading by
commenting out `std::mutex` usage in `ProgressWindow` for web-only builds.

**`GLFW/glfw3.h` not found**
Your Emscripten SDK version may predate `emscripten-glfw3`. Update emsdk to
latest: `./emsdk install latest && ./emsdk activate latest`.

**ImGui ini file not loading**
The `imgui.ini` lives in Emscripten's virtual FS. It is regenerated each session
unless you mount IDBFS (see above). This is usually fine for demos.

**`ofAppGLFWWindow.h` compile error on Emscripten**
You forgot the `#ifndef __EMSCRIPTEN__` guard around the include in `main.cpp`.
