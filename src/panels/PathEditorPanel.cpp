#include "PathEditorPanel.h"

#include "OfPathUtil.h"
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
            if (auto* pc = reg.try_get<ecs::path_component>(selected))
                loadAnchorsFromPath(pc->path, m_path);
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
        ofPathFromEditorPath(m_path, pc.path);
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    m_config.canvasSize = ImGui::GetContentRegionAvail();
    m_widget.Draw("##pathEditor", m_path, m_config);

    ImGui::End();
}

}
