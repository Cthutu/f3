//----------------------------------------------------------------------------------------------------------------------
// User message handling
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <utils/colour_streams.h>
#include <utils/msg.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------

func error(const CmdLine& cmdLine, std::string message) -> bool
{
    cout << ansi::red << "ERROR: " << ansi::reset << message << endl;
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

func msg(const CmdLine& cmdLine, std::string action, std::string info) -> void
{
    assert(action.size() <= 12);

    while (action.size() < 12)
    {
        action = string(" ") + action;
    }

    cout << ansi::green << action << ansi::reset << ' ' << info << endl;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
