#include "CodeEditorPanel.h"

#include "ofMain.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <regex>

namespace ofkitty {

void CodeEditorPanel::setup()
{
    m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
    m_editor.SetPalette(TextEditor::PaletteId::Dark);
    m_editor.SetShowLineNumbersEnabled(true);
    m_editor.SetShowWhitespacesEnabled(false);
    m_editor.SetLineSpacing(1.0f);
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

void CodeEditorPanel::setText(const std::string& text,
                              TextEditor::LanguageDefinitionId lang)
{
    if (lang != TextEditor::LanguageDefinitionId::None)
        m_editor.SetLanguageDefinition(lang);
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

void CodeEditorPanel::setSidebarEntries(std::vector<SidebarEntry> entries)
{
    m_sidebarEntries = std::move(entries);
    m_sidebarSelected = -1;
}

int CodeEditorPanel::getLineCount() const
{
    return std::max(1, m_editor.GetLineCount());
}

int CodeEditorPanel::getCursorLine() const
{
    int line = 0, col = 0;
    m_editor.GetCursorPosition(line, col);
    return line;
}

void CodeEditorPanel::setCursorLine(int line)
{
    line = std::clamp(line, 0, getLineCount() - 1);
    m_editor.SetCursorPosition(line, 0);
}

void CodeEditorPanel::setHighlightLine(int line)
{
    m_highlightLine = line;
    if (line < 0) {
        m_editor.ClearSelections();
        return;
    }
    line = std::clamp(line, 0, getLineCount() - 1);
    m_editor.ClearSelections();
    m_editor.SetCursorPosition(line, 0);
    m_editor.SetViewAtLine(line, TextEditor::SetViewAtLineMode::Centered);
}

void CodeEditorPanel::draw(bool& visible)
{
    ImGui::SetNextWindowSize(ImVec2(820, 580), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_MenuBar
                              | ImGuiWindowFlags_NoFocusOnAppearing;
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
        else if (ext == "xml" || ext == "svg")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Xml);
        else if (ext == "sql")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Sql);
        else if (ext == "gcode" || ext == "nc" || ext == "cnc" || ext == "tap")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Gcode);
        else if (ext == "md" || ext == "markdown")
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Markdown);
        else
            m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
    };

    // Returns {filter string, default filename} based on the current language.
    auto getDialogParams = [this]() -> std::pair<std::string, std::string> {
        auto lang = m_editor.GetLanguageDefinition();
        if (lang == TextEditor::LanguageDefinitionId::Gcode)
            return {"G-code{.gcode,.nc,.cnc,.tap},All Files{.*}", "untitled.gcode"};
        if (lang == TextEditor::LanguageDefinitionId::Glsl)
            return {"Shaders{.glsl,.vert,.frag,.hlsl},All Files{.*}", "untitled.glsl"};
        if (lang == TextEditor::LanguageDefinitionId::Hlsl)
            return {"Shaders{.hlsl,.vert,.frag,.glsl},All Files{.*}", "untitled.hlsl"};
        if (lang == TextEditor::LanguageDefinitionId::Cpp || lang == TextEditor::LanguageDefinitionId::C)
            return {"C/C++{.cpp,.c,.h,.hpp},All Files{.*}", "untitled.cpp"};
        if (lang == TextEditor::LanguageDefinitionId::Cs)
            return {"C#{.cs},All Files{.*}", "untitled.cs"};
        if (lang == TextEditor::LanguageDefinitionId::Python)
            return {"Python{.py},All Files{.*}", "untitled.py"};
        if (lang == TextEditor::LanguageDefinitionId::Lua)
            return {"Lua{.lua},All Files{.*}", "untitled.lua"};
        if (lang == TextEditor::LanguageDefinitionId::Json)
            return {"JSON{.json},All Files{.*}", "untitled.json"};
        if (lang == TextEditor::LanguageDefinitionId::Xml)
            return {"XML{.xml,.svg},All Files{.*}", "untitled.xml"};
        if (lang == TextEditor::LanguageDefinitionId::Sql)
            return {"SQL{.sql},All Files{.*}", "untitled.sql"};
        if (lang == TextEditor::LanguageDefinitionId::Markdown)
            return {"Markdown{.md,.markdown},All Files{.*}", "untitled.md"};
        return {"Source{.cpp,.h,.hpp,.c,.glsl,.vert,.frag,.hlsl,.py,.lua},"
                "G-code{.gcode,.nc,.cnc},"
                "Data{.json,.xml,.yaml,.txt,.md},"
                "All Files{.*}", "untitled.txt"};
    };

    auto doOpen = [&] {
        if (!m_openFile)
            return;
        m_openFile("code_open", "Open File",
                   "Source{.cpp,.h,.hpp,.c,.cs,.py,.lua,.js,.ts},"
                   "Shaders{.glsl,.vert,.frag,.hlsl},"
                   "G-code{.gcode,.nc,.cnc,.tap},"
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
            auto [filter, defaultName] = getDialogParams();
            m_saveFile("code_save", "Save As", filter, defaultName,
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
        auto [filter, defaultName] = getDialogParams();
        const std::string fname = m_filePath.empty() ? defaultName
                                                      : ofFilePath::getFileName(m_filePath);
        m_saveFile("code_save_as", "Save As", filter, fname,
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
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_F)) {
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
            ImGui::BeginDisabled(noPath || m_editor.IsReadOnlyEnabled());
            if (ImGui::MenuItem("Save", "Ctrl+S"))
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
            if (ImGui::MenuItem("Find & Replace", "Ctrl+Shift+F")) {
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
                    {"XML", TextEditor::LanguageDefinitionId::Xml},
                    {"SQL", TextEditor::LanguageDefinitionId::Sql},
                    {"AngelScript", TextEditor::LanguageDefinitionId::AngelScript},
                    {"GLSL", TextEditor::LanguageDefinitionId::Glsl},
                    {"HLSL", TextEditor::LanguageDefinitionId::Hlsl},
                    {"G-code", TextEditor::LanguageDefinitionId::Gcode},
                    {"Markdown", TextEditor::LanguageDefinitionId::Markdown},
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

        // [.*] Regex toggle
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_useRegex ? ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)
                       : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::SmallButton("[.*]"))
            m_useRegex = !m_useRegex;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Regular Expression");

        // [""] Whole Word toggle (disabled when regex is active)
        ImGui::SameLine();
        ImGui::BeginDisabled(m_useRegex);
        ImGui::PushStyleColor(ImGuiCol_Text,
            (m_wholeWord && !m_useRegex)
                ? ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)
                : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::SmallButton("[\"]"))
            m_wholeWord = !m_wholeWord;
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Whole Word");

        ImGui::SameLine();

        // Helpers ----------------------------------------------------------------

        // Convert flat-string offset → (line, col)
        auto offsetToCoords = [](const std::string& text, size_t offset) -> std::pair<int,int> {
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < text.size(); ++i) {
                if (text[i] == '\n') { ++line; col = 0; }
                else                 { ++col; }
            }
            return {line, col};
        };

        auto doFindNext = [&] {
            if (m_findBuf[0] == '\0') return;
            if (m_useRegex) {
                try {
                    auto rxFlags = std::regex_constants::ECMAScript;
                    if (!m_caseSensitive) rxFlags |= std::regex_constants::icase;
                    std::regex  pat(m_findBuf, rxFlags);
                    std::string full = m_editor.GetText();
                    // Compute flat offset of current cursor
                    int curLine = 0, curCol = 0;
                    m_editor.GetCursorPosition(curLine, curCol);
                    size_t startOff = 0;
                    { int ln = 0, col = 0;
                      for (size_t i = 0; i < full.size(); ++i) {
                          if (ln == curLine && col >= curCol) { startOff = i; break; }
                          if (full[i] == '\n') { ++ln; col = 0; } else { ++col; }
                      }
                    }
                    std::smatch sm;
                    bool found = std::regex_search(full.cbegin() + (std::ptrdiff_t)startOff,
                                                   full.cend(), sm, pat);
                    if (!found)
                        found = std::regex_search(full.cbegin(), full.cend(), sm, pat);
                    if (found) {
                        size_t s = (size_t)(sm.prefix().first - full.cbegin()) + (size_t)sm.position();
                        size_t e = s + (size_t)sm.length();
                        auto [sl, sc] = offsetToCoords(full, s);
                        auto [el, ec] = offsetToCoords(full, e);
                        m_editor.SelectRegion(sl, sc, el, ec);
                    }
                } catch (std::regex_error&) {}
            } else {
                m_editor.SelectNextOccurrenceOf(m_findBuf, (int)strlen(m_findBuf),
                                                m_caseSensitive, m_wholeWord);
            }
        };

        auto doFindAll = [&] {
            if (m_findBuf[0] == '\0') return;
            if (m_useRegex) {
                doFindNext(); // regex multi-cursor not supported; Replace All still works
            } else {
                m_editor.SelectAllOccurrencesOf(m_findBuf, (int)strlen(m_findBuf),
                                                m_caseSensitive, m_wholeWord);
            }
        };

        if (ImGui::SmallButton("Next##code_find") || enterPressed)
            doFindNext();
        ImGui::SameLine();
        if (ImGui::SmallButton("All##code_find"))
            doFindAll();
        ImGui::SameLine();
        if (ImGui::SmallButton("x##code_find"))
            m_findVisible = false;

        // Match count display ----------------------------------------------------
        if (m_findBuf[0] != '\0') {
            int count = 0;
            bool invalidRegex = false;

            if (m_useRegex) {
                try {
                    auto rxFlags = std::regex_constants::ECMAScript;
                    if (!m_caseSensitive) rxFlags |= std::regex_constants::icase;
                    std::regex  pat(m_findBuf, rxFlags);
                    std::string full = m_editor.GetText();
                    count = (int)std::distance(
                        std::sregex_iterator(full.begin(), full.end(), pat),
                        std::sregex_iterator{});
                } catch (std::regex_error&) {
                    invalidRegex = true;
                }
            } else {
                const std::string full = m_editor.GetText();
                const std::string term = m_findBuf;
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
                auto isWordChar = [](char c) {
                    return std::isalnum((unsigned char)c) || c == '_';
                };
                for (size_t pos = 0;
                     (pos = haystack.find(needle, pos)) != std::string::npos;
                     pos += needle.size()) {
                    if (m_wholeWord) {
                        bool ok = (pos == 0 || !isWordChar(haystack[pos - 1])) &&
                                  (pos + needle.size() >= haystack.size() ||
                                   !isWordChar(haystack[pos + needle.size()]));
                        if (!ok) continue;
                    }
                    ++count;
                }
            }

            ImGui::SameLine();
            if (invalidRegex)
                ImGui::TextColored({1.f, 0.6f, 0.2f, 1.f}, "invalid regex");
            else if (count == 0)
                ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "no matches");
            else
                ImGui::TextDisabled("%d match%s", count, count == 1 ? "" : "es");
        }

        // Replace bar ------------------------------------------------------------
        if (m_replaceVisible) {
            ImGui::SetNextItemWidth(280.f);
            ImGui::InputText("##replace", m_replaceBuf, sizeof(m_replaceBuf));
            ImGui::SameLine();

            bool hasMatch = m_editor.AnyCursorHasSelection();
            ImGui::BeginDisabled(!hasMatch || m_editor.IsReadOnlyEnabled());
            if (ImGui::SmallButton("Replace##code_find_current")) {
                if (m_findBuf[0] != '\0') {
                    if (m_useRegex) {
                        try {
                            auto rxFlags = std::regex_constants::ECMAScript;
                            if (!m_caseSensitive) rxFlags |= std::regex_constants::icase;
                            std::regex  pat(m_findBuf, rxFlags);
                            std::string src = m_editor.GetText();
                            std::smatch sm;
                            if (std::regex_search(src.cbegin(), src.cend(), sm, pat)) {
                                size_t pos = (size_t)sm.position();
                                src.replace(pos, (size_t)sm.length(), m_replaceBuf);
                                m_editor.SetText(src);
                                doFindNext();
                            }
                        } catch (std::regex_error&) {}
                    } else {
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
                        auto isWordChar = [](char c) {
                            return std::isalnum((unsigned char)c) || c == '_';
                        };
                        size_t pos = 0;
                        while ((pos = lower_src.find(lower_term, pos)) != std::string::npos) {
                            if (!m_wholeWord ||
                                ((pos == 0 || !isWordChar(lower_src[pos - 1])) &&
                                 (pos + lower_term.size() >= lower_src.size() ||
                                  !isWordChar(lower_src[pos + lower_term.size()])))) {
                                src.replace(pos, term.size(), repl);
                                m_editor.SetText(src);
                                m_editor.SelectNextOccurrenceOf(m_findBuf, (int)strlen(m_findBuf),
                                                                 m_caseSensitive, m_wholeWord);
                                break;
                            }
                            pos += lower_term.size();
                        }
                    }
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(m_editor.IsReadOnlyEnabled());
            if (ImGui::SmallButton("Replace All##code_find_all")) {
                if (m_findBuf[0] != '\0') {
                    if (m_useRegex) {
                        try {
                            auto rxFlags = std::regex_constants::ECMAScript;
                            if (!m_caseSensitive) rxFlags |= std::regex_constants::icase;
                            std::regex  pat(m_findBuf, rxFlags);
                            std::string src    = m_editor.GetText();
                            std::string result = std::regex_replace(src, pat, m_replaceBuf);
                            if (result != src)
                                m_editor.SetText(result);
                        } catch (std::regex_error&) {}
                    } else {
                        std::string       src  = m_editor.GetText();
                        const std::string term = m_findBuf;
                        const std::string repl = m_replaceBuf;
                        int               replCount = 0;
                        auto isWordChar = [](char c) {
                            return std::isalnum((unsigned char)c) || c == '_';
                        };
                        if (m_caseSensitive) {
                            for (size_t pos = 0; (pos = src.find(term, pos)) != std::string::npos;) {
                                if (m_wholeWord) {
                                    bool ok = (pos == 0 || !isWordChar(src[pos - 1])) &&
                                              (pos + term.size() >= src.size() ||
                                               !isWordChar(src[pos + term.size()]));
                                    if (!ok) { pos += term.size(); continue; }
                                }
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
                                if (m_wholeWord) {
                                    bool ok = (pos == 0 || !isWordChar(lower[pos - 1])) &&
                                              (pos + lterm.size() >= lower.size() ||
                                               !isWordChar(lower[pos + lterm.size()]));
                                    if (!ok) { pos += lterm.size(); continue; }
                                }
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
            }
            ImGui::EndDisabled();
        }
    }

    ImGui::Separator();

    const float statusH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
    const float sidebarW = m_sidebarEntries.empty() ? 0.f : 180.f;

    if (sidebarW > 0.f) {
        ImGui::BeginChild("##code_sidebar", ImVec2(sidebarW, -statusH), ImGuiChildFlags_Borders);
        for (int i = 0; i < (int)m_sidebarEntries.size(); ++i) {
            const auto& e = m_sidebarEntries[i];
            const bool active = e.isActive || m_sidebarSelected == i;
            if (ImGui::Selectable(e.label.c_str(), active, ImGuiSelectableFlags_AllowOverlap)) {
                m_sidebarSelected = i;
                if (!e.path.empty()) {
                    std::ifstream ifs(e.path);
                    if (ifs) {
                        m_filePath = e.path;
                        m_editor.SetText(std::string(
                            std::istreambuf_iterator<char>(ifs),
                            std::istreambuf_iterator<char>()));
                        m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Gcode);
                    }
                }
            }
            if (!e.path.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", e.path.c_str());
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImVec2 editorSize = ImGui::GetContentRegionAvail();
    editorSize.y -= statusH;

    m_editor.Render("##code", ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows), editorSize);

    if (m_syncPlaybackFromCursor && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        const int curLine = getCursorLine();
        if (curLine != m_lastReportedCursorLine) {
            m_lastReportedCursorLine = curLine;
            if (m_onCursorLineChanged) m_onCursorLineChanged(curLine);
        }
    }

    ImGui::Separator();
    int curLine = 0, curCol = 0;
    m_editor.GetCursorPosition(curLine, curCol);
    ImGui::TextDisabled("Ln %d, Col %d   |   %d lines   |   %s%s", curLine + 1, curCol + 1,
                        m_editor.GetLineCount(), m_editor.GetLanguageDefinitionName(),
                        m_editor.IsReadOnlyEnabled() ? "   [Read Only]" : "");

    ImGui::End();
}

}
