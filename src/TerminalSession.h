#pragma once

#include <windows.h>
#include <consoleapi2.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

class TerminalSession
{
public:
    using OutputCallback = std::function<void(uint64_t, std::string)>;
    using ExitCallback = std::function<void(uint64_t)>;

    TerminalSession() = default;
    ~TerminalSession();

    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

    void SetCallbacks(OutputCallback output, ExitCallback exited);
    bool Start(short columns = 120, short rows = 30);
    void Stop();
    bool Restart(short columns = 120, short rows = 30);
    bool SendRaw(std::string_view utf8);
    void Resize(short columns, short rows);
    bool IsRunning() const { return running_.load(); }
    uint64_t Generation() const { return generation_; }
    const std::wstring& LastError() const { return lastError_; }

private:
    bool CreatePipes();
    bool CreatePseudoConsoleHost(short columns, short rows);
    bool CreateShellProcess();
    void ReaderLoop(uint64_t generation);
    void WriterLoop();
    void SetError(std::wstring message);

    HPCON hpc_ = nullptr;
    HANDLE inputRead_ = nullptr;
    HANDLE inputWrite_ = nullptr;
    HANDLE outputRead_ = nullptr;
    HANDLE outputWrite_ = nullptr;
    HANDLE process_ = nullptr;
    HANDLE processThread_ = nullptr;

    std::thread readerThread_;
    std::thread writerThread_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<std::string> inputQueue_;
    std::mutex consoleMutex_;
    std::atomic_bool stopping_ = false;
    std::atomic_bool running_ = false;
    uint64_t generation_ = 0;
    std::wstring lastError_;
    OutputCallback outputCallback_;
    ExitCallback exitCallback_;
};
