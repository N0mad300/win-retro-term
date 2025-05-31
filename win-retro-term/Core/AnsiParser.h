#pragma once
#include "ITerminalActions.h"
#include <string>
#include <vector>
#include <limits>

namespace winrt::win_retro_term::Core 
{
    enum class ParserState {
        GROUND,

        ESCAPE,
        ESCAPE_INTERMEDIATE,

        CSI_ENTRY,
        CSI_PARAM,
        CSI_INTERMEDIATE,

        OSC_STRING,

        DCS_ENTRY
    };

    class AnsiParser {
    public:
        AnsiParser(ITerminalActions& actions);

        void Parse(const char* data, size_t length);

    private:
        void ProcessChar(wchar_t ch);
        void ClearSequenceState();

        void ParamDigit(wchar_t digit);
        void ParamSeparator();
        void CollectIntermediate(wchar_t ch);
        int GetParam(size_t index, int defaultValue) const;

        void DispatchCsi(wchar_t finalChar);

        ITerminalActions& m_terminalActions;
        ParserState m_currentState;

        std::vector<char> m_utf8PartialSequence;
        std::vector<int> m_params;
        std::wstring m_intermediates;

        static const int MAX_PARAMS = 16;
    };

}