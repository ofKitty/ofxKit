#include "PathEditorPanel.h"

#include <ofxEnTTKit/src/ofxEnTTKit.h>

#include <imgui.h>

#include "ofMain.h"

namespace ofkitty {

void PathEditorPanel::draw(bool& visible, entt::registry& reg, entt::entity selected)
{
    ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Path Editor###ofxkit.window.path_editor", &visible)) {
        ImGui::End();
        return;
    }

    if (selected != m_lastEntity) {
        m_lastEntity = selected;
        m_path.clear();

        if (selected != entt::null && reg.valid(selected)) {
            if (auto* pc = reg.try_get<ecs::path_component>(selected)) {
                const auto& cmds = pc->path.getCommands();
                for (size_t i = 0; i < cmds.size(); ++i) {
                    const auto& cmd = cmds[i];
                    if (cmd.type == ofPath::Command::moveTo || cmd.type == ofPath::Command::lineTo) {
                        ImVectorEditor::Anchor a;
                        a.position = ImVec2(cmd.to.x, cmd.to.y);
                        m_path.anchors.push_back(a);
                    } else if (cmd.type == ofPath::Command::bezierTo) {
                        if (!m_path.anchors.empty()) {
                            auto& prev        = m_path.anchors.back();
                            prev.handleOut    = ImVec2(cmd.cp1.x, cmd.cp1.y);
                            prev.hasHandleOut = true;
                        }
                        ImVectorEditor::Anchor a;
                        a.position    = ImVec2(cmd.to.x, cmd.to.y);
                        a.handleIn    = ImVec2(cmd.cp2.x, cmd.cp2.y);
                        a.hasHandleIn = true;
                        m_path.anchors.push_back(a);
                    } else if (cmd.type == ofPath::Command::close) {
                        m_path.closed = true;
                    }
                }
            }
        }
    }

    bool isPen = (m_config.tool == ImVectorEditor::Tool::Pen);
    if (ImGui::SmallButton(isPen ? "Pen (active)" : "Pen"))
        m_config.tool = ImVectorEditor::Tool::Pen;
    ImGui::SameLine();
    if (ImGui::SmallButton(!isPen ? "Select (active)" : "Select"))
        m_config.tool = ImVectorEditor::Tool::Select;
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear"))
        m_path.clear();

    ImGui::SameLine();
    bool canApply = (m_lastEntity != entt::null && reg.valid(m_lastEntity)
                     && reg.all_of<ecs::path_component>(m_lastEntity));
    ImGui::BeginDisabled(!canApply);
    if (ImGui::SmallButton("Apply to Entity")) {
        auto& pc = reg.get<ecs::path_component>(m_lastEntity);
        pc.path.clear();
        bool first = true;
        for (auto& anchor : m_path.anchors) {
            glm::vec3 pos(anchor.position.x, anchor.position.y, 0.f);
            if (first) {
                pc.path.moveTo(pos);
                first = false;
            } else if (anchor.hasHandleIn || anchor.hasHandleOut) {
                glm::vec3 cp1(anchor.handleIn.x, anchor.handleIn.y, 0.f);
                glm::vec3 cp2(anchor.handleOut.x, anchor.handleOut.y, 0.f);
                pc.path.bezierTo(cp1, cp2, pos);
            } else {
                pc.path.lineTo(pos);
            }
        }
        if (m_path.closed)
            pc.path.close();
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    m_config.canvasSize = ImGui::GetContentRegionAvail();
    m_widget.Draw("##pathEditor", m_path, m_config);

    ImGui::End();
}

}
