#pragma once

namespace winrt::win_retro_term::Core 
{
    class ITerminalActions {
    public:
        virtual ~ITerminalActions() = default;

        // Called for printable characters when in GROUND state
        virtual void PrintChar(wchar_t ch) = 0;

        // Called for C0 control characters (other than ones with specific handlers like LF, CR, BS, HT)
        // or C1 control characters if 7-bit mapping is used (e.g. ESC Fe)
        virtual void ExecuteControlFunction(wchar_t control) = 0;

        // Specific C0 handlers
        virtual void LineFeed() = 0;         // LF, \n
        virtual void CarriageReturn() = 0;   // CR, \r
        virtual void Backspace() = 0;        // BS, \b
        virtual void HorizontalTab() = 0;    // HT, \t
        virtual void Bell() = 0;             // BEL, \a

        virtual void CursorUp(int count) = 0;           // CUU: CSI Pn A
        virtual void CursorDown(int count) = 0;         // CUD: CSI Pn B
        virtual void CursorForward(int count) = 0;      // CUF: CSI Pn C
        virtual void CursorBack(int count) = 0;         // CUB: CSI Pn D
        virtual void CursorPosition(int row, int col) = 0; // CUP: CSI Pn ; Pn H (or f)

        virtual void EraseInDisplay(int mode) = 0;      // ED:  CSI Ps J
        virtual void EraseInLine(int mode) = 0;         // EL:  CSI Ps K

        virtual void SetGraphicsRendition(const std::vector<int>& params) = 0; // SGR

        virtual void DesignateCharSet(uint8_t targetSet, wchar_t charSet) = 0;
        virtual void InvokeCharSet(uint8_t gSetToInvokeIntoGL) = 0;

        virtual void SetDecPrivateMode(int mode, bool enabled) = 0;
    };

}