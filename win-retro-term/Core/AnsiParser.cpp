#include "pch.h"
#include "AnsiParser.h"
#include <Windows.h>

namespace winrt::win_retro_term::Core
{
    AnsiParser::AnsiParser(ITerminalActions& actions) : m_terminalActions(actions), m_currentState(ParserState::GROUND)
    {
        ClearSequenceState();
    }

    void AnsiParser::ClearSequenceState()
    {
        m_params.clear();
        m_intermediates.clear();
    }

    void AnsiParser::ParamDigit(wchar_t digit) {
        if (m_params.empty()) {
            m_params.push_back(0);
        }
        int currentParamIndex = static_cast<int>(m_params.size() - 1);
        long long val = m_params[currentParamIndex];
        val = val * 10 + (digit - L'0');

        // Clamp to avoid overflow, typical terminal int range
        if (val > std::numeric_limits<int>::max()) {
            val = std::numeric_limits<int>::max();
        }
        m_params[currentParamIndex] = static_cast<int>(val);
    }

    void AnsiParser::ParamSeparator() {
        if (m_params.size() < MAX_PARAMS) {
            if (m_params.empty()) {
                m_params.push_back(0);
            }
            m_params.push_back(0);
        }
    }

    void AnsiParser::CollectIntermediate(wchar_t ch) {
        if (m_intermediates.length() < 16) {
            m_intermediates += ch;
        }
    }

    int AnsiParser::GetParam(size_t index, int defaultValue) const {
        if (index < m_params.size()) {
            if (m_params[index] == 0 && defaultValue != 0) {
                return defaultValue;
            }
            return m_params[index];
        }
        return defaultValue;
    }

    void AnsiParser::DispatchCsi(wchar_t finalChar) {
        OutputDebugString((L"AnsiParser: CSI Dispatch - Final: '" + std::wstring(1, finalChar) + L"'").c_str());
        OutputDebugString(L" Params: {");
        for (size_t i = 0; i < m_params.size(); ++i) {
            OutputDebugString(std::to_wstring(m_params[i]).c_str());
            if (i < m_params.size() - 1) OutputDebugString(L", ");
        }
        OutputDebugString(L"} Intermediates: '");
        OutputDebugString(m_intermediates.c_str());
        OutputDebugString(L"'\n");

        if (!m_intermediates.empty() && m_intermediates != L"?" && m_intermediates != L"!") {
            OutputDebugString((L"AnsiParser: CSI Dispatch with unhandled intermediates '" + m_intermediates + L"', ignoring for now.\n").c_str());
            return;
        }

        switch (finalChar) {
        case L'A': // CUU - Cursor Up
            if (m_intermediates.empty()) {
                m_terminalActions.CursorUp(GetParam(0, 1));
            }
            break;
        case L'B': // CUD - Cursor Down
            if (m_intermediates.empty()) {
                m_terminalActions.CursorDown(GetParam(0, 1));
            }
            break;
        case L'C': // CUF - Cursor Forward
            if (m_intermediates.empty()) {
                m_terminalActions.CursorForward(GetParam(0, 1));
            }
            break;
        case L'D': // CUB - Cursor Back
            if (m_intermediates.empty()) {
                m_terminalActions.CursorBack(GetParam(0, 1));
            }
            break;
        case L'H': // CUP - Cursor Position
        case L'f': // HVP - Horizontal and Vertical Position (same as CUP)
            if (m_intermediates.empty()) {
                m_terminalActions.CursorPosition(GetParam(0, 1), GetParam(1, 1));
            }
            break;
        case L'J': // ED - Erase in Display
            if (m_intermediates.empty() || m_intermediates == L"?") { // CSI ? J is DECSED
                m_terminalActions.EraseInDisplay(GetParam(0, 0));
            }
            break;
        case L'K': // EL - Erase in Line
            if (m_intermediates.empty() || m_intermediates == L"?") { // CSI ? K is DECSEL
                m_terminalActions.EraseInLine(GetParam(0, 0));
            }
            break;
        case L'm': // SGR - Select Graphic Rendition
            if (m_params.empty()) {
                m_terminalActions.SetGraphicsRendition({ 0 });
            }
            else {
                m_terminalActions.SetGraphicsRendition(m_params);
            }
            break;
        default:
            OutputDebugString((L"AnsiParser: Unhandled CSI final character: '" + std::wstring(1, finalChar) + L"' with intermediates '" + m_intermediates + L"'\n").c_str());
            break;
        }
    }

    void AnsiParser::DispatchEscapeSequence(wchar_t finalChar) {
        OutputDebugString((L"AnsiParser: ESC Dispatch - Intermediates: '" + m_intermediates + L"' Final: '" + std::wstring(1, finalChar) + L"'\n").c_str());

        if (m_intermediates.length() == 1) {
            wchar_t intermediate = m_intermediates[0];
            uint8_t targetSet = 0xFF;

            switch (intermediate) {
            case L'(': targetSet = 0; break; // G0
            case L')': targetSet = 1; break; // G1
            case L'-': targetSet = 1; break; // G1 (VT300)
            case L'*': targetSet = 2; break; // G2
            case L'.': targetSet = 2; break; // G2 (VT300)
            case L'+': targetSet = 3; break; // G3
            case L'/': targetSet = 3; break; // G3 (VT300)
            default:
                // Unknown intermediate for SCS
                break;
            }

            if (targetSet != 0xFF) {
                m_terminalActions.DesignateCharSet(targetSet, finalChar);
                return;
            }
        }

        if (m_intermediates.empty()) {
            switch (finalChar) {
            case L'D': m_terminalActions.LineFeed(); break; // IND - Index (move down one line)
            case L'E': m_terminalActions.CarriageReturn(); m_terminalActions.LineFeed(); break; // NEL - Next Line
            case L'M': break; // RI - Reverse Index (move up one line, scroll if at top)
            default:
                OutputDebugString((L"AnsiParser: Unhandled simple ESC sequence: ESC " + std::wstring(1, finalChar) + L"\n").c_str());
                break;
            }
        }
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

    void AnsiParser::ProcessChar(wchar_t ch)
    {
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
            if (ch == L'[') // CSI
            {
                m_currentState = ParserState::CSI_ENTRY;
            }
            else if ((ch >= L'(' && ch <= L'+') || (ch >= L'-' && ch <= L'/')) // SCS Intermediate characters (for G0, G1, G2, G3 designation)
            {
                CollectIntermediate(ch);
                m_currentState = ParserState::ESCAPE_INTERMEDIATE;
            }
            else if (ch == L'P') // DCS
            {
                m_currentState = ParserState::DCS_ENTRY;
            }
            else if (ch == L']') // OSC
            {
                m_currentState = ParserState::OSC_STRING;
            }
            else if (ch == L'X')
            {
                m_currentState = ParserState::OSC_STRING;
            }
            else if (ch == L'^')
            {
                m_currentState = ParserState::OSC_STRING;
            }
            else if (ch == L'_')
            {
                m_currentState = ParserState::OSC_STRING;
            }
            else if (ch >= 0x28 && ch <= 0x2F)
            {
                CollectIntermediate(ch);
                m_currentState = ParserState::ESCAPE_INTERMEDIATE;
            }
            else if (ch >= 0x40 && ch <= 0x5F) 
            {
                DispatchEscapeSequence(ch);
                m_currentState = ParserState::GROUND;
            }
            else
            {
                m_currentState = ParserState::GROUND;
            }
            break;

        case ParserState::ESCAPE_INTERMEDIATE:
            if (ch >= 0x20 && ch <= 0x7E) {
                DispatchEscapeSequence(ch);
                m_currentState = ParserState::GROUND;
            }
            else if (ch == 0x1B) { // ESC (Abort)
                m_currentState = ParserState::GROUND;
            }
            else {
                // Unexpected character
                m_currentState = ParserState::GROUND;
            }
            break;


        case ParserState::CSI_ENTRY:
            if (ch >= L'0' && ch <= L'9') { // Parameter digit
                ParamDigit(ch);
                m_currentState = ParserState::CSI_PARAM;
            }
            else if (ch == L';') { // Parameter separator
                ParamSeparator();
                m_currentState = ParserState::CSI_PARAM;
            }
            else if (ch >= L'<' && ch <= L'?') { // Private parameter characters: < = > ?
                CollectIntermediate(ch);
                m_currentState = ParserState::CSI_INTERMEDIATE;
            }
            else if (ch >= 0x20 && ch <= 0x2F) { // Intermediate characters ! " # $ % & ' ( ) * + , - . /
                CollectIntermediate(ch);
                m_currentState = ParserState::CSI_INTERMEDIATE;
            }
            else if (ch >= 0x40 && ch <= 0x7E) { // Final character for CSI sequence
                DispatchCsi(ch);
                m_currentState = ParserState::GROUND;
            }
            else if (ch == 0x1B) { // ESC within CSI (abort)
                m_currentState = ParserState::GROUND;
            }
            else {
                // Unexpected char in CSI_ENTRY
                m_currentState = ParserState::GROUND;
            }
            break;

        case ParserState::CSI_PARAM:
            if (ch >= L'0' && ch <= L'9') { // Parameter digit
                ParamDigit(ch);
            }
            else if (ch == L';') { // Parameter separator
                ParamSeparator();
            }
            else if (ch >= 0x20 && ch <= 0x2F) { // Intermediate characters
                CollectIntermediate(ch);
                m_currentState = ParserState::CSI_INTERMEDIATE;
            }
            else if (ch >= 0x40 && ch <= 0x7E) { // Final character
                DispatchCsi(ch);
                m_currentState = ParserState::GROUND;
            }
            else if (ch == 0x1B) { // ESC within CSI (abort)
                m_currentState = ParserState::GROUND;
            }
            else {
                // Unexpected char in CSI_PARAM
                m_currentState = ParserState::GROUND;
            }
            break;

        case ParserState::CSI_INTERMEDIATE:
            if (ch >= 0x20 && ch <= 0x2F) { // More intermediate characters
                CollectIntermediate(ch);
            }
            else if (ch >= 0x40 && ch <= 0x7E) { // Final character
                DispatchCsi(ch);
                m_currentState = ParserState::GROUND;
            }
            else if (ch == 0x1B) { // ESC within CSI (abort)
                m_currentState = ParserState::GROUND;
            }
            else {
                m_currentState = ParserState::GROUND; // Or an IGNORE state
            }
            break;

            // Future states will be added here
        default:
            OutputDebugStringA("AnsiParser: Reached unknown state, resetting to GROUND.\n");
            m_currentState = ParserState::GROUND;
            break;
        }
    }
}