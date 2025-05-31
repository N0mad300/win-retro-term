#pragma once
#include "ITerminalActions.h"
#include <string>
#include <vector>

namespace winrt::win_retro_term::Core 
{
    enum class ParserState {
        GROUND,
        ESCAPE
    };

    class AnsiParser {
    public:
        AnsiParser(ITerminalActions& actions);

        void Parse(const char* data, size_t length);

    private:
        void ProcessChar(wchar_t ch);
        void ClearSequenceState();

        ITerminalActions& m_terminalActions;
        ParserState m_currentState;
        std::vector<char> m_utf8PartialSequence;
    };

}