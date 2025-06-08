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

    namespace DecPrivateModes {
        const int DECCKM_CursorKeys = 1;                // Application Cursor Keys Mode
        const int DECANM_AnsiVt52Mode = 2;              // ANSI/VT52 mode (VT52 is rare now)
        const int DECCOLM_132ColumnMode = 3;            // 132 Column Mode
        const int DECSCLM_SmoothScroll = 4;             // Smooth Scroll Mode
        const int DECSCNM_ScreenMode = 5;               // Reverse Video Screen Mode
        const int DECOM_OriginMode = 6;                 // Origin Mode (relative vs. absolute cursor addressing)
        const int DECAWM_AutoWrapMode = 7;              // Auto Wrap Mode
        const int DECARM_AutoRepeatMode = 8;            // Auto Repeat Keys Mode
        const int XTERM_SendMouseXYOnClick = 9;         // X10 Mouse Reporting (Press/Release only)
        const int DECTCEM_TextCursorEnable = 25;        // Show/Hide Cursor
        const int DECNKM_KeypadApplication = 66;        // Application Keypad Mode (xterm)
        const int XTERM_AlternateScreenBuffer = 1049;   // Uses alternate screen, saves/restores cursor & screen
        const int XTERM_MouseBtnEvent = 1000;           // Send Mouse X & Y on button press and release.
        const int XTERM_MouseMotionEvent = 1002;        // Send Mouse X & Y on button press, release, and motion.
        const int XTERM_MouseAnyEvent = 1003;           // Send Mouse X & Y on any mouse event (press, release, motion).
        const int XTERM_FocusEvent = 1004;              // Send FocusIn/FocusOut events.
        const int XTERM_SGRMouseMode = 1006;            // Extended SGR mouse reporting.
    }

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

    const wchar_t CHARSET_US_ASCII = L'B';
    const wchar_t CHARSET_DEC_SPECIAL_GRAPHICS = L'0';
    const wchar_t CHARSET_UK = L'A';

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

        void DesignateCharSet(uint8_t targetSet, wchar_t charSetType) override;
        void InvokeCharSet(uint8_t gSetToInvokeIntoGL) override;

        void SetDecPrivateMode(int mode, bool enabled) override;

        bool IsApplicationCursorKeysMode() const { return m_applicationCursorKeysMode; }
        bool IsApplicationKeypadMode() const { return m_applicationKeypadMode; }
        bool IsCursorVisible() const { return m_cursorVisible; }
        bool IsAlternateScreenActive() const { return m_isAlternateScreenActive; }

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

        wchar_t m_charsets[4];
        uint8_t m_glCharsetIndex;
        uint8_t m_grCharsetIndex;

        bool m_applicationCursorKeysMode = false;
        bool m_applicationKeypadMode = false;

        bool m_cursorVisible = true;
        bool m_autoWrapMode = true;
        bool m_originMode = false;

        bool m_isAlternateScreenActive = false;
        std::vector<std::vector<Cell>> m_mainScreenBufferBackup;
        Cell m_mainScreenCursorAttributesBackup;
        int m_mainScreenCursorXBackup = 0;
        int m_mainScreenCursorYBackup = 0;

        wchar_t MapCharacter(wchar_t ch);
    };
}