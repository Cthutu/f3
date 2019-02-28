//----------------------------------------------------------------------------------------------------------------------
// Process API
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <core.h>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <utils/lines.h>
#include <vector>


#if OS_WIN32
#   include <Windows.h>
#endif

//----------------------------------------------------------------------------------------------------------------------

class Process
{
public:
#if OS_WIN32
    using IdType = unsigned long;
    using FdType = void*;
#else
#   error Define IdType and FdType for your platform
#endif

    using OutputHandler = std::function<void(const char*, size_t)>;

public:
    Process(
        std::string&& cmd,
        std::vector<std::string>&& args,
        std::filesystem::path&& currentPath = std::filesystem::current_path(),
        OutputHandler stdoutReader = nullptr,
        OutputHandler stderrReader = nullptr,
        bool openStdin = false,
        size_t bufferSize = 131072);
    ~Process();

    func id() const->IdType;
    func get() -> int;
    func tryGet(int& outExitCode) -> bool;

    func write(const char* bytes, size_t len) -> bool;
    func write(const std::string& bytes) -> bool;
    func closeStdin() -> void;

private:
    class Data
    {
    public:
        Data();
        IdType id;
#if OS_WIN32
        void* handle;
#endif
    };

    Data m_data;
    bool m_closed;
    std::mutex m_closeMutex;
    OutputHandler m_stdoutHandler;
    OutputHandler m_stderrHandler;
    std::thread m_stdoutThread;
    std::thread m_stderrThread;
    bool m_openStdin;
    std::mutex m_stdinMutex;
    size_t m_bufferSize;
    std::optional<FdType> m_stdout, m_stderr, m_stdin;

    func open(const std::string& cmd, const std::string& path) -> IdType;

    func asyncRead() -> void;
    func closeFds() -> void;
};


//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
