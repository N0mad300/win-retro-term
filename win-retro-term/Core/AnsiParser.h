#pragma once
#include "TerminalBuffer.h"
#include <string>
#include <vector>

namespace winrt::win_retro_term::Core 
{
    class AnsiParser {
    public:
        AnsiParser(TerminalBuffer& buffer);

        void Parse(const char* data, size_t length);

    private:
        TerminalBuffer& m_terminalBuffer;
        std::vector<char> m_utf8PartialSequence; // For handling partial UTF-8 sequences
    };

}