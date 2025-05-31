#include "pch.h"
#include "AnsiParser.h"
#include <Windows.h>

namespace winrt::win_retro_term::Core 
{
    AnsiParser::AnsiParser(ITerminalActions& actions) : m_terminalActions(actions), m_currentState(ParserState::GROUND) {}

    void AnsiParser::ClearSequenceState() 
    {
    }

    void AnsiParser::Parse(const char* data, size_t length) 
    {
        if (length == 0) return;

        // Prepend any partial UTF-8 sequence from the previous call
        std::vector<char> currentData;
        if (!m_utf8PartialSequence.empty()) {
            currentData.insert(currentData.end(), m_utf8PartialSequence.begin(), m_utf8PartialSequence.end());
            m_utf8PartialSequence.clear();
        }
        currentData.insert(currentData.end(), data, data + length);

        // Convert UTF-8 (common from ConPTY) to wide char (UTF-16)
        if (currentData.empty()) return;

        int wideCharCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, currentData.data(), static_cast<int>(currentData.size()), nullptr, 0);

        std::wstring wideString;
        if (wideCharCount > 0) {
            wideString.resize(wideCharCount);
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, currentData.data(), static_cast<int>(currentData.size()), &wideString[0], wideCharCount);
        }
        else 
        {
            DWORD error = GetLastError();
            if (error == ERROR_NO_UNICODE_TRANSLATION) 
            {
                size_t bytesToKeep = 0;
                if (currentData.size() >= 1 && (static_cast<unsigned char>(currentData.back()) & 0xC0) == 0xC0) bytesToKeep = 1;
                if (currentData.size() >= 2 && (static_cast<unsigned char>(currentData[currentData.size() - 2]) & 0xE0) == 0xE0 && (static_cast<unsigned char>(currentData.back()) & 0xC0) == 0x80) bytesToKeep = 2;
                if (currentData.size() >= 3 && (static_cast<unsigned char>(currentData[currentData.size() - 3]) & 0xF0) == 0xF0 && (static_cast<unsigned char>(currentData[currentData.size() - 2]) & 0xC0) == 0x80 && (static_cast<unsigned char>(currentData.back()) & 0xC0) == 0x80) bytesToKeep = 3;

                if (bytesToKeep > 0 && bytesToKeep < currentData.size()) 
                {
                    m_utf8PartialSequence.assign(currentData.end() - bytesToKeep, currentData.end());
                    // Try converting the part before the partial sequence
                    wideCharCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, currentData.data(), static_cast<int>(currentData.size() - bytesToKeep), nullptr, 0);
                    if (wideCharCount > 0) 
                    {
                        wideString.resize(wideCharCount);
                        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, currentData.data(), static_cast<int>(currentData.size() - bytesToKeep), &wideString[0], wideCharCount);
                    }
                    else 
                    {
                        OutputDebugStringA("AnsiParser: Failed to convert to wide char or no complete chars.\n");
                        return;
                    }
                }
                else if (bytesToKeep == currentData.size()) 
                {
                    m_utf8PartialSequence.assign(currentData.begin(), currentData.end());
                    return;
                }
                else 
                {
                    OutputDebugStringA("AnsiParser: MultiByteToWideChar failed with ERROR_NO_UNICODE_TRANSLATION.\n");
                    return;
                }
            }
            else if (error != 0) 
            {
                OutputDebugStringA("AnsiParser: MultiByteToWideChar failed.\n");
                return;
            }
        }

        for (wchar_t wc : wideString) {
            ProcessChar(wc);
        }
    }

    void AnsiParser::ProcessChar(wchar_t ch) {

        switch (m_currentState) {
        case ParserState::GROUND:
            if (ch >= 0x20 && ch <= 0x7F) {
                m_terminalActions.PrintChar(ch);
            }
            else if (ch == L'\x1B') { // ESC
                ClearSequenceState();
                m_currentState = ParserState::ESCAPE;
            }
            else if (ch == L'\n') { // LF
                m_terminalActions.LineFeed();
            }
            else if (ch == L'\r') { // CR
                m_terminalActions.CarriageReturn();
            }
            else if (ch == L'\b') { // BS
                m_terminalActions.Backspace();
            }
            else if (ch == L'\t') { // HT
                m_terminalActions.HorizontalTab();
            }
            else if (ch == L'\x07') { // BEL
                m_terminalActions.Bell();
            }
            else if ((ch >= 0x00 && ch <= 0x1A) || (ch >= 0x1C && ch <= 0x1F)) {
                m_terminalActions.ExecuteControlFunction(ch);
            }
            else if (ch >= 0x80) {
                // This is a simplification. Proper C1 handling is more involved.
                m_terminalActions.PrintChar(ch);
            }
            break;

        case ParserState::ESCAPE:
            m_currentState = ParserState::GROUND;
            break;

            // Future states will be added here
        default:
            OutputDebugStringA("AnsiParser: Reached unknown state, resetting to GROUND.\n");
            m_currentState = ParserState::GROUND;
            break;
        }
    }
}