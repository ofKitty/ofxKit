#pragma once

#include <functional>
#include <memory>
#include <string>

#include <ofxImGuiTextEdit/src/ofxImGuiTextEdit.h>

namespace ofkitty {

class CodeEditorPanel {
public:
    using ConfirmPath = std::function<void(const std::string& path)>;

    void setup();

    /// Route file dialogs through the host (`Runtime::openFileDialog` /
    /// `saveFileDialog`) so IGFD stays centralized.
    void setDialogCallbacks(
        std::function<void(const std::string& key,
                           const std::string& title,
                           const std::string& filters,
                           ConfirmPath onConfirm)> openFile,
        std::function<void(const std::string& key,
                           const std::string& title,
                           const std::string& filters,
                           const std::string& defaultFileName,
                           ConfirmPath onConfirm)> saveFile);

    void draw(bool& visible);

    void           setText(const std::string& text,
                           TextEditor::LanguageDefinitionId lang = TextEditor::LanguageDefinitionId::None);
    std::string    getText() const;
    void           setLanguage(TextEditor::LanguageDefinitionId lang);

    TextEditor&       editor() { return m_editor; }
    const TextEditor& editor() const { return m_editor; }

private:
    TextEditor  m_editor;
    std::string m_filePath;

    std::function<void(const std::string& key,
                       const std::string& title,
                       const std::string& filters,
                       ConfirmPath onConfirm)> m_openFile;
    std::function<void(const std::string& key,
                       const std::string& title,
                       const std::string& filters,
                       const std::string& defaultFileName,
                       ConfirmPath onConfirm)> m_saveFile;

    char m_findBuf[256]    = {};
    char m_replaceBuf[256] = {};
    bool m_caseSensitive   = false;
    bool m_useRegex        = false;
    bool m_wholeWord       = false;
    bool m_findVisible     = false;
    bool m_replaceVisible  = false;
};

}
