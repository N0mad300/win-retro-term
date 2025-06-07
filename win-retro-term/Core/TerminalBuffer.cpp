#include "pch.h"
#include "TerminalBuffer.h"
#include <stdexcept>

namespace winrt::win_retro_term::Core 
{
    const std::map<wchar_t, wchar_t> decSpecialGraphicsMap = {
        {L'`', L'\u25C6'}, // Diamond
        {L'a', L'\u2592'}, // Checker board (stipple)
        {L'b', L'\u2409'}, // HT symbol
        {L'c', L'\u240C'}, // FF symbol
        {L'd', L'\u240D'}, // CR symbol
        {L'e', L'\u240A'}, // LF symbol
        {L'f', L'\u00B0'}, // Degree Symbol
        {L'g', L'\u00B1'}, // Plus/Minus Symbol
        {L'h', L'\u2424'}, // NL symbol
        {L'i', L'\u240B'}, // VT symbol
        {L'j', L'\u2518'}, // Lower Right Corner
        {L'k', L'\u2510'}, // Upper Right Corner
        {L'l', L'\u250C'}, // Upper Left Corner
        {L'm', L'\u2514'}, // Lower Left Corner
        {L'n', L'\u253C'}, // Crossing Lines (plus)
        {L'o', L'\u23BA'}, // Scan Line 1 (horizontal line - top)
        {L'p', L'\u23BB'}, // Scan Line 3
        {L'q', L'\u2500'}, // Scan Line 5 (horizontal line - middle)
        {L'r', L'\u23BC'}, // Scan Line 7
        {L's', L'\u23BD'}, // Scan Line 9 (horizontal line - bottom)
        {L't', L'\u251C'}, // Left Tee
        {L'u', L'\u2524'}, // Right Tee
        {L'v', L'\u2534'}, // Bottom Tee
        {L'w', L'\u252C'}, // Top Tee
        {L'x', L'\u2502'}, // Vertical Line
        {L'y', L'\u2264'}, // Less Than Or Equal To
        {L'z', L'\u2265'}, // Greater Than Or Equal To
        {L'{', L'\u03C0'}, // Pi
        {L'|', L'\u2260'}, // Not Equal To
        {L'}', L'\u00A3'}, // UK Pound Sterling
        {L'~', L'\u00B7'}  // Centered Dot (bullet)
    };

    TerminalBuffer::TerminalBuffer(int rows, int cols) : m_rows(rows), m_cols(cols), m_cursorX(0), m_cursorY(0)
    {
        m_defaultAttributes.foregroundColor = AnsiColor::Foreground;
        m_defaultAttributes.backgroundColor = AnsiColor::Background;
        m_defaultAttributes.attributes = CellAttributesFlags::None;
        m_currentAttributes = m_defaultAttributes;

        m_charsets[0] = CHARSET_US_ASCII;
        m_charsets[1] = CHARSET_US_ASCII;
        m_charsets[2] = CHARSET_US_ASCII;
        m_charsets[3] = CHARSET_US_ASCII;
        m_glCharsetIndex = 0;
        m_grCharsetIndex = 1;

        InitBuffer();
    }

    void TerminalBuffer::InitBuffer() {
        m_screenBuffer.assign(m_rows, std::vector<Cell>(m_cols));
        
        Cell defaultCell;
        defaultCell.foregroundColor = m_defaultAttributes.foregroundColor;
        defaultCell.backgroundColor = m_defaultAttributes.backgroundColor;
        defaultCell.attributes = m_defaultAttributes.attributes;

        for (int r = 0; r < m_rows; ++r) {
            for (int c = 0; c < m_cols; ++c) {
                m_screenBuffer[r][c] = defaultCell;
            }
        }
    }

    void TerminalBuffer::Resize(int newRows, int newCols) {
        // Naive resize: create a new buffer and copy what fits.
        // More sophisticated resize would try to preserve scrollback and content.
        std::vector<std::vector<Cell>> newBuffer(newRows, std::vector<Cell>(newCols));

        for (int r = 0; r < std::min(m_rows, newRows); ++r) {
            for (int c = 0; c < std::min(m_cols, newCols); ++c) {
                if (r < m_screenBuffer.size() && c < m_screenBuffer[r].size()) {
                    newBuffer[r][c] = m_screenBuffer[r][c];
                }
            }
        }

        m_screenBuffer = std::move(newBuffer);
        m_rows = newRows;
        m_cols = newCols;

        EnsureCursorInBounds(); // Make sure cursor is still valid
    }


    void TerminalBuffer::SetChar(int r, int c, wchar_t ch) {
        if (r >= 0 && r < m_rows && c >= 0 && c < m_cols) {
            m_screenBuffer[r][c].character = ch;
        }
    }

    Cell TerminalBuffer::GetCell(int r, int c) const {
        if (r >= 0 && r < m_rows && c >= 0 && c < m_cols) {
            return m_screenBuffer[r][c];
        }
        // Consider throwing or returning a default 'empty' cell
        // For rendering, it's often better to handle out-of-bounds gracefully
        return Cell{}; // Default character ' '
    }

    const std::vector<std::vector<Cell>>& TerminalBuffer::GetScreenBuffer() const {
        return m_screenBuffer;
    }

    void TerminalBuffer::Clear() {
        for (int r = 0; r < m_rows; ++r) {
            for (int c = 0; c < m_cols; ++c) {
                m_screenBuffer[r][c].character = L' ';
                m_screenBuffer[r][c].foregroundColor = m_defaultAttributes.foregroundColor;
                m_screenBuffer[r][c].backgroundColor = m_defaultAttributes.backgroundColor;
                m_screenBuffer[r][c].attributes = m_defaultAttributes.attributes;
            }
        }
        m_cursorX = 0;
        m_cursorY = 0;
    }

    void TerminalBuffer::ScrollUp(int linesToScroll) {
        if (linesToScroll <= 0) return;
        if (linesToScroll >= m_rows) {
            Clear(); // Scroll everything off screen
            return;
        }

        // Move lines up
        for (int r = 0; r < m_rows - linesToScroll; ++r) {
            m_screenBuffer[r] = m_screenBuffer[r + linesToScroll];
        }

        Cell defaultCellWithSpace = m_defaultAttributes;
        defaultCellWithSpace.character = L' ';

        // Clear the new lines at the bottom
        for (int r = m_rows - linesToScroll; r < m_rows; ++r) {
            for (int c = 0; c < m_cols; ++c) {
                m_screenBuffer[r][c] = defaultCellWithSpace;
            }
        }
    }

    void TerminalBuffer::SetCursorPosition(int r, int c) {
        m_cursorY = std::max(0, std::min(r, m_rows - 1));
        m_cursorX = std::max(0, std::min(c, m_cols - 1));
    }

    void TerminalBuffer::EnsureCursorInBounds() {
        m_cursorY = std::max(0, std::min(m_cursorY, m_rows - 1));
        m_cursorX = std::max(0, std::min(m_cursorX, m_cols - 1));
    }


    void TerminalBuffer::PrintChar(wchar_t ch) {
        if (m_cursorY >= m_rows) {
            m_cursorY = m_rows - 1;
            ScrollUp();
        }
        if (m_cursorX >= m_cols) {
            CarriageReturn();
            LineFeed();
        }

        wchar_t mappedChar = MapCharacter(ch);

        if (m_cursorY < m_rows && m_cursorX < m_cols) {
            m_screenBuffer[m_cursorY][m_cursorX].character = mappedChar;
            m_screenBuffer[m_cursorY][m_cursorX].foregroundColor = m_currentAttributes.foregroundColor;
            m_screenBuffer[m_cursorY][m_cursorX].backgroundColor = m_currentAttributes.backgroundColor;
            m_screenBuffer[m_cursorY][m_cursorX].attributes = m_currentAttributes.attributes;
            m_cursorX++;
        }
    }

    wchar_t TerminalBuffer::MapCharacter(wchar_t ch) 
    {
        if (ch < 0x20 || ch == 0x7F) {
            return ch;
        }

        wchar_t activeCharset = m_charsets[m_glCharsetIndex];

        if (activeCharset == CHARSET_DEC_SPECIAL_GRAPHICS) {
            if (ch == L'_') return L' ';

            auto it = decSpecialGraphicsMap.find(ch);
            if (it != decSpecialGraphicsMap.end()) {
                return it->second;
            }
            return ch;
        }
        else if (activeCharset == CHARSET_UK) {
            if (ch == L'#') return L'\u00A3';
            return ch;
        }

        return ch;
    }


    void TerminalBuffer::DesignateCharSet(uint8_t targetSet, wchar_t charSetType) {
        if (targetSet > 3) return;

        m_charsets[targetSet] = charSetType;

        OutputDebugString((L"TerminalBuffer: Designated G" + std::to_wstring(targetSet) + L" as type '" + std::wstring(1, charSetType) + L"'\n").c_str());
    }


    void TerminalBuffer::InvokeCharSet(uint8_t gSetToInvokeIntoGL) {
        if (gSetToInvokeIntoGL > 3) return;

        m_glCharsetIndex = gSetToInvokeIntoGL;
        OutputDebugString((L"TerminalBuffer: Invoked G" + std::to_wstring(gSetToInvokeIntoGL) + L" (type '" + std::wstring(1,m_charsets[gSetToInvokeIntoGL]) + L"') into GL\n").c_str());
    }

    void TerminalBuffer::ExecuteControlFunction(wchar_t control) 
    {
        if (control == 0x0E) { // SO - Shift Out, invoke G1 into GL
            InvokeCharSet(1);
        }
        else if (control == 0x0F) { // SI - Shift In, invoke G0 into GL
            InvokeCharSet(0);
        }
        else {
            OutputDebugString((L"TerminalBuffer: Unhandled ExecuteControlFunction: 0x" + std::to_wstring(static_cast<int>(control)) + L"\n").c_str());
        }
    }

    void TerminalBuffer::LineFeed() { // LF, \n
        m_cursorY++;
        if (m_cursorY >= m_rows) {
            ScrollUp();
            m_cursorY = m_rows - 1;
        }
    }

    void TerminalBuffer::CarriageReturn() { // CR, \r
        m_cursorX = 0;
    }

    void TerminalBuffer::Backspace() { // BS, \b
        if (m_cursorX > 0) {
            m_cursorX--;
            m_screenBuffer[m_cursorY][m_cursorX].character = L' ';
        }
        else if (m_cursorY > 0) {
            m_cursorY--;
            m_screenBuffer[m_cursorY][m_cursorX].character = L' ';
        }
    }

    void TerminalBuffer::HorizontalTab() { // HT, \t
        int nextTabStop = (m_cursorX / TAB_WIDTH + 1) * TAB_WIDTH;
        if (nextTabStop >= m_cols) {
            m_cursorX = m_cols - 1;
        }
        else {
            m_cursorX = nextTabStop;
        }
    }

    void TerminalBuffer::Bell() { // BEL, \a
        MessageBeep(MB_OK);
    }

    void TerminalBuffer::CursorUp(int count) {
        m_cursorY = std::max(0, m_cursorY - count);
        EnsureCursorInBounds();
    }

    void TerminalBuffer::CursorDown(int count) {
        m_cursorY = std::min(m_rows - 1, m_cursorY + count);
        EnsureCursorInBounds();
    }

    void TerminalBuffer::CursorForward(int count) {
        m_cursorX = std::min(m_cols - 1, m_cursorX + count);
        EnsureCursorInBounds();
    }

    void TerminalBuffer::CursorBack(int count) {
        m_cursorX = std::max(0, m_cursorX - count);
        EnsureCursorInBounds();
    }

    void TerminalBuffer::CursorPosition(int row, int col) {
        m_cursorY = std::max(0, std::min(row - 1, m_rows - 1));
        m_cursorX = std::max(0, std::min(col - 1, m_cols - 1));
    }

    void TerminalBuffer::EraseInDisplay(int mode) {
        // ED: CSI Ps J
        // Ps = 0: Erase from cursor to end of screen (inclusive of cursor position).
        // Ps = 1: Erase from beginning of screen to cursor (inclusive).
        // Ps = 2: Erase entire screen (cursor position does not change).
        // Ps = 3: Erase entire screen + scrollback buffer (DEC specific, Windows Terminal supports). For now, treat as 2.

        Cell defaultCellWithSpace = m_defaultAttributes;
        defaultCellWithSpace.character = L' ';

        switch (mode) {
        case 0: // From cursor to end
            for (int c = m_cursorX; c < m_cols; ++c) m_screenBuffer[m_cursorY][c] = defaultCellWithSpace;
            for (int r = m_cursorY + 1; r < m_rows; ++r) 
            {
                for (int c = 0; c < m_cols; ++c) m_screenBuffer[r][c] = defaultCellWithSpace;
            }
            break;
        case 1: // From beginning to cursor
            for (int r = 0; r < m_cursorY; ++r) 
            {
                for (int c = 0; c < m_cols; ++c) m_screenBuffer[r][c] = defaultCellWithSpace;
            }
            for (int c = 0; c <= m_cursorX; ++c) m_screenBuffer[m_cursorY][c] = defaultCellWithSpace;
            break;
        case 2: // Erase entire screen
        case 3: // Erase entire screen + scrollback (treat as 2 for now)
            for (int r = 0; r < m_rows; ++r) 
            {
                for (int c = 0; c < m_cols; ++c) m_screenBuffer[r][c] = defaultCellWithSpace;
            }
            // Cursor position does NOT change for ED with Ps=2 or Ps=3
            break;
        default:
            // Unknown mode, ignore
            break;
        }
    }

    void TerminalBuffer::EraseInLine(int mode) {
        // EL: CSI Ps K
        // Ps = 0: Erase from cursor to end of line (inclusive).
        // Ps = 1: Erase from beginning of line to cursor (inclusive).
        // Ps = 2: Erase entire line (cursor position does not change).
        if (m_cursorY < 0 || m_cursorY >= m_rows) return;

        Cell defaultCellWithSpace = m_defaultAttributes;
        defaultCellWithSpace.character = L' ';

        switch (mode) {
        case 0: // From cursor to end of line
            for (int c = m_cursorX; c < m_cols; ++c) {
                m_screenBuffer[m_cursorY][c] = defaultCellWithSpace;
            }
            break;
        case 1: // From beginning of line to cursor
            for (int c = 0; c <= m_cursorX; ++c) {
                if (c < m_cols) { // Boundary check
                    m_screenBuffer[m_cursorY][c] = defaultCellWithSpace;
                }
            }
            break;
        case 2: // Erase entire line
            for (int c = 0; c < m_cols; ++c) {
                m_screenBuffer[m_cursorY][c] = defaultCellWithSpace;
            }
            break;
        default:
            // Unknown mode, ignore
            break;
        }
    }

    void TerminalBuffer::SetGraphicsRendition(const std::vector<int>& params) {
        if (params.empty()) {
            m_currentAttributes = m_defaultAttributes;
            return;
        }

        for (size_t i = 0; i < params.size(); ++i) {
            int p = params[i];
            if (p == 0) { // Reset / Normal
                m_currentAttributes = m_defaultAttributes;
            }
            else if (p == 1) { // Bold or increased intensity
                m_currentAttributes.attributes |= CellAttributesFlags::Bold;
            }
            else if (p == 2) { // Dim
                m_currentAttributes.attributes |= CellAttributesFlags::Dim;
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Bold; // Faint typically overrides Bold
            }
            else if (p == 3) { // Italic
                m_currentAttributes.attributes |= CellAttributesFlags::Italic;
            }
            else if (p == 4) { // Underline
                m_currentAttributes.attributes |= CellAttributesFlags::Underline;
            }
            else if (p == 7) { // Inverse video
                m_currentAttributes.attributes |= CellAttributesFlags::Inverse;
            }
            else if (p == 8) { // Concealed (not visible)
                m_currentAttributes.attributes |= CellAttributesFlags::Concealed;
            }
            else if (p == 9) { // Strikethrough / crossed-out
                m_currentAttributes.attributes |= CellAttributesFlags::Strikethrough;
            }
            else if (p == 21) { // Doubly underlined (treat as single underline for now)
                m_currentAttributes.attributes |= CellAttributesFlags::Underline;
            }
            else if (p == 22) { // Normal intensity (neither bold nor dim)
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Bold;
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Dim;
            }
            else if (p == 23) { // Not italic
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Italic;
            }
            else if (p == 24) { // Not underlined
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Underline;
            }
            else if (p == 27) { // Not inverse
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Inverse;
            }
            else if (p == 28) { // Not concealed
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Concealed;
            }
            else if (p == 29) { // Not strikethrough
                m_currentAttributes.attributes = m_currentAttributes.attributes & ~CellAttributesFlags::Strikethrough;
            }
            // Basic 3/4-bit ANSI Colors
            else if (p >= 30 && p <= 37) { // Set foreground color
                m_currentAttributes.foregroundColor = static_cast<AnsiColor>(p - 30);
            }
            else if (p == 39) { // Default foreground color
                m_currentAttributes.foregroundColor = m_defaultAttributes.foregroundColor;
            }
            else if (p >= 40 && p <= 47) { // Set background color
                m_currentAttributes.backgroundColor = static_cast<AnsiColor>(p - 40);
            }
            else if (p == 49) { // Default background color
                m_currentAttributes.backgroundColor = m_defaultAttributes.backgroundColor;
            }
            // Bright 3/4-bit ANSI Colors
            else if (p >= 90 && p <= 97) { // Set bright foreground color
                m_currentAttributes.foregroundColor = static_cast<AnsiColor>((p - 90) + 8); // Offset by 8 for bright
            }
            else if (p >= 100 && p <= 107) { // Set bright background color
                m_currentAttributes.backgroundColor = static_cast<AnsiColor>((p - 100) + 8); // Offset by 8 for bright
            }
            // 8-bit (256) colors: 38 ; 5 ; P s or 48 ; 5 ; P s
            else if (p == 38 || p == 48) {
                if (i + 2 < params.size()) {
                    int colorMode = params[i + 1];
                    int colorValue = params[i + 2];
                    if (colorMode == 5) { // 256-color palette
                        if (colorValue >= 0 && colorValue <= 255) {
                            AnsiColor mappedColor = AnsiColor::White;
                            if (colorValue < 8) mappedColor = static_cast<AnsiColor>(colorValue);
                            else if (colorValue < 16) mappedColor = static_cast<AnsiColor>(static_cast<uint8_t>(colorValue - 8) + static_cast<uint8_t>(AnsiColor::BrightBlack));

                            if (p == 38) m_currentAttributes.foregroundColor = mappedColor;
                            else m_currentAttributes.backgroundColor = mappedColor;
                        }
                    }
                    else if (colorMode == 2) {
                        if (i + 4 < params.size()) {
                            // TODO: Store RGB directly in Cell and signal renderer to use it.
                            i += 2;
                        }
                    }
                    i += 2;
                }
            }
        }
    }
}