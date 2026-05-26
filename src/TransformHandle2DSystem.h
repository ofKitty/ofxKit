#pragma once
//
//  TransformHandle2DSystem.h  —  ofxKit
//
//  ECS system that draws a 2D bounding-box transform handle for every entity
//  that has both a selectable_component (selected == true) and a
//  rectangle_component.  Changes are written back to the rectangle_component
//  immediately so any downstream render system sees the updated bounds.
//
//  Call from your ImGui draw loop with a canvas transform:
//
//    ecs::TransformHandle2DSystem handleSys;
//
//    // In your ImGui window draw:
//    handleSys.draw(registry, drawList, panX, panY, zoom);
//
//  The pan/zoom describe how canvas-space coordinates map to screen pixels:
//    screen = origin + canvas * zoom + pan
//  where origin is the top-left of your canvas area in screen space.
//
#include "ofxEnTTKit_all.h"
#include "TransformHandle2D.h"
#include "imgui.h"
#include <unordered_map>

namespace ecs {

class TransformHandle2DSystem : public ISystem {
public:
    const char* getName() const override { return "TransformHandle2DSystem"; }

    /// Draw handles for all selected rectangle_component entities.
    /// @param registry   The ECS registry.
    /// @param dl         Target ImDrawList (pass ImGui::GetWindowDrawList()).
    /// @param originX/Y  Screen position of canvas origin (top-left of the viewport area).
    /// @param zoom       Canvas-to-screen scale factor.
    void draw(entt::registry& registry, ImDrawList* dl,
              float originX, float originY, float zoom = 1.f);

    /// Same as above, accepting an ImVec2 for origin.
    void draw(entt::registry& registry, ImDrawList* dl,
              ImVec2 origin, float zoom = 1.f) {
        draw(registry, dl, origin.x, origin.y, zoom);
    }

    // ISystem overrides (unused — call the overloads above from your ImGui code)
    void update(entt::registry&, float) override {}
    void draw(entt::registry&)          override {}

private:
    // One persistent handle widget per entity so drag state is preserved.
    std::unordered_map<entt::entity, TransformHandle2D> m_handles;
};

inline void TransformHandle2DSystem::draw(
    entt::registry& registry, ImDrawList* dl,
    float ox, float oy, float zoom)
{
    if (!dl || zoom <= 0.f) return;

    auto toScreen = [ox, oy, zoom](float cx, float cy) -> ImVec2 {
        return { ox + cx * zoom, oy + cy * zoom };
    };
    auto toCanvas = [ox, oy, zoom](float sx, float sy) -> ImVec2 {
        return { (sx - ox) / zoom, (sy - oy) / zoom };
    };

    auto view = registry.view<selectable_component, rectangle_component>();
    for (auto entity : view) {
        auto& sel  = view.get<selectable_component>(entity);
        if (!sel.selected) continue;

        auto& rc = view.get<rectangle_component>(entity);
        Rect2D r { rc.x, rc.y, rc.width, rc.height };

        m_handles[entity].draw(dl, r, toScreen, toCanvas);

        // Write back if the handle moved or resized the rect.
        if (r.x != rc.x || r.y != rc.y || r.w != rc.width || r.h != rc.height) {
            rc.x      = r.x;  rc.y      = r.y;
            rc.width  = r.w;  rc.height = r.h;
        }
    }

    // Prune handles for entities that no longer exist.
    for (auto it = m_handles.begin(); it != m_handles.end(); ) {
        if (!registry.valid(it->first)) it = m_handles.erase(it);
        else ++it;
    }
}

} // namespace ecs
