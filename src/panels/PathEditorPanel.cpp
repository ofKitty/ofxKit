#include "PathEditorPanel.h"

#include "ofxEnTTKit_all.h"

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
                            prev.handleOut    = ImVec2(cmd.cp1.x - prev.position.x,
                                                       cmd.cp1.y - prev.position.y);
                            prev.hasHandleOut = true;
                        }
                        ImVectorEditor::Anchor a;
                        a.position    = ImVec2(cmd.to.x, cmd.to.y);
                        a.handleIn    = ImVec2(cmd.cp2.x - cmd.to.x,
                                               cmd.cp2.y - cmd.to.y);
                        a.hasHandleIn = true;
                        m_path.anchors.push_back(a);
                    } else if (cmd.type == ofPath::Command::quadBezierTo) {
                        // cmd.cp1 = previous position (P0), cmd.cp2 = control point (CP), cmd.to = endpoint (P2)
                        // Promote quadratic to cubic: CP1 = P0 + 2/3*(CP-P0), CP2 = P2 + 2/3*(CP-P2)
                        glm::vec2 p0(cmd.cp1.x, cmd.cp1.y);
                        glm::vec2 cp(cmd.cp2.x, cmd.cp2.y);
                        glm::vec2 p2(cmd.to.x,  cmd.to.y);
                        glm::vec2 cp1 = p0 + (2.f / 3.f) * (cp - p0);
                        glm::vec2 cp2 = p2 + (2.f / 3.f) * (cp - p2);
                        if (!m_path.anchors.empty()) {
                            auto& prev        = m_path.anchors.back();
                            prev.handleOut    = ImVec2(cp1.x - prev.position.x,
                                                       cp1.y - prev.position.y);
                            prev.hasHandleOut = true;
                        }
                        ImVectorEditor::Anchor a;
                        a.position    = ImVec2(p2.x, p2.y);
                        a.handleIn    = ImVec2(cp2.x - p2.x, cp2.y - p2.y);
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
        for (size_t i = 0; i < m_path.anchors.size(); ++i) {
            const auto& cur = m_path.anchors[i];
            glm::vec3 pos(cur.position.x, cur.position.y, 0.f);
            if (i == 0) {
                pc.path.moveTo(pos);
            } else {
                const auto& prev = m_path.anchors[i - 1];
                if (cur.hasHandleIn || prev.hasHandleOut) {
                    glm::vec3 cp1(prev.position.x + prev.handleOut.x,
                                  prev.position.y + prev.handleOut.y, 0.f);
                    glm::vec3 cp2(cur.position.x + cur.handleIn.x,
                                  cur.position.y + cur.handleIn.y, 0.f);
                    pc.path.bezierTo(cp1, cp2, pos);
                } else {
                    pc.path.lineTo(pos);
                }
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
