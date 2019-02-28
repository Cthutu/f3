//----------------------------------------------------------------------------------------------------------------------
// Forge build and package management system
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <array>
#include <data/env.h>
#include <functional>
#include <iostream>
#include <utils/msg.h>

//----------------------------------------------------------------------------------------------------------------------
// Command dispatcher

func cmd_new(const Env& env) -> int;
func cmd_edit(const Env& env) -> int;
func cmd_clean(const Env& env) -> int;
func cmd_build(const Env& env) -> int;
func cmd_run(const Env& env) -> int;
func cmd_test(const Env& env) -> int;

//----------------------------------------------------------------------------------------------------------------------

func processCmd(const Env& env) -> int
{
    struct CommandInfo
    {
        using Handler = function<int(const Env&)>;

        string      cmd;        // Command name
        Handler     handler;    // Function that handles this command

        CommandInfo(string&& cmd, Handler&& handler) : cmd(move(cmd)), handler(move(handler)) {}
    };

    array<CommandInfo, 6> commands =
    {
        CommandInfo { "new", cmd_new },
        CommandInfo { "edit", cmd_edit },
        CommandInfo { "clean", cmd_clean },
        CommandInfo { "build", cmd_build },
        CommandInfo { "run", cmd_run },
        CommandInfo { "test", cmd_test },
    };

    bool foundCommand = false;
    int result = 1;

    for (const auto& cmd : commands)
    {
        if (cmd.cmd == env.cmdLine.command())
        {
            result = cmd.handler(env);
            foundCommand = true;
        }
    }
    if (!foundCommand)
    {
        error(env.cmdLine, stringFormat("Unknown command '{0}'.", env.cmdLine.command()));
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------
// Usage

func usage() -> void
{
    cout << "Forge (version Dev.0.1)" << endl;
    cout << "Usage: forge <command> [<params and flags> ...] [-- <sub-params>]" << endl << endl;

    cout << "Command:" << endl;
    cout << "  new        Create a new project." << endl;
    cout << "  edit       Generate IDE files and launch the IDE." << endl;
    cout << "  build      Build the project." << endl;
    cout << "  run        Build (if necessary) and run the project (if it's an exe)." << endl;
    cout << "  clean      Remove all generated files." << endl;
    cout << "  test       Build the library and unit test executable, and run it." << endl;

    cout << endl;
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry point

func main(int argc, char** argv) -> int
{
#if OS_WIN32
    DWORD mode;
    HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(handleOut, &mode);
    mode |= 0x04;
    SetConsoleMode(handleOut, mode);
#endif
    
    Env env(argc, argv, fs::current_path());

    if (env.cmdLine.command().empty())
    {
        usage();
    }
    else
    {
        return processCmd(env);
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
