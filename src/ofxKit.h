#pragma once

// ============================================================================
// ofxKit — ofKitty runtime
// ============================================================================
// ofKitty project templates include this from ofApp.h. In main.cpp, create the
// app shared_ptr, call ofkitty::Runtime::attach(window, app), then pass the app
// to ofRunApp(window, std::move(app)).
//
// Press Cmd-E at runtime to toggle the Edit-mode inspector overlay.
// ============================================================================

#include "ofAppRunner.h"
#include "Runtime.h"
#include "ShortcutManager.h"
#include "ProgressWindow.h"
