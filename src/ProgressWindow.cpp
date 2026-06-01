#include "ProgressWindow.h"
#include "Runtime.h"
#include "ViewWindow.h"

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

void ProgressWindow::setUseStatusBar(bool useStatusBar)
{
    m_useStatusBar = useStatusBar;
    if (m_useStatusBar) {
        runtime().setWindowVisible("##ofkprogress", false);
    }
}

bool ProgressWindow::useStatusBar() const
{
    return m_useStatusBar;
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

    if (!m_useStatusBar) {
        runtime().setWindowVisible("##ofkprogress", true);
    }
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
    if (!m_useStatusBar) {
        runtime().setWindowVisible("##ofkprogress", true);
    }
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

    runtime().registerWindow(makeViewWindow(
        "##ofkprogress",
        [this](bool& visible) { draw(visible); },
        {.menuGroup = "", .visible = false, .editModeOnly = false}));
}

void ProgressWindow::attachStatusBarItem()
{
    if (m_statusBarItemAttached) return;
    m_statusBarItemAttached = true;

    runtime().registerStatusItem({
        "ofxkit.status.progress",
        "ofxkit.progress",
        /*visible=*/ true,
        [this]() { drawInStatusBar(); }
    });
}

// ============================================================================
// Draw — status bar slot
// ============================================================================

void ProgressWindow::drawInStatusBar()
{
    if (!m_useStatusBar) {
        return;
    }

    std::string titleSnap, labelSnap;
    float       progressSnap  = 0.f;
    bool        finishedSnap  = false;
    bool        activeSnap    = false;
    double      finishTimeSnap = -1.0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        titleSnap      = m_title;
        labelSnap      = m_label;
        progressSnap   = m_progress;
        finishedSnap   = m_finished;
        finishTimeSnap = m_finishTime;
        activeSnap     = m_active;
    }

    if (finishedSnap && finishTimeSnap >= 0.0) {
        double elapsed = static_cast<double>(ofGetElapsedTimef()) - finishTimeSnap;
        if (elapsed >= static_cast<double>(m_autoHideDelay)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_finished   = false;
            m_finishTime = -1.0;
            return;
        }
    }

    if (!activeSnap && !finishedSnap) {
        return;
    }

    ImGui::TextUnformatted(titleSnap.c_str());
    ImGui::SameLine(0, 10.f);

    float barW = std::max(160.f, ImGui::GetContentRegionAvail().x * 0.42f);
    ImGui::ProgressBar(progressSnap, ImVec2(barW, 0.f));

    ImGui::SameLine(0, 10.f);
    ImGui::TextUnformatted(labelSnap.c_str());
}

// ============================================================================
// Draw — floating window
// ============================================================================

void ProgressWindow::draw(bool& visible)
{
    if (m_useStatusBar) {
        return;
    }

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

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(vp->Pos.x + vp->Size.x * 0.5f,
               vp->Pos.y + vp->Size.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.f, 0.f), ImGuiCond_Appearing);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoNav
                           | ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("Progress###ofkprogress_float", &visible, flags)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(titleSnap.c_str());
    ImGui::ProgressBar(progressSnap, ImVec2(-1.f, 0.f));
    ImGui::TextUnformatted(labelSnap.c_str());
    ImGui::End();
}

} // namespace ofkitty
