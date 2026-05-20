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

void Runtime::setPropertiesSupplement(std::function<void()> draw)
{
    m_propertiesSupplement = std::move(draw);
}

void Runtime::clearPropertiesSupplement()
{
    m_propertiesSupplement = nullptr;
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

void Runtime::codeEditorSetSidebarEntries(
    std::vector<CodeEditorPanel::SidebarEntry> entries)
{
    if (m_codeEditor)
        m_codeEditor->setSidebarEntries(std::move(entries));
}

void Runtime::codeEditorSetHighlightLine(int line)
{
    if (m_codeEditor) m_codeEditor->setHighlightLine(line);
}

void Runtime::codeEditorSetCursorLine(int line)
{
    if (m_codeEditor) m_codeEditor->setCursorLine(line);
}

int Runtime::codeEditorGetCursorLine() const
{
    return m_codeEditor ? m_codeEditor->getCursorLine() : 0;
}

int Runtime::codeEditorGetLineCount() const
{
    return m_codeEditor ? m_codeEditor->getLineCount() : 1;
}

void Runtime::codeEditorSetSyncPlaybackFromCursor(bool enabled)
{
    if (m_codeEditor) m_codeEditor->setSyncPlaybackFromCursor(enabled);
}

void Runtime::codeEditorSetOnCursorLineChanged(std::function<void(int line)> cb)
{
    if (m_codeEditor) m_codeEditor->setOnCursorLineChanged(std::move(cb));
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

Runtime::ViewportInstance* Runtime::addViewportWindow2D(
    std::string title, glm::vec2 contentSize, std::string contentUnit, bool editModeOnly)
{
    auto* vp = addViewportWindow(std::move(title));
    if (vp) {
        vp->mode         = ViewportInstance::Mode::Ortho2D;
        vp->contentSize  = contentSize;
        vp->contentUnit  = std::move(contentUnit);
        vp->editModeOnly = editModeOnly;
        // Patch the already-registered window's editModeOnly flag to match.
        if (auto* win = findWindow(vp->title))
            win->editModeOnly = editModeOnly;
    }
    return vp;
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
    if (vp.mode == ViewportInstance::Mode::Ortho2D) {
        drawViewportWindow2D(vp, visible);
        return;
    }

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

void Runtime::drawViewportWindow2D(ViewportInstance& vp, bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(600, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(vp.title.c_str(), &visible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        // App-specific menus (zoom controls, overlay toggles, etc.)
        if (vp.menuBarDraw)
            vp.menuBarDraw();
        // Built-in View menu — always appended last
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Rulers", nullptr, &vp.showRulers);
            if (ImGui::MenuItem("Fit to Window")) { vp.pan2D = {}; vp.zoom2D = 1.f; }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Optional header: progress bars, status messages, etc.
    // If headerDraw returns true the canvas is suppressed for this frame.
    if (vp.headerDraw && vp.headerDraw()) {
        ImGui::End();
        return;
    }

    if (!vp.renderer2D) {
        ImGui::TextDisabled("No renderer.");
        ImGui::TextWrapped("Set vp->renderer2D = [this]{ /* OF draw */ }; in setup().");
        ImGui::End();
        return;
    }

    // ---- Layout: optional ruler strips along top and left edges ------------
    const auto& prefs = appPrefs();
    const float RS_px = vp.showRulers
        ? std::round(20.f * m_uiScale * prefs.rulerScale)
        : 0.f;

    const ImVec2 windowOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 fullAvail    = ImGui::GetContentRegionAvail();
    const ImVec2 canvasOrigin(windowOrigin.x + RS_px, windowOrigin.y + RS_px);
    const float  canvasW = std::max(4.f, fullAvail.x - RS_px);
    const float  canvasH = std::max(4.f, fullAvail.y - RS_px);

    // Invisible hit region for hover / drag detection.
    ImGui::SetCursorScreenPos(canvasOrigin);
    ImGui::InvisibleButton("canvas2d", ImVec2(canvasW, canvasH));
    const bool canvasHovered = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(windowOrigin);

    const ImGuiIO& io = ImGui::GetIO();

    // ---- Scroll-to-zoom (zoom around cursor) --------------------------------
    const float fitZoom = (vp.contentSize.x > 0.f && vp.contentSize.y > 0.f)
        ? std::min(canvasW / vp.contentSize.x, canvasH / vp.contentSize.y)
        : 1.f;

    if (canvasHovered && io.MouseWheel != 0.f) {
        const float factor  = (io.MouseWheel > 0.f) ? 1.15f : 1.f / 1.15f;
        const float zoomOld = fitZoom * vp.zoom2D;
        const float pxOld   = canvasW * 0.5f + vp.pan2D.x - vp.contentSize.x * zoomOld * 0.5f;
        const float pyOld   = canvasH * 0.5f + vp.pan2D.y - vp.contentSize.y * zoomOld * 0.5f;
        const float mu      = (io.MousePos.x - canvasOrigin.x - pxOld) / zoomOld;
        const float mv      = (io.MousePos.y - canvasOrigin.y - pyOld) / zoomOld;
        vp.zoom2D          *= factor;
        const float zoomNew = fitZoom * vp.zoom2D;
        vp.pan2D.x = io.MousePos.x - canvasOrigin.x - mu * zoomNew
                     - canvasW * 0.5f + vp.contentSize.x * zoomNew * 0.5f;
        vp.pan2D.y = io.MousePos.y - canvasOrigin.y - mv * zoomNew
                     - canvasH * 0.5f + vp.contentSize.y * zoomNew * 0.5f;
    }

    // ---- Middle-mouse / Alt+LMB pan ----------------------------------------
    if (canvasHovered && (io.MouseDown[2] || (io.MouseDown[0] && io.KeyAlt))) {
        vp.pan2D.x += io.MouseDelta.x;
        vp.pan2D.y += io.MouseDelta.y;
    }

    // ---- Double-click to fit ------------------------------------------------
    if (canvasHovered && io.MouseDoubleClicked[0] && !io.KeyAlt) {
        vp.zoom2D = 1.f;
        vp.pan2D  = {};
    }

    // ---- Canonical coordinate state (updated every frame) ------------------
    // Coordinate system (Y-DOWN, matching OF screen and ImGui):
    //   screen  = (ox + content.x * zoom,  oy + content.y * zoom)
    //   content = ((screen.x - ox) / zoom, (screen.y - oy) / zoom)
    const float zoom = fitZoom * vp.zoom2D;
    const float ox   = canvasOrigin.x + canvasW * 0.5f + vp.pan2D.x
                       - vp.contentSize.x * zoom * 0.5f;
    const float oy   = canvasOrigin.y + canvasH * 0.5f + vp.pan2D.y
                       - vp.contentSize.y * zoom * 0.5f;
    vp._ox = ox;           vp._oy = oy;           vp._zoom = zoom;
    vp._canvasOx = canvasOrigin.x;  vp._canvasOy = canvasOrigin.y;
    vp._canvasW  = canvasW;         vp._canvasH  = canvasH;
    vp._canvasHovered = canvasHovered;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasOrigin,
                     ImVec2(canvasOrigin.x + canvasW, canvasOrigin.y + canvasH), true);

    // ---- Allocate / resize FBO ---------------------------------------------
    const bool needsAlloc = !vp.fbo.isAllocated()
        || std::fabs(vp.lastPanelSize.x - canvasW) > 0.5f
        || std::fabs(vp.lastPanelSize.y - canvasH) > 0.5f;
    if (needsAlloc) {
        ofDisableArbTex();
        ofFboSettings s;
        s.width          = static_cast<int>(canvasW);
        s.height         = static_cast<int>(canvasH);
        s.internalformat = GL_RGBA;
        s.useDepth       = false;
        vp.fbo.allocate(s);
        ofEnableArbTex();
        vp.lastPanelSize = { canvasW, canvasH };
    }

    // ---- Render content into FBO -------------------------------------------
    // ofSetupScreenOrtho is Y-UP; we immediately flip to Y-DOWN so that the
    // renderer2D callback works in the same coordinate space as screen/ImGui.
    const float fboW  = static_cast<float>(vp.fbo.getWidth());
    const float fboH  = static_cast<float>(vp.fbo.getHeight());
    const float fboOx = ox - canvasOrigin.x;   // canvas-local content TL X
    const float fboOy = oy - canvasOrigin.y;   // canvas-local content TL Y

    vp.fbo.begin();
    ofClear(18, 18, 24, 255);
    ofSetupScreenOrtho(fboW, fboH, -1.f, 1.f);
    ofTranslate(0.f, fboH);
    ofScale(1.f, -1.f);          // flip to Y-DOWN
    ofTranslate(fboOx, fboOy);   // apply pan
    ofScale(zoom, zoom);          // apply zoom
    vp.renderer2D();
    vp.fbo.end();

    // ---- Blit FBO (UV-flip compensates for OpenGL's bottom-up storage) -----
    const auto& tex = vp.fbo.getTexture();
    if (tex.getTextureData().textureTarget == GL_TEXTURE_2D) {
        ImGui::SetCursorScreenPos(canvasOrigin);
        ImGui::Image(GetImTextureID(tex), ImVec2(canvasW, canvasH),
                     ImVec2(0, 1), ImVec2(1, 0));
    }

    // ---- App overlays (toScreen() / toContent() are now valid) -------------
    if (vp.overlayDraw)
        vp.overlayDraw(vp);

    dl->PopClipRect();

    // ---- Rulers ------------------------------------------------------------
    if (vp.showRulers && zoom > 0.f) {
        const ImVec2 contentTLcanvas { ox - canvasOrigin.x, oy - canvasOrigin.y };
        drawRulersInRegion(dl, windowOrigin, fullAvail, io.MousePos,
                           zoom, vp.contentUnit.c_str(),
                           m_uiScale, prefs.rulerScale,
                           vp.guides, contentTLcanvas);
    }

    ImGui::End();
}

} // namespace ofkitty
