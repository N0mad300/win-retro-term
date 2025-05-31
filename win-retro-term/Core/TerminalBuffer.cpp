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


    void TerminalBuffer::AddChar(wchar_t ch) {
        if (m_cursorY >= m_rows) { // Should not happen if NewLine is managed correctly
            m_cursorY = m_rows - 1;
            ScrollUp();
        }
        if (m_cursorX >= m_cols) {
            CarriageReturn();
            NewLine(); // Wrap to next line
        }

        if (m_cursorY < m_rows && m_cursorX < m_cols) {
            m_screenBuffer[m_cursorY][m_cursorX].character = ch;
            m_cursorX++;
            if (m_cursorX >= m_cols) { // If character filled the last column
                // Don't automatically wrap here; let next char or newline handle it.
                // Many terminals have a "deferred wrap" or "auto wrap" mode.
                // For now, we'll just let it sit at m_cols, and the next AddChar will wrap.
            }
        }
    }

    void TerminalBuffer::NewLine() {
        m_cursorY++;
        // m_cursorX remains unchanged (standard VT100 behavior for LF without CR)
        if (m_cursorY >= m_rows) {
            ScrollUp();
            m_cursorY = m_rows - 1; // Stay on the last line
        }
    }

    void TerminalBuffer::CarriageReturn() {
        m_cursorX = 0;
    }

    void TerminalBuffer::Backspace() {
        if (m_cursorX > 0) {
            m_cursorX--;
            // Optional: Erase character at new m_cursorX
            // m_screenBuffer[m_cursorY][m_cursorX].character = L' ';
        }
        // else if (m_cursorY > 0) { // More complex: wrap to previous line
        //    m_cursorY--;
        //    m_cursorX = m_cols -1;
        //    m_screenBuffer[m_cursorY][m_cursorX].character = L' ';
        // }
    }

    void TerminalBuffer::Tab() {
        int nextTabStop = (m_cursorX / TAB_WIDTH + 1) * TAB_WIDTH;
        if (nextTabStop >= m_cols) {
            m_cursorX = m_cols - 1; // Go to last column or wrap
            // If you want to wrap on tab past end of line:
            // CarriageReturn();
            // NewLine();
        }
        else {
            m_cursorX = nextTabStop;
        }
    }
}