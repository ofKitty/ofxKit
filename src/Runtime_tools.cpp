#include "Runtime.h"
#include "panels/CodeEditorPanel.h"
#include "panels/PathEditorPanel.h"
#include "RulerUtil.h"

#include <ofxEnTTKit/src/ofxEnTTKit.h>
#include <ofxImGui/src/ofxImGui.h>

#include <ofxImGuiFileDialog/src/ofxImGuiFileDialog.h>

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

namespace ofkitty {

void Runtime::openFileDialog(const std::string& key,
                             const std::string& title,
                             const std::string& filters,
                             std::function<void(const std::string& path)> onConfirm)
{
    m_fileDialogCbs[key] = std::move(onConfirm);
    IGFD::FileDialogConfig cfg;
    cfg.path  = ".";
    cfg.flags = ImGuiFileDialogFlags_Modal;
    ImGuiFileDialog::Instance()->OpenDialog(key, title, filters.c_str(), cfg);
}

void Runtime::saveFileDialog(const std::string& key,
                             const std::string& title,
                             const std::string& filters,
                             const std::string& defaultFileName,
                             std::function<void(const std::string& path)> onConfirm)
{
    m_fileDialogCbs[key] = std::move(onConfirm);
    IGFD::FileDialogConfig cfg;
    cfg.path     = ".";
    cfg.fileName = defaultFileName;
    cfg.flags =
        ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog(key, title, filters.c_str(), cfg);
}

void Runtime::processFileDialogs()
{
    const ImVec2 minSz(480.f * m_uiScale, 320.f * m_uiScale);
    const ImVec2 maxSz(FLT_MAX, FLT_MAX);

    for (auto it = m_fileDialogCbs.begin(); it != m_fileDialogCbs.end();) {
        const std::string& key = it->first;
        if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoCollapse, minSz, maxSz)) {
            if (ImGuiFileDialog::Instance()->IsOk() && it->second)
                it->second(ImGuiFileDialog::Instance()->GetFilePathName());
            ImGuiFileDialog::Instance()->Close();
            it = m_fileDialogCbs.erase(it);
        } else {
            ++it;
        }
    }
}

void Runtime::setSceneCamera(ofCamera* cam)
{
    m_sceneCamera  = cam;
    m_sceneEasyCam = cam ? dynamic_cast<ofEasyCam*>(cam) : nullptr;
    m_sceneViewCaptured = false;
}

void Runtime::clearSceneCamera()
{
    setSceneCamera(nullptr);
}

bool Runtime::isSceneHovered() const
{
    return m_sceneViewport.isHovered() && !isGizmoActive();
}

void Runtime::drawGizmoOverlay()
{
    if (!m_sceneCamera)
        return;
    if (m_selected == entt::null || !registry().valid(m_selected))
        return;
    auto* nc = registry().try_get<ecs::node_component>(m_selected);
    if (!nc)
        return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    constexpr ImGuiWindowFlags kOverlayFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##gizmo_scene_overlay", nullptr, kOverlayFlags);
    ImGui::PopStyleVar(2);

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetID(1);
    const ofRectangle sceneRect(vp->Pos.x, vp->Pos.y, vp->Size.x, vp->Size.y);

    auto toIGOp = [](GizmoOperation op) -> ImGuizmo::OPERATION {
        switch (op) {
            case GizmoOperation::Rotate:
                return ImGuizmo::ROTATE;
            case GizmoOperation::Scale:
                return ImGuizmo::SCALE;
            case GizmoOperation::Universal:
                return ImGuizmo::UNIVERSAL;
            default:
                return ImGuizmo::TRANSLATE;
        }
    };
    auto toIGMode = [](GizmoMode m) -> ImGuizmo::MODE {
        return m == GizmoMode::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    };

    ImGuizmo::Manipulate(*m_sceneCamera,
                         nc->node,
                         toIGOp(m_gizmoOp),
                         toIGMode(m_gizmoMode),
                         &sceneRect);

    ImGui::End();
}

void Runtime::captureSceneView()
{
    m_capturedView      = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
    m_capturedProj      = ofGetCurrentMatrix(OF_MATRIX_PROJECTION);
    m_sceneViewCaptured = true;
}

bool Runtime::isGizmoActive() const
{
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void Runtime::drawGizmoInViewport(ViewportInstance& vp, const ofRectangle& imgScreenRect)
{
    if (m_selected == entt::null || !registry().valid(m_selected))
        return;
    auto* nc = registry().try_get<ecs::node_component>(m_selected);
    if (!nc)
        return;

    auto toIGOp = [](GizmoOperation op) -> ImGuizmo::OPERATION {
        switch (op) {
            case GizmoOperation::Rotate:
                return ImGuizmo::ROTATE;
            case GizmoOperation::Scale:
                return ImGuizmo::SCALE;
            case GizmoOperation::Universal:
                return ImGuizmo::UNIVERSAL;
            default:
                return ImGuizmo::TRANSLATE;
        }
    };
    auto toIGMode = [](GizmoMode m) -> ImGuizmo::MODE {
        return m == GizmoMode::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    };

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetID(static_cast<int>(
        1000 + (reinterpret_cast<uintptr_t>(&vp) & 0x0fffffff)));

    ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
    ImGuizmo::Manipulate(vp.cam,
                         nc->node,
                         toIGOp(m_gizmoOp),
                         toIGMode(m_gizmoMode),
                         &imgScreenRect);
    ImGuizmo::SetAlternativeWindow(nullptr);
}

void Runtime::codeEditorSetText(const std::string& text,
                                TextEditor::LanguageDefinitionId lang)
{
    if (m_codeEditor)
        m_codeEditor->setText(text, lang);
}

std::string Runtime::codeEditorGetText() const
{
    return m_codeEditor ? m_codeEditor->getText() : "";
}

void Runtime::codeEditorSetLanguage(TextEditor::LanguageDefinitionId lang)
{
    if (m_codeEditor)
        m_codeEditor->setLanguage(lang);
}

void Runtime::drawCodeEditorWindow(bool& visible)
{
    if (m_codeEditor)
        m_codeEditor->draw(visible);
}

void Runtime::drawPathEditorWindow(bool& visible)
{
    if (m_pathEditor)
        m_pathEditor->draw(visible, registry(), m_selected);
}

void Runtime::setViewportRenderer(ViewportRenderer fn)
{
    m_viewportRenderer = std::move(fn);
}

void Runtime::clearViewportRenderer()
{
    m_viewportRenderer = nullptr;
}

Runtime::ViewportInstance* Runtime::addViewportWindow(std::string title)
{
    if (title.empty()) {
        title = "Scene View";
        int n = 2;
        while (std::any_of(m_viewportInstances.begin(),
                           m_viewportInstances.end(),
                           [&](const std::unique_ptr<ViewportInstance>& p) {
                               return p->title == title;
                           })) {
            title = "Scene View " + std::to_string(n++);
        }
    }

    for (auto& inst : m_viewportInstances) {
        if (inst->title == title) {
            ofLogWarning("ofxKit") << "addViewportWindow: title '" << title
                                   << "' already exists; returning existing instance.";
            return inst.get();
        }
    }

    auto& inst = m_viewportInstances.emplace_back(std::make_unique<ViewportInstance>());
    inst->title  = title;
    ViewportInstance* raw = inst.get();

    registerWindow({
        title,
        "View",
        true,
        true,
        [this, raw](bool& vis) { drawViewportWindow(*raw, vis); },
    });

    return raw;
}

void Runtime::removeViewportWindow(const std::string& title)
{
    m_windows.erase(
        std::remove_if(m_windows.begin(),
                       m_windows.end(),
                       [&](const RuntimeWindow& w) { return w.name == title; }),
        m_windows.end());

    m_viewportInstances.erase(
        std::remove_if(m_viewportInstances.begin(),
                       m_viewportInstances.end(),
                       [&](const std::unique_ptr<ViewportInstance>& p) {
                           return p->title == title;
                       }),
        m_viewportInstances.end());
}

void Runtime::updateViewportCamera(ViewportInstance& vp)
{
    float az = glm::radians(vp.azimuth);
    float el = glm::radians(vp.elevation);
    glm::vec3 offset(vp.distance * std::cos(el) * std::sin(az),
                     vp.distance * std::sin(el),
                     vp.distance * std::cos(el) * std::cos(az));
    vp.cam.setPosition(vp.target + offset);
    vp.cam.lookAt(vp.target);
}

void Runtime::drawViewportWindow(ViewportInstance& vp, bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(480, 320), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(vp.title.c_str(), &visible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    struct Preset {
        const char* label;
        float       az;
        float       el;
    };
    static const Preset presets[] = {
        {"Perspective", 30.f, 20.f},
        {"Front", 0.f, 0.f},
        {"Back", 180.f, 0.f},
        {"Top", 0.f, 89.9f},
        {"Bottom", 0.f, -89.9f},
        {"Right", 90.f, 0.f},
        {"Left", 270.f, 0.f},
    };
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::SeparatorText("Camera Preset");
            for (auto& p : presets) {
                if (ImGui::MenuItem(p.label)) {
                    vp.azimuth   = p.az;
                    vp.elevation = p.el;
                }
            }
            ImGui::Separator();
            ImGui::SetNextItemWidth(110.f);
            ImGui::DragFloat("Distance", &vp.distance, 5.f, 10.f, 5000.f, "%.0f");
            ImGui::Separator();
            ImGui::MenuItem("Show Gizmo",  nullptr, &vp.showGizmo);
            ImGui::MenuItem("Show Rulers", nullptr, &vp.showRulers);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (!m_viewportRenderer) {
        ImGui::TextDisabled("No renderer registered.");
        ImGui::TextWrapped(
            "Call runtime().setViewportRenderer([this]{ /* draw scene */ }) in "
            "your ofApp::setup().");
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 4.f)
        avail.x = 4.f;
    if (avail.y < 4.f)
        avail.y = 4.f;

    bool needsAlloc =
        !vp.fbo.isAllocated()
        || std::fabs(vp.lastPanelSize.x - avail.x) > 0.5f
        || std::fabs(vp.lastPanelSize.y - avail.y) > 0.5f;

    if (needsAlloc) {
        ofDisableArbTex();
        ofFboSettings s;
        s.width                 = static_cast<int>(avail.x);
        s.height                = static_cast<int>(avail.y);
        s.internalformat        = GL_RGBA;
        s.useDepth              = true;
        s.depthStencilAsTexture = false;
        vp.fbo.allocate(s);
        ofEnableArbTex();
        vp.lastPanelSize = {avail.x, avail.y};
        vp.cam.setNearClip(1.f);
        vp.cam.setFarClip(10000.f);
    }

    vp.fbo.begin();
    ofClear(18, 18, 24, 255);
    ofEnableDepthTest();
    updateViewportCamera(vp);
    vp.cam.begin(ofRectangle(
        0,
        0,
        static_cast<float>(vp.fbo.getWidth()),
        static_cast<float>(vp.fbo.getHeight())));
    m_viewportRenderer();
    vp.cam.end();
    ofDisableDepthTest();
    vp.fbo.end();

    const auto& tex          = vp.fbo.getTexture();
    ImVec2      imgPosScreen = ImGui::GetCursorScreenPos();
    if (tex.getTextureData().textureTarget == GL_TEXTURE_2D) {
        // OpenGL FBOs are stored bottom-up; flip the V axis so the image
        // appears right-side up in ImGui's top-down coordinate system.
        ImTextureID tid = GetImTextureID(tex);
        ImGui::Image(tid, ImVec2(avail.x, avail.y), ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::TextDisabled("Viewport texture is not GL_TEXTURE_2D.");
        ImGui::TextDisabled("textureTarget: 0x%X", tex.getTextureData().textureTarget);
    }

    if (m_editMode && vp.showGizmo) {
        const ofRectangle imgScreenRect(imgPosScreen.x, imgPosScreen.y, avail.x,
                                        avail.y);
        drawGizmoInViewport(vp, imgScreenRect);
    }

    if (vp.showRulers) {
        drawRulersInRegion(
            ImGui::GetWindowDrawList(),
            imgPosScreen,
            avail,
            ImGui::GetIO().MousePos,
            1.0f, "px",
            m_uiScale, m_prefs.rulerScale, nullptr, ImVec2(0.f, 0.f));
    }

    const bool gizmoWantsInput =
        ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    const ImVec2 p1 = {imgPosScreen.x + avail.x, imgPosScreen.y + avail.y};
    const bool vpHovered = ImGui::IsMouseHoveringRect(imgPosScreen, p1);

    if (vpHovered && !gizmoWantsInput) {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            vp.azimuth -= io.MouseDelta.x * 0.4f;
            vp.elevation += io.MouseDelta.y * 0.4f;
            vp.elevation = glm::clamp(vp.elevation, -89.f, 89.f);
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            float      az      = glm::radians(vp.azimuth);
            glm::vec3  right(std::cos(az), 0.f, -std::sin(az));
            glm::vec3  up(0.f, 1.f, 0.f);
            float panSpeed = vp.distance * 0.001f;
            vp.target -= right * (io.MouseDelta.x * panSpeed);
            vp.target += up * (io.MouseDelta.y * panSpeed);
        }

        if (io.MouseWheel != 0.f) {
            vp.distance -= io.MouseWheel * vp.distance * 0.1f;
            vp.distance = glm::clamp(vp.distance, 10.f, 5000.f);
        }

        ImGui::SetTooltip("Drag: orbit   Middle-drag: pan   Scroll: zoom");
    }

    ImGui::End();
}

} // namespace ofkitty
