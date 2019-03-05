//----------------------------------------------------------------------------------------------------------------------
// Edit command
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/backends.h>
#include <data/env.h>
#include <data/workspace.h>
#include <utils/msg.h>

//----------------------------------------------------------------------------------------------------------------------

func cmd_edit(const Env& env) -> int
{
    auto ws = buildWorkspace(env);
    if (!ws) return 1;

    auto backend = getBackend(env.cmdLine);
    if (!backend->generateWorkspace(ws))
    {
        error(env.cmdLine, "Unable to generate IDE files.");
        return 1;
    }

    if (!env.cmdLine.flag("gen"))
    {
        backend->launchIde(ws);
    }

    return 0;
}