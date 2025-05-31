#include "pch.h"
#include "AnsiParser.h"
#include <Windows.h>

namespace winrt::win_retro_term::Core 
{
    AnsiParser::AnsiParser(TerminalBuffer& buffer) : m_terminalBuffer(buffer) {}

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
        else {
            // Handle error or potential partial sequence.
            // If MultiByteToWideChar fails because the last bytes form an incomplete UTF-8 sequence,
            // we should save those bytes for the next call.
            DWORD error = GetLastError();
            if (error == ERROR_NO_UNICODE_TRANSLATION) {
                // This error can mean an invalid sequence or an incomplete one at the end.
                // A more robust solution would analyze the byte sequence to determine
                // how many bytes at the end form a valid partial UTF-8 character.
                // For simplicity here, we'll assume it might be a partial sequence if it's near the end.
                // This is a heuristic and not perfectly robust.
                size_t bytesToKeep = 0;
                if (currentData.size() >= 1 && (static_cast<unsigned char>(currentData.back()) & 0xC0) == 0xC0) bytesToKeep = 1; // Start of 2,3,4 byte seq
                if (currentData.size() >= 2 && (static_cast<unsigned char>(currentData[currentData.size() - 2]) & 0xE0) == 0xE0 && (static_cast<unsigned char>(currentData.back()) & 0xC0) == 0x80) bytesToKeep = 2;
                if (currentData.size() >= 3 && (static_cast<unsigned char>(currentData[currentData.size() - 3]) & 0xF0) == 0xF0 && (static_cast<unsigned char>(currentData[currentData.size() - 2]) & 0xC0) == 0x80 && (static_cast<unsigned char>(currentData.back()) & 0xC0) == 0x80) bytesToKeep = 3;

                if (bytesToKeep > 0 && bytesToKeep < currentData.size()) {
                    m_utf8PartialSequence.assign(currentData.end() - bytesToKeep, currentData.end());
                    // Try converting the part before the partial sequence
                    wideCharCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, currentData.data(), static_cast<int>(currentData.size() - bytesToKeep), nullptr, 0);
                    if (wideCharCount > 0) {
                        wideString.resize(wideCharCount);
                        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, currentData.data(), static_cast<int>(currentData.size() - bytesToKeep), &wideString[0], wideCharCount);
                    }
                    else {
                        // Still failed, or nothing to convert before partial
                        OutputDebugStringA("AnsiParser: Failed to convert to wide char or no complete chars.\n");
                        return;
                    }
                }
                else if (bytesToKeep == currentData.size()) {
                    // Entire buffer is potentially a partial sequence
                    m_utf8PartialSequence.assign(currentData.begin(), currentData.end());
                    return; // Wait for more data
                }
                else {
                    OutputDebugStringA("AnsiParser: MultiByteToWideChar failed with ERROR_NO_UNICODE_TRANSLATION.\n");
                    return; // Or substitute with '?'
                }
            }
            else if (error != 0) {
                OutputDebugStringA("AnsiParser: MultiByteToWideChar failed.\n");
                return; // Or substitute with '?'
            }
        }


        for (wchar_t wc : wideString) {
            switch (wc) {
            case L'\n': // Line Feed
                m_terminalBuffer.NewLine();
                break;
            case L'\r': // Carriage Return
                m_terminalBuffer.CarriageReturn();
                break;
            case L'\b': // Backspace
                m_terminalBuffer.Backspace();
                break;
            case L'\t': // Tab
                m_terminalBuffer.Tab();
                break;
            case 0x07: // BEL (Bell)
                // TODO: Play a sound or flash screen
                MessageBeep(MB_OK); // Simple beep for now
                break;
                // TODO: Add cases for ESC (0x1B) to start parsing escape sequences
            default:
                // For now, assume printable if not a control character we handle
                // A more robust check would be iswcntrl or similar
                if (wc >= 32 || wc < 0) { // Basic check for printable (and allow extended chars)
                    m_terminalBuffer.AddChar(wc);
                }
                else {
                    // OutputDebugString((L"Unhandled control char: " + std::to_wstring(static_cast<int>(wc)) + L"\n").c_str());
                }
                break;
            }
        }
    }

}