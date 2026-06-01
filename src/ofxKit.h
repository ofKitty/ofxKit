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
#include "ViewWindow.h"
#include "ShortcutManager.h"
#include "ProgressWindow.h"

// Expose the full ofxEnTTKit component library (ecs:: types, ofxNode, …)
// so that any file that does #include "ofxKit.h" can use ECS components
// directly without needing a separate #include "ofxEnTTKit_all.h".
#include "ofxEnTTKit_all.h"

// GuiEventHelper registers event filters at OF_EVENT_ORDER_BEFORE_APP that
// block OF mouse/keyboard events from reaching the app when ImGui has claimed
// them (WantCaptureMouse / WantCaptureKeyboard).  Without this, the camera
// and other OF input handlers fight with ImGui for every drag and click.
#include "GuiEventHelper.h"

// Generic named-layer stack + ImGui panel widget
#include "LayerStack.h"

// ECS-based layer panel — works with any registry tagged with ecs::layer_component
#include "panels/LayersPanel.h"

// Generic resource browser panel (images, SVGs, G-code snippets)
#include "panels/ResourcesPanel.h"

// Generic drag-and-drop reorder / reparent helper (3-zone: Before / Into / After)
#include "ReorderDragDrop.h"

// Re-orderable step chains (effects, pipeline, SendFx, …)
#include "ChainEditor.h"
#include "EmbeddedCodeEditor.h"

// ECS system for 2D bounding-box transform handles (requires ImGui + ofxEnTTKit)
#include "TransformHandle2DSystem.h"

// ofPath hit-testing, ImVectorEditor conversion, ImDrawList path strokes
#include "OfPathUtil.h"

// Click-vs-drag rectangle selection for 2D viewport overlays
#include "MarqueeSelect2D.h"

// Pan/zoom coordinate state usable in both ImGui viewports and OF main scene
#include "View2DState.h"
#include "View2DMouseAdapter.h"
