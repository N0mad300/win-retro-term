#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <processthreadsapi.h>
#include <consoleapi.h>

namespace winrt::win_retro_term::Core { class ConPtyProcess; }

class ConPtyProcess {
public:
    // Callback function type for when data is received from the PTY
    // Parameters: buffer pointer, buffer size
    using DataReceivedCallback = std::function<void(const char*, size_t)>;

    ConPtyProcess();
    ~ConPtyProcess();

    // Starts the PTY and the specified command line process
    // commandLine: e.g., L"cmd.exe" or L"powershell.exe"
    // size: Initial dimensions of the PTY
    // callback: Function to call when data is received from the PTY
    bool Start(const std::wstring& commandLine, COORD size, DataReceivedCallback callback);

    // Writes data to the PTY's input.
    bool WriteInput(const std::string& data);

    // Resizes the PTY.
    bool Resize(COORD newSize);

    // Stops the PTY and terminates the client process.
    void Stop();

    bool IsRunning() const { return m_running; }

private:
    void OutputThreadFunc();
    void CloseAllHandles();

    HPCON m_hPC = nullptr;              // Handle to the Pseudo Console

    HANDLE m_hInputPipeOurRead = nullptr;  // Read end for PTY output
    HANDLE m_hInputPipePtyWrite = nullptr; // PTY's write end for its output

    HANDLE m_hOutputPipeOurWrite = nullptr; // Write end for PTY input
    HANDLE m_hOutputPipePtyRead = nullptr;  // PTY's read end for its input

    PROCESS_INFORMATION m_piClient = { 0 }; // Information about the client process

    std::thread m_outputThread;
    std::atomic<bool> m_running = false;

    DataReceivedCallback m_onDataReceivedCallback;
};