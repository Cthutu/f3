//----------------------------------------------------------------------------------------------------------------------
// Backend factory
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/backends.h>
#include <utils/cmdline.h>
#include <utils/msg.h>

#if OS_WIN32
#   include <backends/vstudio.h>
#endif

using namespace std;

//----------------------------------------------------------------------------------------------------------------------
// getBackEnd

func getBackend(const CmdLine& cmdLine) -> unique_ptr<IBackend>
{
#if OS_WIN32

    auto vs = make_unique<VStudioBackend>();
    if (vs->available()) return vs;

#endif

    error(cmdLine, "Unable to find supported IDE or compiler.");
    return {};
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

