#include "CodeEditorPanel.h"

#include "ofMain.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>

namespace ofkitty {

void CodeEditorPanel::setup()
{
    m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Glsl);
    m_editor.SetPalette(TextEditor::PaletteId::Dark);
    m_editor.SetShowLineNumbersEnabled(true);
}

void CodeEditorPanel::setDialogCallbacks(
    std::function<void(const std::string& key,
                       const std::string& title,
                       const std::string& filters,
                       ConfirmPath onConfirm)> openFile,
    std::function<void(const std::string& key,
                       const std::string& title,
                       const std::string& filters,
                       const std::string& defaultFileName,
                       ConfirmPath onConfirm)> saveFile)
{
    m_openFile = std::move(openFile);
    m_saveFile = std::move(saveFile);
}

void CodeEditorPanel::setText(const std::string& text)
{
    m_editor.SetText(text);
}

std::string CodeEditorPanel::getText() const
{
    return m_editor.GetText();
}

void CodeEditorPanel::setLanguage(TextEditor::LanguageDefinitionId lang)
{
    m_editor.SetLanguageDefinition(lang);
}

void CodeEditorPanel::draw(bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(820, 580), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_MenuBar;
    if (!ImGui::Begin("Code Editor###ofxkit.window.code_editor", &visible, winFlags)) {
        ImGui::End();
        return;
    }

    auto detectLanguage = [this](const std::string& path) {
        const std::string ext = ofFilePath::getFileExt(path);
        if (ext == "glsl" || ext == "vert" || ext == "frag")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Glsl);
        else if (ext == "hlsl")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Hlsl);
        else if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cpp);
        else if (ext == "cs")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cs);
        else if (ext == "lua")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Lua);
        else if (ext == "py")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Python);
        else if (ext == "json")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Json);
        else if (ext == "sql")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Sql);
        else
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
    };

    auto doOpen = [&] {
        if (!m_openFile)
            return;
        m_openFile("code_open", "Open File",
                   "Source{.cpp,.h,.hpp,.c,.cs,.py,.lua,.js,.ts},"
                   "Shaders{.glsl,.vert,.frag,.hlsl},"
                   "Data{.json,.xml,.yaml,.txt,.md},"
                   "All Files{.*}",
                   [this, detectLanguage](const std::string& path) {
                       std::ifstream ifs(path);
                       if (ifs) {
                           m_filePath = path;
                           m_editor.SetText(std::string(
                               std::istreambuf_iterator<char>(ifs),
                               std::istreambuf_iterator<char>()));
                           detectLanguage(path);
                       }
                   });
    };

    auto doSave = [&] {
        if (!m_saveFile) {
            if (!m_filePath.empty()) {
                std::ofstream ofs(m_filePath);
                if (ofs)
                    ofs << m_editor.GetText();
            }
            return;
        }
        if (m_filePath.empty()) {
            m_saveFile("code_save", "Save As",
                       "Source{.cpp,.h,.hpp,.c,.glsl,.vert,.frag,.hlsl,.py,.lua},"
                       "Text & Data{.txt,.md,.json,.xml,.yaml},"
                       "All Files{.*}",
                       "untitled.glsl",
                       [this](const std::string& path) {
                           m_filePath = path;
                           std::ofstream ofs(path);
                           if (ofs)
                               ofs << m_editor.GetText();
                       });
        } else {
            std::ofstream ofs(m_filePath);
            if (ofs)
                ofs << m_editor.GetText();
        }
    };

    auto doSaveAs = [&] {
        if (!m_saveFile)
            return;
        m_saveFile("code_save_as", "Save As",
                   "Source{.cpp,.h,.hpp,.c,.glsl,.vert,.frag,.hlsl,.py,.lua},"
                   "Text & Data{.txt,.md,.json,.xml,.yaml},"
                   "All Files{.*}",
                   m_filePath.empty() ? "untitled.glsl"
                                      : ofFilePath::getFileName(m_filePath),
                   [this](const std::string& path) {
                       m_filePath = path;
                       std::ofstream ofs(path);
                       if (ofs)
                           ofs << m_editor.GetText();
                   });
    };

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
            m_editor.SetText("");
            m_filePath = "";
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
            doOpen();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
            if (ImGui::GetIO().KeyShift)
                doSaveAs();
            else
                doSave();
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) {
            m_findVisible     = true;
            m_replaceVisible  = false;
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_H)) {
            m_findVisible     = true;
            m_replaceVisible  = true;
        }
        if (m_findVisible && ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_findVisible = false;
    }

    if (ImGui::BeginMenuBar()) {

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                m_editor.SetText("");
                m_filePath = "";
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
                doOpen();
            ImGui::Separator();
            bool noPath = m_filePath.empty();
            ImGui::BeginDisabled(m_editor.IsReadOnlyEnabled());
            if (ImGui::MenuItem(noPath ? "Save As..." : "Save", "Ctrl+S"))
                doSave();
            ImGui::EndDisabled();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
                doSaveAs();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            bool ro = m_editor.IsReadOnlyEnabled();
            ImGui::BeginDisabled(!m_editor.CanUndo() || ro);
            if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                m_editor.Undo();
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!m_editor.CanRedo() || ro);
            if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                m_editor.Redo();
            ImGui::EndDisabled();
            ImGui::Separator();
            if (ImGui::MenuItem("Find", "Ctrl+F")) {
                m_findVisible    = true;
                m_replaceVisible = false;
            }
            if (ImGui::MenuItem("Find & Replace", "Ctrl+H")) {
                m_findVisible    = true;
                m_replaceVisible = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Read Only", nullptr, ro))
                m_editor.SetReadOnlyEnabled(!ro);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::BeginMenu("Language")) {
                const struct {
                    const char* label;
                    TextEditor::LanguageDefinitionId id;
                } kLangs[] = {
                    {"None", TextEditor::LanguageDefinitionId::None},
                    {"C++", TextEditor::LanguageDefinitionId::Cpp},
                    {"C", TextEditor::LanguageDefinitionId::C},
                    {"C#", TextEditor::LanguageDefinitionId::Cs},
                    {"Python", TextEditor::LanguageDefinitionId::Python},
                    {"Lua", TextEditor::LanguageDefinitionId::Lua},
                    {"JSON", TextEditor::LanguageDefinitionId::Json},
                    {"SQL", TextEditor::LanguageDefinitionId::Sql},
                    {"AngelScript", TextEditor::LanguageDefinitionId::AngelScript},
                    {"GLSL", TextEditor::LanguageDefinitionId::Glsl},
                    {"HLSL", TextEditor::LanguageDefinitionId::Hlsl},
                };
                auto curLang = m_editor.GetLanguageDefinition();
                for (auto& l : kLangs)
                    if (ImGui::MenuItem(l.label, nullptr, curLang == l.id))
                        m_editor.SetLanguageDefinition(l.id);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Colour Theme")) {
                const struct {
                    const char*           label;
                    TextEditor::PaletteId id;
                } kPals[] = {
                    {"Dark", TextEditor::PaletteId::Dark},
                    {"Light", TextEditor::PaletteId::Light},
                    {"Mariana", TextEditor::PaletteId::Mariana},
                    {"Retro Blue", TextEditor::PaletteId::RetroBlue},
                };
                auto curPal = m_editor.GetPalette();
                for (auto& p : kPals)
                    if (ImGui::MenuItem(p.label, nullptr, curPal == p.id))
                        m_editor.SetPalette(p.id);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            bool lineNums = m_editor.IsShowLineNumbersEnabled();
            if (ImGui::MenuItem("Line Numbers", nullptr, lineNums))
                m_editor.SetShowLineNumbersEnabled(!lineNums);
            bool autoInd = m_editor.IsAutoIndentEnabled();
            if (ImGui::MenuItem("Auto Indent", nullptr, autoInd))
                m_editor.SetAutoIndentEnabled(!autoInd);
            bool showWS = m_editor.IsShowWhitespacesEnabled();
            if (ImGui::MenuItem("Show Whitespace", nullptr, showWS))
                m_editor.SetShowWhitespacesEnabled(!showWS);
            bool shortTabs = m_editor.IsShortTabsEnabled();
            if (ImGui::MenuItem("Short Tabs", nullptr, shortTabs))
                m_editor.SetShortTabsEnabled(!shortTabs);
            ImGui::Separator();
            {
                int tabSz = m_editor.GetTabSize();
                ImGui::SetNextItemWidth(60.f);
                if (ImGui::DragInt("Tab Size", &tabSz, 1.f, 1, 8))
                    m_editor.SetTabSize(tabSz);
            }
            {
                float ls = m_editor.GetLineSpacing();
                ImGui::SetNextItemWidth(60.f);
                if (ImGui::DragFloat("Line Spacing", &ls, 0.05f, 1.0f, 2.0f, "%.2f"))
                    m_editor.SetLineSpacing(ls);
            }
            ImGui::EndMenu();
        }

        if (!m_filePath.empty()) {
            const std::string fname = ofFilePath::getFileName(m_filePath);
            float           fW      = ImGui::CalcTextSize(fname.c_str()).x + 8.f;
            float           avail     = ImGui::GetContentRegionAvail().x;
            if (avail > fW)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - fW);
            ImGui::TextDisabled("%s", fname.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", m_filePath.c_str());
        }

        ImGui::EndMenuBar();
    }

    if (m_findVisible) {
        ImGui::Separator();

        ImGui::SetNextItemWidth(280.f);
        bool enterPressed = ImGui::InputText("##find", m_findBuf, sizeof(m_findBuf),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ImGui::Checkbox("Aa", &m_caseSensitive);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Case Sensitive");
        ImGui::SameLine();

        auto doFindNext = [&] {
            if (m_findBuf[0] != '\0')
                m_editor.SelectNextOccurrenceOf(m_findBuf, (int)strlen(m_findBuf), m_caseSensitive);
        };
        auto doFindAll = [&] {
            if (m_findBuf[0] != '\0')
                m_editor.SelectAllOccurrencesOf(m_findBuf, (int)strlen(m_findBuf), m_caseSensitive);
        };

        if (ImGui::SmallButton("Next##code_find") || enterPressed)
            doFindNext();
        ImGui::SameLine();
        if (ImGui::SmallButton("All##code_find"))
            doFindAll();
        ImGui::SameLine();
        if (ImGui::SmallButton("x##code_find"))
            m_findVisible = false;

        if (m_findBuf[0] != '\0') {
            const std::string full   = m_editor.GetText();
            const std::string term   = m_findBuf;
            int               count = 0;
            std::string haystack =
                m_caseSensitive
                    ? full
                    : [&] {
                          std::string lc = full;
                          std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
                          return lc;
                      }();
            std::string needle =
                m_caseSensitive
                    ? term
                    : [&] {
                          std::string lc = term;
                          std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
                          return lc;
                      }();
            for (size_t pos = 0; (pos = haystack.find(needle, pos)) != std::string::npos;
                 pos += needle.size())
                ++count;
            ImGui::SameLine();
            if (count == 0)
                ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "no matches");
            else
                ImGui::TextDisabled("%d match%s", count, count == 1 ? "" : "es");
        }

        if (m_replaceVisible) {
            ImGui::SetNextItemWidth(280.f);
            ImGui::InputText("##replace", m_replaceBuf, sizeof(m_replaceBuf));
            ImGui::SameLine();

            bool hasMatch = m_editor.AnyCursorHasSelection();
            ImGui::BeginDisabled(!hasMatch || m_editor.IsReadOnlyEnabled());
            if (ImGui::SmallButton("Replace##code_find_current")) {
                if (m_findBuf[0] != '\0') {
                    std::string       src       = m_editor.GetText();
                    const std::string term       = m_findBuf;
                    const std::string repl       = m_replaceBuf;
                    std::string       lower_src  = src;
                    std::string       lower_term = term;
                    if (!m_caseSensitive) {
                        std::transform(lower_src.begin(), lower_src.end(), lower_src.begin(),
                                       ::tolower);
                        std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(),
                                       ::tolower);
                    }
                    size_t pos = lower_src.find(lower_term);
                    if (pos != std::string::npos) {
                        src.replace(pos, term.size(), repl);
                        m_editor.SetText(src);
                        m_editor.SelectNextOccurrenceOf(m_findBuf, (int)strlen(m_findBuf),
                                                         m_caseSensitive);
                    }
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(m_editor.IsReadOnlyEnabled());
            if (ImGui::SmallButton("Replace All##code_find_all")) {
                if (m_findBuf[0] != '\0') {
                    std::string       src  = m_editor.GetText();
                    const std::string term = m_findBuf;
                    const std::string repl = m_replaceBuf;
                    int               replCount = 0;
                    if (m_caseSensitive) {
                        for (size_t pos = 0; (pos = src.find(term, pos)) != std::string::npos;) {
                            src.replace(pos, term.size(), repl);
                            pos += repl.size();
                            ++replCount;
                        }
                    } else {
                        std::string lower = src;
                        std::string lterm = term;
                        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                        std::transform(lterm.begin(), lterm.end(), lterm.begin(), ::tolower);
                        for (size_t pos = 0; (pos = lower.find(lterm, pos)) != std::string::npos;) {
                            src.replace(pos, term.size(), repl);
                            lower.replace(pos, lterm.size(),
                                          repl.size() > 0 ? std::string(repl.size(), ' ')
                                                          : std::string());
                            pos += repl.size();
                            ++replCount;
                        }
                    }
                    if (replCount > 0)
                        m_editor.SetText(src);
                    ofLogNotice("ofxKit::CodeEditor") << "Replaced " << replCount
                                                      << " occurrence(s).";
                }
            }
            ImGui::EndDisabled();
        }
    }

    ImGui::Separator();

    const float statusH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
    ImVec2      editorSize = ImGui::GetContentRegionAvail();
    editorSize.y -= statusH;

    m_editor.Render("##code", ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows), editorSize);

    ImGui::Separator();
    int curLine = 0, curCol = 0;
    m_editor.GetCursorPosition(curLine, curCol);
    ImGui::TextDisabled("Ln %d, Col %d   |   %d lines   |   %s%s", curLine + 1, curCol + 1,
                        m_editor.GetLineCount(), m_editor.GetLanguageDefinitionName(),
                        m_editor.IsReadOnlyEnabled() ? "   [Read Only]" : "");

    ImGui::End();
}

}
