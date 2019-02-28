//---------------------------------------------------------------------------------------------------------------------
// Command processing.
//---------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <utils/cmdline.h>

#if OS_WIN32
#   include <Windows.h>
#endif

using namespace std;

//---------------------------------------------------------------------------------------------------------------------
// Constructor

CmdLine::CmdLine(int argc, char** argv)
{
    //
    // Get executable path name
    //

#if OS_WIN32
    int len = MAX_PATH;
    for (;;)
    {
        char* buf = new char[len];
        if (buf)
        {
            DWORD pathLen = GetModuleFileNameA(0, buf, len);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                len = 3 * len / 2;
                delete[] buf;
                continue;
            }

            while (buf[pathLen] != '\\') --pathLen;
            buf[pathLen] = 0;

            m_exePath = buf;
            delete[] buf;
            break;
        }
    }
#else
#   error Write code for discovering executable path name.
#endif

    //
    // Get command
    //

    if (argc > 1)
    {
        m_command = argv[1];
        argv += 2;
    }

    //
    // Get parameters and flags
    //

    while (*argv)
    {
        if (**argv == '-')
        {
            if ((*argv)[1] == '-')
            {
                if ((*argv)[2] == 0)
                {
                    // Double hyphen - start of secondary parameters.
                    ++argv;
                    while (*argv != 0)
                    {
                        m_secondaryParams.emplace_back(*argv);
                        ++argv;
                    }
                    break;
                }
                else
                {
                    m_flags.emplace(&(*argv)[2]);
                }
            }
            else
            {
                for (char* scan = &(*argv)[1]; *scan != 0; ++scan)
                {
                    m_flags.emplace(scan, scan + 1);
                }
            }
        }
        else
        {
            m_params.emplace_back(*argv);
        }

        ++argv;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// command
// Returns the command parameter

func CmdLine::command() const -> const string&
{
    return m_command;
}

//---------------------------------------------------------------------------------------------------------------------
// exePath
// Returns the full path of this executable (not including the filename).

func CmdLine::exePath() const -> const string&
{
    return m_exePath;
}

//---------------------------------------------------------------------------------------------------------------------
// numParams
// Returns the number of parameters passed to the command (i.e. non-flag parameters).

func CmdLine::numParams() const -> uint
{
    return m_params.size();
}

//---------------------------------------------------------------------------------------------------------------------
// param
// Returns the nth parameter in the command.

func CmdLine::param(uint i) const -> const string&
{
    return m_params[i];
}

//---------------------------------------------------------------------------------------------------------------------
// flag
// Returns true if the flag was given (i.e. via '-x' or '--word')

func CmdLine::flag(string name) const -> bool
{
    return m_flags.find(name) != m_flags.end();
}

//----------------------------------------------------------------------------------------------------------------------
// secondaryParams

func CmdLine::secondaryParams() const -> const vector<string>&
{
    return m_secondaryParams;
}

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
