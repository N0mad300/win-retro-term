#pragma once
#include <string>
#include <vector>
#include <algorithm> // For std::fill, std::min, std::max

namespace winrt::win_retro_term::Core 
{
    struct Cell {
        wchar_t character = L' ';
        // Future: Add attributes like color, bold, etc.
    };

    class TerminalBuffer {
    public:
        TerminalBuffer(int rows, int cols);

        void SetChar(int r, int c, wchar_t ch);
        Cell GetCell(int r, int c) const;
        const std::vector<std::vector<Cell>>& GetScreenBuffer() const;

        void Clear();
        void ScrollUp(int linesToScroll = 1);

        void SetCursorPosition(int r, int c);
        int GetCursorRow() const { return m_cursorY; }
        int GetCursorCol() const { return m_cursorX; }

        int GetRows() const { return m_rows; }
        int GetCols() const { return m_cols; }

        // Basic character insertion and control codes
        void AddChar(wchar_t ch);
        void NewLine();
        void CarriageReturn();
        void Backspace();
        void Tab(); // Basic tab for now

        // Called when the PTY output indicates a resize
        void Resize(int newRows, int newCols);


    private:
        void EnsureCursorInBounds();
        void InitBuffer();


        int m_rows;
        int m_cols;
        std::vector<std::vector<Cell>> m_screenBuffer; // The main grid

        int m_cursorX;
        int m_cursorY;

        const int TAB_WIDTH = 8; // Typical tab width
    };
}