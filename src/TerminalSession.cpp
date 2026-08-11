#include "TerminalSession.h"

#include "Utf.h"

#include <processthreadsapi.h>

#include <algorithm>
#include <array>

namespace {
void CloseHandleIf(HANDLE& handle)
{
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = nullptr;
    }
}
}

TerminalSession::~TerminalSession()
{
    Stop();
}

void TerminalSession::SetCallbacks(OutputCallback output, ExitCallback exited)
{
    outputCallback_ = std::move(output);
    exitCallback_ = std::move(exited);
}

void TerminalSession::SetError(std::wstring message)
{
    lastError_ = std::move(message);
}

bool TerminalSession::CreatePipes()
{
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    if (!CreatePipe(&inputRead_, &inputWrite_, &security, 0)) {
        SetError(L"创建 ConPTY 输入管道失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }
    if (!SetHandleInformation(inputWrite_, HANDLE_FLAG_INHERIT, 0)) {
        SetError(L"设置输入管道句柄失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }
    if (!CreatePipe(&outputRead_, &outputWrite_, &security, 0)) {
        SetError(L"创建 ConPTY 输出管道失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }
    if (!SetHandleInformation(outputRead_, HANDLE_FLAG_INHERIT, 0)) {
        SetError(L"设置输出管道句柄失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }
    return true;
}

bool TerminalSession::CreatePseudoConsoleHost(short columns, short rows)
{
    const COORD size{columns, rows};
    const HRESULT result = CreatePseudoConsole(size, inputRead_, outputWrite_, 0, &hpc_);
    if (FAILED(result)) {
        SetError(L"创建 ConPTY 失败：" + Win32ErrorMessage(static_cast<unsigned long>(result)));
        return false;
    }
    CloseHandleIf(inputRead_);
    CloseHandleIf(outputWrite_);
    return true;
}

bool TerminalSession::CreateShellProcess(const TerminalLaunchSpec& spec)
{
    if (spec.executable.empty() || GetFileAttributesW(spec.executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SetError(L"找不到" + (spec.displayName.empty() ? std::wstring(L"终端程序") : spec.displayName) +
                 L"：" + spec.executable);
        return false;
    }

    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    auto* attributes = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attributeSize));
    if (attributes == nullptr) {
        SetError(L"分配进程属性列表失败");
        return false;
    }
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize)) {
        HeapFree(GetProcessHeap(), 0, attributes);
        SetError(L"初始化进程属性列表失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   hpc_, sizeof(hpc_), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(attributes);
        HeapFree(GetProcessHeap(), 0, attributes);
        SetError(L"绑定 ConPTY 到" + spec.displayName + L"失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine = L"\"" + spec.executable + L"\"";
    if (!spec.arguments.empty()) commandLine += L" " + spec.arguments;
    const DWORD flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
    const BOOL created = CreateProcessW(spec.executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                                        flags, nullptr, spec.workingDirectory.empty() ? nullptr : spec.workingDirectory.c_str(),
                                        &startup.StartupInfo, &processInfo);
    DeleteProcThreadAttributeList(attributes);
    HeapFree(GetProcessHeap(), 0, attributes);
    if (!created) {
        SetError(L"启动" + spec.displayName + L"失败：" + Win32ErrorMessage(GetLastError()));
        return false;
    }
    process_ = processInfo.hProcess;
    processThread_ = processInfo.hThread;
    return true;
}

bool TerminalSession::Start(const TerminalLaunchSpec& spec, short columns, short rows)
{
    if (running_) {
        return true;
    }
    if (hpc_ != nullptr || process_ != nullptr || readerThread_.joinable() ||
        writerThread_.joinable() || processWaiterThread_.joinable()) {
        Stop();
    }
    lastError_.clear();
    stopping_ = false;
    {
        std::lock_guard lock(queueMutex_);
        inputQueue_.clear();
    }
    launchSpec_ = spec;
    if (!CreatePipes() || !CreatePseudoConsoleHost(columns, rows) || !CreateShellProcess(spec)) {
        Stop();
        return false;
    }

    const uint64_t generation = ++generation_;
    lastOutputTick_ = GetTickCount64();
    running_ = true;
    readerThread_ = std::thread(&TerminalSession::ReaderLoop, this, generation);
    writerThread_ = std::thread(&TerminalSession::WriterLoop, this);
    processWaiterThread_ = std::thread(&TerminalSession::ProcessWaitLoop, this, generation);
    return true;
}

void TerminalSession::Stop()
{
    const bool hadResources = running_.load() || hpc_ != nullptr || process_ != nullptr ||
                              readerThread_.joinable() || writerThread_.joinable() || processWaiterThread_.joinable() ||
                              inputRead_ != nullptr || inputWrite_ != nullptr ||
                              outputRead_ != nullptr || outputWrite_ != nullptr ||
                              processThread_ != nullptr;
    if (!hadResources) {
        return;
    }
    stopping_ = true;
    running_ = false;
    queueCv_.notify_all();

    if (hpc_ != nullptr) {
        ClosePseudoConsole(hpc_);
        hpc_ = nullptr;
    }
    if (process_ != nullptr) {
        if (WaitForSingleObject(process_, 350) == WAIT_TIMEOUT) {
            TerminateProcess(process_, 1);
            WaitForSingleObject(process_, 350);
        }
    }
    CloseHandleIf(inputWrite_);
    CloseHandleIf(outputRead_);

    if (writerThread_.joinable()) writerThread_.join();
    if (readerThread_.joinable()) readerThread_.join();
    if (processWaiterThread_.joinable()) processWaiterThread_.join();

    CloseHandleIf(inputRead_);
    CloseHandleIf(outputWrite_);
    CloseHandleIf(processThread_);
    CloseHandleIf(process_);
    {
        std::lock_guard lock(queueMutex_);
        inputQueue_.clear();
    }
}

bool TerminalSession::Restart(short columns, short rows)
{
    Stop();
    return Start(launchSpec_, columns, rows);
}

bool TerminalSession::SendRaw(std::string_view utf8)
{
    if (!running_ || utf8.empty()) {
        return false;
    }
    {
        std::lock_guard lock(queueMutex_);
        if (stopping_) return false;
        inputQueue_.emplace_back(utf8);
    }
    queueCv_.notify_one();
    return true;
}

void TerminalSession::Resize(short columns, short rows)
{
    std::lock_guard lock(consoleMutex_);
    if (hpc_ != nullptr) {
        ResizePseudoConsole(hpc_, COORD{columns, rows});
    }
}

void TerminalSession::WriterLoop()
{
    for (;;) {
        std::string item;
        {
            std::unique_lock lock(queueMutex_);
            queueCv_.wait(lock, [this] { return stopping_ || !inputQueue_.empty(); });
            if (inputQueue_.empty() && stopping_) break;
            item = std::move(inputQueue_.front());
            inputQueue_.pop_front();
        }
        if (inputWrite_ == nullptr) break;
        DWORD written = 0;
        if (!WriteFile(inputWrite_, item.data(), static_cast<DWORD>(item.size()), &written, nullptr)) {
            if (!stopping_) stopping_ = true;
            break;
        }
    }
}

void TerminalSession::ReaderLoop(uint64_t generation)
{
    std::array<unsigned char, 8192> buffer{};
    for (;;) {
        DWORD read = 0;
        if (outputRead_ == nullptr || !ReadFile(outputRead_, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            break;
        }
        if (read == 0) break;
        lastOutputTick_ = GetTickCount64();
        if (outputCallback_) {
            outputCallback_(generation, std::string(reinterpret_cast<char*>(buffer.data()), read));
        }
        lastOutputTick_ = GetTickCount64();
    }
    (void)generation;
}

void TerminalSession::ProcessWaitLoop(uint64_t generation)
{
    if (process_ == nullptr) return;
    WaitForSingleObject(process_, INFINITE);
    const ULONGLONG exitTick = GetTickCount64();
    constexpr ULONGLONG quietPeriodMs = 100;
    constexpr ULONGLONG maximumDrainMs = 600;
    for (;;) {
        const ULONGLONG now = GetTickCount64();
        const ULONGLONG lastOutput = std::max(exitTick, lastOutputTick_.load());
        if (now - lastOutput >= quietPeriodMs || now - exitTick >= maximumDrainMs) break;
        Sleep(10);
    }
    const bool expectedStop = stopping_.exchange(true);
    running_ = false;
    queueCv_.notify_all();
    if (!expectedStop && exitCallback_) exitCallback_(generation);
}
