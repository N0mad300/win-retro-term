#include "pch.h"
#include "TerminalBuffer.h"
#include <stdexcept>

namespace winrt::win_retro_term::Core 
{
    TerminalBuffer::TerminalBuffer(int rows, int cols)
        : m_rows(rows), m_cols(cols), m_cursorX(0), m_cursorY(0) {
        InitBuffer();
    }

    void TerminalBuffer::InitBuffer() {
        m_screenBuffer.assign(m_rows, std::vector<Cell>(m_cols));
        // Initialize cells if needed (default Cell constructor does L' ')
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
        // Clear the new lines at the bottom
        for (int r = m_rows - linesToScroll; r < m_rows; ++r) {
            for (int c = 0; c < m_cols; ++c) {
                m_screenBuffer[r][c].character = L' ';
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

        if (m_cursorY < m_rows && m_cursorX < m_cols) {
            m_screenBuffer[m_cursorY][m_cursorX].character = ch;
            m_cursorX++;
        }
    }

    void TerminalBuffer::ExecuteControlFunction(wchar_t control) 
    {
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
            m_cursorX = m_cols -1;
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

        switch (mode) {
        case 0: // From cursor to end
            // Erase current line from cursor to end
            for (int c = m_cursorX; c < m_cols; ++c) {
                m_screenBuffer[m_cursorY][c].character = L' ';
                // TODO: Reset attributes
            }
            // Erase subsequent lines
            for (int r = m_cursorY + 1; r < m_rows; ++r) {
                for (int c = 0; c < m_cols; ++c) {
                    m_screenBuffer[r][c].character = L' ';
                    // TODO: Reset attributes
                }
            }
            break;
        case 1: // From beginning to cursor
            // Erase previous lines
            for (int r = 0; r < m_cursorY; ++r) {
                for (int c = 0; c < m_cols; ++c) {
                    m_screenBuffer[r][c].character = L' ';
                    // TODO: Reset attributes
                }
            }
            // Erase current line from beginning to cursor
            for (int c = 0; c <= m_cursorX; ++c) {
                m_screenBuffer[m_cursorY][c].character = L' ';
                // TODO: Reset attributes
            }
            break;
        case 2: // Erase entire screen
        case 3: // Erase entire screen + scrollback (treat as 2 for now)
            for (int r = 0; r < m_rows; ++r) {
                for (int c = 0; c < m_cols; ++c) {
                    m_screenBuffer[r][c].character = L' ';
                    // TODO: Reset attributes
                }
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

        switch (mode) {
        case 0: // From cursor to end of line
            for (int c = m_cursorX; c < m_cols; ++c) {
                m_screenBuffer[m_cursorY][c].character = L' ';
                // TODO: Reset attributes
            }
            break;
        case 1: // From beginning of line to cursor
            for (int c = 0; c <= m_cursorX; ++c) {
                if (c < m_cols) { // Boundary check
                    m_screenBuffer[m_cursorY][c].character = L' ';
                    // TODO: Reset attributes
                }
            }
            break;
        case 2: // Erase entire line
            for (int c = 0; c < m_cols; ++c) {
                m_screenBuffer[m_cursorY][c].character = L' ';
                // TODO: Reset attributes
            }
            break;
        default:
            // Unknown mode, ignore
            break;
        }
    }
}