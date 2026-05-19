#pragma once

#include <entt.hpp>
#include <ofxImGuiVectorEditor/src/ofxImGuiVectorEditor.h>

namespace ofkitty {

class PathEditorPanel {
public:
    void draw(bool& visible, entt::registry& reg, entt::entity selected);

private:
    ImVectorEditor::Editor m_widget;
    ImVectorEditor::Path   m_path;
    ImVectorEditor::Config m_config;
    entt::entity           m_lastEntity {entt::null};
};

}
