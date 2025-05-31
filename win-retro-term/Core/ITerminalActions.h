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
    };

}