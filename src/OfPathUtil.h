#pragma once

#include "ofMain.h"
#include "ImVectorEditor.h"
#include <functional>
#include <glm/vec2.hpp>

struct ImDrawList;
struct ImVec2;
using ImU32 = unsigned int;

namespace ofkitty {

float distSqPointSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b);
glm::vec2 cubicPoint(const glm::vec2& p0, const glm::vec2& p1,
                     const glm::vec2& p2, const glm::vec2& p3, float t);

float distSqToPath(const ofPath& path, float x, float y);
bool  pathIntersectsRect(const ofPath& path, float rx0, float ry0, float rx1, float ry1);

void loadAnchorsFromPath(const ofPath& src, ImVectorEditor::Path& out);
void ofPathFromEditorPath(const ImVectorEditor::Path& src, ofPath& out);

/// Tessellate @p path to ImDrawList segments via @p toScreen (path space → screen px).
void drawPathOnDrawList(const ofPath& path, ImDrawList* dl, ImU32 col, float thickness,
                        const std::function<ImVec2(float x, float y)>& toScreen);

} // namespace ofkitty
