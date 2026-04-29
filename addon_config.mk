meta:
	ADDON_NAME = ofxKit
	ADDON_DESCRIPTION = ofKitty runtime kit — composition-first Edit mode overlay for any ofBaseApp sketch.
	ADDON_AUTHOR = @gitbruno
	ADDON_TAGS = "addon" "ecs" "inspector" "editor" "ofkitty" "ofxkit"
	ADDON_URL = https://github.com/ofkitty/ofxKit

common:
	ADDON_DEPENDENCIES = ofxEnTTKit ofxEnTTInspector ofxImGui

	# $(OF_ROOT)/addons needed so cross-addon includes resolve correctly.
	ADDON_INCLUDES += $(OF_ROOT)/addons

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
