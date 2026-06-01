#pragma once

#include "ofMain.h"
#include "components/base_components.h"
#include <entt.hpp>

namespace ofkitty {

/// World-space ray through a screen point (uses ofCamera MVP — no OF orientation helper required).
inline void cameraScreenToWorldRay(const ofCamera& cam,
                                   glm::vec2       screenPx,
                                   const ofRectangle& viewport,
                                   glm::vec3&      outOrigin,
                                   glm::vec3&      outDir)
{
    glm::mat4 mvp = cam.getModelViewProjectionMatrix(viewport);
    if (cam.isVFlipped())
        mvp = glm::scale(glm::mat4(1.f), glm::vec3(1.f, -1.f, 1.f)) * mvp;

    const float x = 2.f * (screenPx.x - viewport.x) / viewport.width - 1.f;
    const float y = 1.f - 2.f * (screenPx.y - viewport.y) / viewport.height;

    auto unproject = [&](float ndcZ) {
        const glm::vec4 h = glm::inverse(mvp) * glm::vec4(x, y, ndcZ, 1.f);
        return glm::vec3(h) / h.w;
    };

    outOrigin = unproject(0.f);
    const glm::vec3 w1 = unproject(1.f);
    outDir    = w1 - outOrigin;
    const float len = glm::length(outDir);
    if (len > 1e-6f)
        outDir /= len;
}

inline entt::entity pickSelectableEntity(entt::registry& registry,
                                         const ofCamera& cam,
                                         glm::vec2 screenPx,
                                         const ofRectangle& viewport)
{
    if (viewport.width <= 0.f || viewport.height <= 0.f)
        return entt::null;

    glm::vec3 w0, dir;
    cameraScreenToWorldRay(cam, screenPx, viewport, w0, dir);
    if (glm::length(dir) < 1e-6f)
        return entt::null;

    return ecs::pickSelectableEntityRay(registry, w0, dir);
}

} // namespace ofkitty
