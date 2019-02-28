//----------------------------------------------------------------------------------------------------------------------
// Process API implementation
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <utils/process.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// WIN32 Part
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#if OS_WIN32

//----------------------------------------------------------------------------------------------------------------------
// Data constructor

Process::Data::Data()
    : id(0)
    , handle(0)
{

}

//----------------------------------------------------------------------------------------------------------------------
// Handle wrapper

class Handle
{
public:
    Handle() : m_handle(INVALID_HANDLE_VALUE) {}
    ~Handle() { close(); }

    func close() -> void
    {
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    func detach() -> HANDLE
    {
        HANDLE oldHandle = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return oldHandle;
    }

    operator HANDLE() const { return m_handle; }
    func operator&() -> HANDLE* { return &m_handle; }

private:
    HANDLE m_handle;
};

//----------------------------------------------------------------------------------------------------------------------
// Global mutex

std::mutex gCreateProcessMutex;

//----------------------------------------------------------------------------------------------------------------------
// Open

func Process::open(const string& cmd, const string& path) -> IdType
{
    if (m_openStdin) m_stdin = FdType(0);
    if (m_stdoutHandler) m_stdout = FdType(0);
    if (m_stderrHandler) m_stderr = FdType(0);

    Handle stdin_r;
    Handle stdin_w;
    Handle stdout_r;
    Handle stdout_w;
    Handle stderr_r;
    Handle stderr_w;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    std::lock_guard<std::mutex> lock(gCreateProcessMutex);

    if (m_stdin)
    {
        if (!CreatePipe(&stdin_r, &stdin_w, &sa, 0) ||
            !SetHandleInformation(stdin_w, HANDLE_FLAG_INHERIT, 0))
        {
            return 0;
        }
    }
    if (m_stdout)
    {
        if (!CreatePipe(&stdout_r, &stdout_w, &sa, 0) ||
            !SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0))
        {
            return 0;
        }
    }
    if (m_stderr)
    {
        if (!CreatePipe(&stderr_r, &stderr_w, &sa, 0) ||
            !SetHandleInformation(stderr_r, HANDLE_FLAG_INHERIT, 0))
        {
            return 0;
        }
    }

    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));

    si.cb = sizeof(STARTUPINFO);
    si.hStdInput = stdin_r;
    si.hStdOutput = stdout_w;
    si.hStdError = stderr_w;
    if (m_stdin || m_stdout || m_stderr)
    {
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    BOOL success = CreateProcess(nullptr, (char *)(cmd.empty() ? nullptr : cmd.c_str()), 0, 0, TRUE, 0, 0,
        path.empty() ? nullptr : path.c_str(), &si, &pi);
    if (!success)
    {
        return 0;
    }
    else
    {
        CloseHandle(pi.hThread);
    }

    if (m_stdin) m_stdin = stdin_w.detach();
    if (m_stdout) m_stdout = stdout_r.detach();
    if (m_stderr) m_stderr = stderr_r.detach();

    m_closed = false;
    m_data.id = pi.dwProcessId;
    m_data.handle = pi.hProcess;
    return pi.dwProcessId;
}

//----------------------------------------------------------------------------------------------------------------------
// asyncRead

func Process::asyncRead() -> void
{
    if (m_data.id == 0) return;

    if (m_stdout)
    {
        m_stdoutThread = thread([this]() {
            DWORD n;
            vector<char> buffer(m_bufferSize);
            for (;;)
            {
                BOOL success = ReadFile(*m_stdout, buffer.data(), (DWORD)buffer.size(), &n, nullptr);
                if (!success || n == 0) break;
                m_stdoutHandler(buffer.data(), (size_t)n);
            }
        });
    }
    if (m_stderr)
    {
        m_stderrThread = thread([this]() {
            DWORD n;
            vector<char> buffer(m_bufferSize);
            for (;;)
            {
                BOOL success = ReadFile(*m_stderr, buffer.data(), (DWORD)buffer.size(), &n, nullptr);
                if (!success || n == 0) break;
                m_stderrHandler(buffer.data(), (size_t)n);
            }
        });
    }
}

//----------------------------------------------------------------------------------------------------------------------
// get

func Process::get() -> int
{
    if (m_data.id == 0) return -1;

    DWORD exitStatus;
    WaitForSingleObject(m_data.handle, INFINITE);
    if (!GetExitCodeProcess(m_data.handle, &exitStatus)) exitStatus = -1;

    {
        lock_guard<mutex> lock(m_closeMutex);
        CloseHandle(m_data.handle);
        m_closed = true;
    }

    closeFds();
    return (int)exitStatus;
}

//----------------------------------------------------------------------------------------------------------------------
// tryGet

func Process::tryGet(int& outExitCode) -> bool
{
    if (m_data.id == 0) return false;

    DWORD waitStatus = WaitForSingleObject(m_data.handle, 0);
    if (waitStatus == WAIT_TIMEOUT) return false;

    DWORD exitStatusWin;
    if (!GetExitCodeProcess(m_data.handle, &exitStatusWin)) exitStatusWin = -1;

    {
        lock_guard<mutex> lock(m_closeMutex);
        CloseHandle(m_data.handle);
        m_closed = true;
    }

    closeFds();
    outExitCode = (int)exitStatusWin;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// closeFds

func Process::closeFds() -> void
{
    if (m_stdoutThread.joinable()) m_stdoutThread.join();
    if (m_stderrThread.joinable()) m_stderrThread.join();

    if (m_stdin) closeStdin();
    if (m_stdout) {
        if (*m_stdout != 0) CloseHandle(*m_stdout);
        m_stdout = {};
    }
    if (m_stderr) {
        if (*m_stderr != 0) CloseHandle(*m_stderr);
        m_stderr = {};
    }
}

//----------------------------------------------------------------------------------------------------------------------
// write

func Process::write(const char* bytes, size_t len) -> bool
{
    assert(m_openStdin);

    lock_guard<mutex> lk(m_stdinMutex);
    if (m_stdin)
    {
        DWORD written;
        BOOL success = WriteFile(*m_stdin, bytes, (DWORD)len, &written, nullptr);
        return (success && written != 0);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// closeStdin

func Process::closeStdin() -> void
{
    lock_guard<mutex> lk(m_stdinMutex);
    if (m_stdin)
    {
        if (*m_stdin != 0) CloseHandle(*m_stdin);
        m_stdin = {};
    }
}

//----------------------------------------------------------------------------------------------------------------------


#endif // OS_WIN32

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// Generic Part
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------------
// Constructor

Process::Process(
    std::string&& cmd,
    std::vector<std::string>&& args,
    std::filesystem::path&& currentPath /* = std::filesystem::current_path() */,
    OutputHandler stdoutReader /* = nullptr */,
    OutputHandler stderrReader /* = nullptr */,
    bool openStdin /* = false */,
    size_t bufferSize /* = 131072 */)
    : m_closed(true)
    , m_stdoutHandler(move(stdoutReader))
    , m_stderrHandler(move(stderrReader))
    , m_openStdin(openStdin)
    , m_bufferSize(bufferSize)
{
    cmd = string("\"") + cmd + "\"";
    for (const auto& arg : args)
    {
        if (arg.find("\"") == string::npos && arg.find(" ") != string::npos)
        {
            cmd += " \"" + arg + "\"";
        }
        else
        {
            cmd += " " + arg;
        }
    }

    open(cmd, currentPath.string());
    asyncRead();
}

//----------------------------------------------------------------------------------------------------------------------
// Destructor

Process::~Process()
{
    closeFds();
}

//----------------------------------------------------------------------------------------------------------------------

func Process::id() const -> IdType
{
    return m_data.id;
}

//----------------------------------------------------------------------------------------------------------------------

func Process::write(const std::string& bytes) -> bool
{
    return write(bytes.c_str(), bytes.size());
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

