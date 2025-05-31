#pragma once
#include "ITerminalActions.h"
#include <string>
#include <vector>
#include <algorithm>

namespace winrt::win_retro_term::Core 
{
    struct Cell {
        wchar_t character = L' ';
        // Future: Add attributes like color, bold, etc.
    };

    class TerminalBuffer : public ITerminalActions {
    public:
        TerminalBuffer(int rows, int cols);

        void SetChar(int r, int c, wchar_t ch);
        Cell GetCell(int r, int c) const;
        const std::vector<std::vector<Cell>>& GetScreenBuffer() const;

        void Clear();
        void Resize(int newRows, int newCols);
        void ScrollUp(int linesToScroll = 1);
        void SetCursorPosition(int r, int c);

        int GetCursorRow() const { return m_cursorY; }
        int GetCursorCol() const { return m_cursorX; }
        int GetRows() const { return m_rows; }
        int GetCols() const { return m_cols; }

        // --- ITerminalActions Implementation ---
        void PrintChar(wchar_t ch) override;
        void ExecuteControlFunction(wchar_t control) override;

        void LineFeed() override;
        void CarriageReturn() override;
        void Backspace() override;
        void HorizontalTab() override;
        void Bell() override;

        void CursorUp(int count) override;
        void CursorDown(int count) override;
        void CursorForward(int count) override;
        void CursorBack(int count) override;
        void CursorPosition(int row, int col) override;

        void EraseInDisplay(int mode) override;
        void EraseInLine(int mode) override;

    private:
        void EnsureCursorInBounds();
        void InitBuffer();

        int m_rows;
        int m_cols;

        std::vector<std::vector<Cell>> m_screenBuffer;

        int m_cursorX;
        int m_cursorY;

        const int TAB_WIDTH = 8;
    };
}