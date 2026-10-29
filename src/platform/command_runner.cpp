#include "platform/command_runner.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <utility>

namespace platform {
namespace {

class UniqueHandle {
    public:
    explicit UniqueHandle(HANDLE handle = nullptr) : handle_(handle) {}

    ~UniqueHandle() { Reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.Release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
        Reset(other.Release());
        }
        return *this;
    }

    HANDLE get() const { return handle_; }

    HANDLE Release() {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    void Reset(HANDLE handle = nullptr) 
    {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) 
        {
            CloseHandle(handle_);   
        }
        handle_ = handle;
    }
    private:
    HANDLE handle_ = nullptr;
};

}/* namespace */

CommandResult RunCommand(const std::wstring& command_line) 
{
    CommandResult result;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE read_handle = nullptr;
    HANDLE write_handle = nullptr;
    if (CreatePipe(&read_handle, &write_handle, &sa, 0) == 0) 
    {
        result.error = GetLastError();
        return result;
    }

    UniqueHandle read_pipe(read_handle);
    UniqueHandle write_pipe(write_handle);

    /* 读端不能被继承，否则子进程也会持有它。 */
    if (SetHandleInformation(read_pipe.get(), HANDLE_FLAG_INHERIT, 0) == 0) 
    {
        result.error = GetLastError();
        return result;
    }
    /* 命令不需要输入时，用 NUL 提供有效的 stdin。 */
    UniqueHandle nul_handle(CreateFileW(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa, OPEN_EXISTING, 0, nullptr));

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput =
        (nul_handle.get() == INVALID_HANDLE_VALUE) ? nullptr : nul_handle.get();
    si.hStdOutput = write_pipe.get();
    si.hStdError = write_pipe.get();

    PROCESS_INFORMATION pi{};
    std::wstring mutable_command = command_line;

    const BOOL created = CreateProcessW(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (created == 0) {
        result.error = GetLastError();
        return result;
    }

    result.launched = true;

    UniqueHandle process(pi.hProcess);
    UniqueHandle thread(pi.hThread);

    /* 子进程已经继承写端，父进程必须关闭，否则读端等不到 EOF。 */
    write_pipe.Reset();
    nul_handle.Reset();

    char buffer[4096];
    while (true) {
        DWORD bytes_read = 0;
        const BOOL ok =
            ReadFile(read_pipe.get(), buffer, sizeof(buffer), &bytes_read, nullptr);

        if (ok == 0) {
        const DWORD error = GetLastError();
        if (error != ERROR_BROKEN_PIPE) {
            result.error = error;
        }
        break;
        }

        if (bytes_read == 0) {
        break;
        }

        result.output.append(buffer, bytes_read);
    }

    const DWORD wait_result = WaitForSingleObject(process.get(), INFINITE);
    if (wait_result == WAIT_FAILED) {
        result.error = GetLastError();
    }

    DWORD exit_code = 0;
    if (GetExitCodeProcess(process.get(), &exit_code) == 0) {
        result.error = GetLastError();
    } else {
        result.exit_code = exit_code;
    }

    return result;
    }
} /* namespace platform */
