#pragma once
#include "ITerminalActions.h"
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace winrt::win_retro_term::Core 
{
    enum class AnsiColor : uint8_t {
        Black = 0, 
        Red = 1, 
        Green = 2, 
        Yellow = 3, 
        Blue = 4, 
        Magenta = 5, 
        Cyan = 6, 
        White = 7,
        BrightBlack = 8, 
        BrightRed = 9, 
        BrightGreen = 10, 
        BrightYellow = 11,
        BrightBlue = 12, 
        BrightMagenta = 13, 
        BrightCyan = 14, 
        BrightWhite = 15,
        Foreground = 16,
        Background = 17
    };

    enum class CellAttributesFlags : uint16_t {
        None = 0,
        Bold = 1 << 0,
        Italic = 1 << 1,
        Underline = 1 << 2,
        Inverse = 1 << 3,
        Concealed = 1 << 4,
        Strikethrough = 1 << 5,
        Dim = 1 << 6
    };

    inline CellAttributesFlags operator|(CellAttributesFlags a, CellAttributesFlags b) {
        return static_cast<CellAttributesFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
    }
    inline CellAttributesFlags& operator|=(CellAttributesFlags& a, CellAttributesFlags b) {
        a = a | b;
        return a;
    }
    inline CellAttributesFlags operator&(CellAttributesFlags a, CellAttributesFlags b) {
        return static_cast<CellAttributesFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
    }
    inline CellAttributesFlags operator~(CellAttributesFlags a) {
        return static_cast<CellAttributesFlags>(~static_cast<uint16_t>(a));
    }

    struct Cell {
        wchar_t character = L' ';
        AnsiColor foregroundColor = AnsiColor::Foreground;
        AnsiColor backgroundColor = AnsiColor::Background;
        CellAttributesFlags attributes = CellAttributesFlags::None;
        Cell() = default;
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

        void SetGraphicsRendition(const std::vector<int>& params) override;

        CellAttributesFlags GetCurrentAttributesFlags() const { return m_currentAttributes.attributes; }
        AnsiColor GetCurrentForegroundColor() const { return m_currentAttributes.foregroundColor; }
        AnsiColor GetCurrentBackgroundColor() const { return m_currentAttributes.backgroundColor; }

    private:
        void EnsureCursorInBounds();
        void InitBuffer();

        int m_rows;
        int m_cols;
        std::vector<std::vector<Cell>> m_screenBuffer;
        int m_cursorX;
        int m_cursorY;
        const int TAB_WIDTH = 8;

        Cell m_currentAttributes;
        Cell m_defaultAttributes;
    };
}