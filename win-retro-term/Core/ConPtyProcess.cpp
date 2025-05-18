#include "pch.h" // Or your precompiled header
#include "ConPtyProcess.h"
#include <cassert>
#include <iostream>

// Helper to convert HRESULT to a more usable error message
std::string HResultToString(HRESULT hr) {
    if (FACILITY_WINDOWS == HRESULT_FACILITY(hr))
        hr = HRESULT_CODE(hr);
    TCHAR* szErrMsg;

    if (FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&szErrMsg, 0, NULL) != 0)
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, szErrMsg, -1, NULL, 0, NULL, NULL);
        std::string errMsg(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, szErrMsg, -1, &errMsg[0], size_needed, NULL, NULL);

        LocalFree(szErrMsg);
        return errMsg;
    }
    else
    {
        return "Unknown error (FormatMessage failed)";
    }
}


ConPtyProcess::ConPtyProcess() {
    m_piClient.hProcess = INVALID_HANDLE_VALUE;
    m_piClient.hThread = INVALID_HANDLE_VALUE;
}

ConPtyProcess::~ConPtyProcess() {
    Stop();
}

void ConPtyProcess::CloseAllHandles() {
    if (m_hOutputPipeOurWrite != INVALID_HANDLE_VALUE && m_hOutputPipeOurWrite != nullptr) {
        CloseHandle(m_hOutputPipeOurWrite);
        m_hOutputPipeOurWrite = nullptr;
    }
    if (m_hInputPipeOurRead != INVALID_HANDLE_VALUE && m_hInputPipeOurRead != nullptr) {
        CloseHandle(m_hInputPipeOurRead);
        m_hInputPipeOurRead = nullptr;
    }

    // These are the PTY-side handles. They should have been closed
    // immediately after CreateProcess or if CreatePseudoConsole failed.
    // Closing them here is a safeguard.
    if (m_hInputPipePtyWrite != INVALID_HANDLE_VALUE && m_hInputPipePtyWrite != nullptr) {
        CloseHandle(m_hInputPipePtyWrite);
        m_hInputPipePtyWrite = nullptr;
    }
    if (m_hOutputPipePtyRead != INVALID_HANDLE_VALUE && m_hOutputPipePtyRead != nullptr) {
        CloseHandle(m_hOutputPipePtyRead);
        m_hOutputPipePtyRead = nullptr;
    }

    // Clean up process information
    if (m_piClient.hProcess != INVALID_HANDLE_VALUE && m_piClient.hProcess != nullptr) {
        CloseHandle(m_piClient.hProcess);
        m_piClient.hProcess = INVALID_HANDLE_VALUE;
    }
    if (m_piClient.hThread != INVALID_HANDLE_VALUE && m_piClient.hThread != nullptr) {
        CloseHandle(m_piClient.hThread);
        m_piClient.hThread = INVALID_HANDLE_VALUE;
    }

    // Close the Pseudo Console
    if (m_hPC != nullptr) { // HPCON is not a standard HANDLE so check against nullptr
        ClosePseudoConsole(m_hPC);
        m_hPC = nullptr;
    }
}

bool ConPtyProcess::Start(const std::wstring& commandLine, COORD size, DataReceivedCallback callback) {
    if (m_running) {
        std::cerr << "ConPtyProcess already running." << std::endl;
        return false;
    }

    m_onDataReceivedCallback = callback;
    HRESULT hr = S_OK;

    // 1. Create Pipes for PTY communication
    //    - Input pipe: Data written by us, read by PTY's client
    //    - Output pipe: Data written by PTY's client, read by us
    if (!CreatePipe(&m_hOutputPipePtyRead, &m_hOutputPipeOurWrite, nullptr, 0) ||
        !CreatePipe(&m_hInputPipeOurRead, &m_hInputPipePtyWrite, nullptr, 0)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::cerr << "CreatePipe failed: " << HResultToString(hr) << std::endl;
        CloseAllHandles();
        return false;
    }

    // 2. Create the Pseudo Console (ConPTY)
    //    Pass the PTY-side of the pipes.
    hr = CreatePseudoConsole(size, m_hOutputPipePtyRead, m_hInputPipePtyWrite, 0, &m_hPC);
    if (FAILED(hr)) {
        std::cerr << "CreatePseudoConsole failed: " << HResultToString(hr) << std::endl;
        CloseAllHandles(); // This will close all 4 pipe handles
        return false;
    }

    // We've given the PTY-side pipe handles to CreatePseudoConsole.
    // We should close our handles to the PTY-side of the pipes
    // *before* CreateProcess, so that the PTY process is the sole owner of its end.
    if (m_hOutputPipePtyRead != INVALID_HANDLE_VALUE) { CloseHandle(m_hOutputPipePtyRead); m_hOutputPipePtyRead = nullptr; }
    if (m_hInputPipePtyWrite != INVALID_HANDLE_VALUE) { CloseHandle(m_hInputPipePtyWrite); m_hInputPipePtyWrite = nullptr; }


    // 3. Prepare Startup Information for the client process
    STARTUPINFOEXW siEx = { 0 };
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

    SIZE_T attributeListSize = 0;
    // Get the required size for the attribute list.
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);

    // Allocate memory for the attribute list.
    siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attributeListSize);
    if (!siEx.lpAttributeList) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::cerr << "HeapAlloc for attribute list failed: " << HResultToString(hr) << std::endl;
        CloseAllHandles();
        return false;
    }

    // Initialize the attribute list.
    if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attributeListSize)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::cerr << "InitializeProcThreadAttributeList failed: " << HResultToString(hr) << std::endl;
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
        CloseAllHandles();
        return false;
    }

    // Set the pseudo console attribute.
    if (!UpdateProcThreadAttribute(
        siEx.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        m_hPC, // Pass the HPCON to the child process
        sizeof(HPCON),
        nullptr,
        nullptr)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::cerr << "UpdateProcThreadAttribute failed: " << HResultToString(hr) << std::endl;
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
        CloseAllHandles();
        return false;
    }

    // 4. Create the Client Process
    //    CreateProcessW requires a mutable string for commandLine.
    std::vector<wchar_t> commandLineBuf(commandLine.begin(), commandLine.end());
    commandLineBuf.push_back(L'\0'); // Null-terminate

    if (!CreateProcessW(
        nullptr,                // lpApplicationName
        commandLineBuf.data(),  // lpCommandLine
        nullptr,                // lpProcessAttributes
        nullptr,                // lpThreadAttributes
        FALSE,                  // bInheritHandles (ConPTY handles this)
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
        nullptr,                // lpEnvironment
        nullptr,                // lpCurrentDirectory
        &siEx.StartupInfo,      // lpStartupInfo
        &m_piClient             // lpProcessInformation
    )) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::cerr << "CreateProcessW failed: " << HResultToString(hr) << std::endl;
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
        CloseAllHandles(); // This will close m_hPC and our pipe ends
        return false;
    }

    // Clean up attribute list
    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

    // Success!
    m_running = true;

    // 5. Start the output reading thread
    m_outputThread = std::thread(&ConPtyProcess::OutputThreadFunc, this);

    std::cout << "ConPtyProcess started successfully." << std::endl;
    return true;
}

void ConPtyProcess::OutputThreadFunc() {
    const DWORD BUFFER_SIZE = 4096;
    std::vector<char> buffer(BUFFER_SIZE);
    DWORD bytesRead = 0;

    while (m_running) {
        // ReadFile will block until data is available or an error occurs (e.g., pipe closed)
        BOOL success = ReadFile(m_hInputPipeOurRead, buffer.data(), BUFFER_SIZE, &bytesRead, nullptr);

        if (!success || bytesRead == 0) {
            // Error or pipe closed (client process likely exited)
            // ERROR_BROKEN_PIPE is expected when the PTY client closes its end of the pipe.
            DWORD error = GetLastError();
            if (error != ERROR_BROKEN_PIPE && error != ERROR_SUCCESS /* ReadFile can return TRUE with bytesRead=0 on EOF */) {
                std::cerr << "ReadFile failed in OutputThreadFunc: " << HResultToString(HRESULT_FROM_WIN32(error)) << std::endl;
            }
            m_running = false; // Signal to stop
            break;
        }

        if (m_onDataReceivedCallback) {
            m_onDataReceivedCallback(buffer.data(), bytesRead);
        }
    }
    std::cout << "OutputThreadFunc exiting." << std::endl;

    // Optionally, trigger an event or callback here to notify that the connection is lost
    // This can be useful for the UI to know the terminal session ended.
}

bool ConPtyProcess::WriteInput(const std::string& data) {
    if (!m_running || m_hOutputPipeOurWrite == INVALID_HANDLE_VALUE || data.empty()) {
        return false;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(m_hOutputPipeOurWrite, data.c_str(), static_cast<DWORD>(data.length()), &bytesWritten, nullptr)) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        std::cerr << "WriteFile failed: " << HResultToString(hr) << std::endl;
        // If WriteFile fails, it might mean the process has terminated.
        // Consider setting m_running to false or handling this more gracefully.
        return false;
    }
    return true;
}

bool ConPtyProcess::Resize(COORD newSize) {
    if (!m_running || m_hPC == nullptr) {
        return false;
    }

    HRESULT hr = ResizePseudoConsole(m_hPC, newSize);
    if (FAILED(hr)) {
        std::cerr << "ResizePseudoConsole failed: " << HResultToString(hr) << std::endl;
        return false;
    }
    return true;
}

void ConPtyProcess::Stop() {
    if (!m_running && m_hPC == nullptr && m_piClient.hProcess == INVALID_HANDLE_VALUE) {
        // Already stopped or never started fully
        return;
    }
    std::cout << "ConPtyProcess Stop requested." << std::endl;
    m_running = false; // Signal output thread to stop

    // Closing our write end of the PTY input pipe might be necessary
    // if the PTY client is blocked on a read, but typically TerminateProcess is enough.
    // Closing our read end of the PTY output pipe will cause ReadFile in OutputThreadFunc to unblock.
    if (m_hInputPipeOurRead != INVALID_HANDLE_VALUE && m_hInputPipeOurRead != nullptr) {
        // This might be controversial. If the thread is stuck in ReadFile,
        // closing the handle will make it return with an error.
        // Alternatively, rely on m_running flag and potentially short timeouts in ReadFile,
        // or use overlapped I/O with cancellation. For simplicity, we close.
        // Note: If the client process exits, ReadFile will also unblock.
        // This handle is closed in CloseAllHandles eventually.
    }


    // Wait for the output thread to finish
    if (m_outputThread.joinable()) {
        m_outputThread.join();
    }
    std::cout << "Output thread joined." << std::endl;


    // Terminate the client process if it's still running
    if (m_piClient.hProcess != INVALID_HANDLE_VALUE && m_piClient.hProcess != nullptr) {
        // Check if the process is still running
        DWORD exitCode;
        if (GetExitCodeProcess(m_piClient.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            std::cout << "Terminating client process." << std::endl;
            TerminateProcess(m_piClient.hProcess, 0);
            WaitForSingleObject(m_piClient.hProcess, 5000); // Wait a bit for termination
        }
    }

    // Now clean up all handles
    CloseAllHandles();

    std::cout << "ConPtyProcess stopped." << std::endl;
}