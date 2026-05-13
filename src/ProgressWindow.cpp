#include "ProgressWindow.h"
#include "Runtime.h"

// ofMain.h is pulled in transitively via Runtime.h.
// imgui.h is pulled in transitively via Runtime.h → ofxImGui.h.
#include <algorithm>        // std::max / std::min

namespace ofkitty {

// ============================================================================
// Singleton
// ============================================================================

ProgressWindow& ProgressWindow::instance()
{
    static ProgressWindow s_instance;
    return s_instance;
}

// ============================================================================
// Configuration
// ============================================================================

void ProgressWindow::setBottomAnchored(bool anchored)
{
    m_bottomAnchored = anchored;
}

bool ProgressWindow::isBottomAnchored() const
{
    return m_bottomAnchored;
}

void ProgressWindow::setAutoHideDelay(float seconds)
{
    m_autoHideDelay = seconds;
}

float ProgressWindow::autoHideDelay() const
{
    return m_autoHideDelay;
}

// ============================================================================
// Lifecycle
// ============================================================================

void ProgressWindow::begin(std::string title, int totalSteps)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_title        = std::move(title);
        m_label        = "";
        m_progress     = 0.f;
        m_totalSteps   = std::max(totalSteps, 1);
        m_currentStep  = 0;
        m_active       = true;
        m_finished     = false;
        m_finishTime   = -1.0;
    }
    registerWithRuntime();

    // Make the RuntimeWindow visible (it starts hidden).
    runtime().setWindowVisible("##ofkprogress", true);
}

void ProgressWindow::begin(std::string title)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_title        = std::move(title);
        m_label        = "";
        m_progress     = 0.f;
        m_totalSteps   = 0;     // 0 = absolute mode
        m_currentStep  = 0;
        m_active       = true;
        m_finished     = false;
        m_finishTime   = -1.0;
    }
    registerWithRuntime();
    runtime().setWindowVisible("##ofkprogress", true);
}

void ProgressWindow::tick(std::string label)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active) return;

    m_label = std::move(label);

    if (m_totalSteps > 0) {
        m_currentStep = std::min(m_currentStep + 1, m_totalSteps);
        m_progress    = static_cast<float>(m_currentStep) / static_cast<float>(m_totalSteps);
    }
}

void ProgressWindow::tick(std::string label, float prog)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active) return;

    m_label = std::move(label);
    // Negative values are forwarded as-is to ImGui::ProgressBar, which renders
    // them as an animated marquee (indeterminate). Positive values are clamped.
    m_progress = (prog < 0.f) ? -1.f : std::min(prog, 1.f);
}

void ProgressWindow::tickIndeterminate(std::string label)
{
    tick(std::move(label), -1.f);
}

void ProgressWindow::finish(std::string doneLabel)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress   = 1.f;
    m_label      = std::move(doneLabel);
    m_active     = false;
    m_finished   = true;
    m_finishTime = static_cast<double>(ofGetElapsedTimef());
}

void ProgressWindow::hide()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active   = false;
        m_finished = false;
    }
    runtime().setWindowVisible("##ofkprogress", false);
}

// ============================================================================
// State queries
// ============================================================================

bool ProgressWindow::isActive() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active;
}

float ProgressWindow::progress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

std::string ProgressWindow::label() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_label;
}

std::string ProgressWindow::title() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_title;
}

// ============================================================================
// Runtime integration
// ============================================================================

void ProgressWindow::registerWithRuntime()
{
    if (m_registered) return;
    m_registered = true;

    // menuGroup = "" keeps it out of the View menu.
    // Starts hidden; begin() makes it visible.
    runtime().registerWindow({
        "##ofkprogress",
        /*menuGroup=*/ "",
        /*visible=*/   false,
        /*editModeOnly=*/ false,
        [this](bool& visible) { draw(visible); }
    });
}

// ============================================================================
// Draw — called on the GL / main thread inside the Runtime overlay
// ============================================================================

void ProgressWindow::draw(bool& visible)
{
    // Snapshot mutable state under the lock so the render path is lock-free.
    std::string titleSnap, labelSnap;
    float       progressSnap  = 0.f;
    bool        finishedSnap  = false;
    double      finishTimeSnap = -1.0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        titleSnap      = m_title;
        labelSnap      = m_label;
        progressSnap   = m_progress;
        finishedSnap   = m_finished;
        finishTimeSnap = m_finishTime;
    }

    // Auto-hide after the delay once finished.
    if (finishedSnap && finishTimeSnap >= 0.0) {
        double elapsed = static_cast<double>(ofGetElapsedTimef()) - finishTimeSnap;
        if (elapsed >= static_cast<double>(m_autoHideDelay)) {
            visible = false;

            std::lock_guard<std::mutex> lock(m_mutex);
            m_finished   = false;
            m_finishTime = -1.0;
            return;
        }
    }

    // ------------------------------------------------------------------
    // Window layout
    // ------------------------------------------------------------------
    constexpr float kPanelHeight = 52.f; // pts (scaled by ImGui font scale)

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoNav
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoBringToFrontOnFocus
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoFocusOnAppearing;

    if (m_bottomAnchored) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float scale = ImGui::GetIO().FontGlobalScale;
        float panelH = kPanelHeight * scale;

        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - panelH),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(vp->Size.x, panelH),
            ImGuiCond_Always);

        flags |= ImGuiWindowFlags_NoMove
              |  ImGuiWindowFlags_NoResize
              |  ImGuiWindowFlags_NoTitleBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    } else {
        // Floating: center on first appearance, let the user move it.
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + vp->Size.x * 0.5f,
                   vp->Pos.y + vp->Size.y * 0.5f),
            ImGuiCond_Appearing,
            ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(360.f, 0.f), ImGuiCond_Appearing);
    }

    bool open = ImGui::Begin("##ofkprogress", nullptr, flags);

    if (m_bottomAnchored) {
        ImGui::PopStyleVar(2);
    }

    if (open) {
        float availW = ImGui::GetContentRegionAvail().x;

        if (m_bottomAnchored) {
            // Single-line layout: "Title   [====    ] label"
            ImGui::Text("%s", titleSnap.c_str());
            ImGui::SameLine();
            ImGui::ProgressBar(progressSnap, ImVec2(availW * 0.55f, 0.f));
            ImGui::SameLine();
            ImGui::TextUnformatted(labelSnap.c_str());
        } else {
            // Stacked layout: title on top, bar full width, label below.
            ImGui::TextUnformatted(titleSnap.c_str());
            ImGui::ProgressBar(progressSnap, ImVec2(-1.f, 0.f));
            ImGui::TextUnformatted(labelSnap.c_str());
        }
    }
    ImGui::End();
}

} // namespace ofkitty
