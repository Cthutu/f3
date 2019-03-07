//----------------------------------------------------------------------------------------------------------------------
// Build command
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/backends.h>
#include <data/env.h>
#include <data/workspace.h>
#include <utils/msg.h>

//----------------------------------------------------------------------------------------------------------------------

func cmd_build(const Env& env) -> int
{
    if (!checkProject(env)) return 1;

    auto backEnd = getBackend(env.cmdLine);
    if (!backEnd) return 1;

    auto ws = buildWorkspace(env);
    // env is no longer valid from this point onwards!!!  Fetch it from ws->mainProject->env.

    auto state = backEnd->build(ws);

    switch (state)
    {
    case BuildState::Success:
        msg(env.cmdLine, "Finished", stringFormat("`{0}` built.", ws->projects[0]->name));
        return 0;

    case BuildState::Failed:
        error(env.cmdLine, "Compilation failed.");
        return 1;

    case BuildState::NoWork:
        msg(env.cmdLine, "Finished", "Already up to date.");
        return 0;
    }

    assert(0);
    return 1;
}

