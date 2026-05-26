meta:
	ADDON_NAME = ofxKit
	ADDON_DESCRIPTION = ofKitty runtime
	ADDON_AUTHOR = ofKitty
	ADDON_DESCRIPTION = ofKitty runtime kit — composition-first Edit Mode overlay.
	ADDON_TAGS = "addon" "ecs" "inspector" "editor" "ofkitty" "ofxkit"
	ADDON_URL = https://github.com/ofkitty/ofxKit

common:
	ADDON_DEPENDENCIES = ofxEnTTKit ofxEnTTInspector ofxImGui ofxImGuiStyle ofxImGuiTextEdit ofxImGuiFileDialog ofxImGuiVectorEditor ofxImGuizmo

linux64:
vs:
	ADDON_CPPFLAGS = /bigobj
linuxarmv6l:
linuxarmv7l:
android/armeabi:
android/armeabi-v7a:
osx:
ios:
tvos:
emscripten:
	# ofxKit is Emscripten-compatible without additional flags.
	# ImGui multi-viewports are automatically disabled under TARGET_OPENGLES.
	# GLFW is available in Emscripten (emscripten-glfw3 layer).
	# File I/O uses Emscripten's virtual FS; add IDBFS mounting in main.cpp
	# if you need preferences/shortcuts to persist across page reloads.
