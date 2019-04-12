//----------------------------------------------------------------------------------------------------------------------
// Clean command
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <data/env.h>
#include <data/workspace.h>
#include <filesystem>
#include <utils/msg.h>

namespace fs = std::filesystem;
using namespace std;

//----------------------------------------------------------------------------------------------------------------------

func cmd_clean(const Env& env) -> int
{
    if (!checkProject(env)) return 1;
    vector<Project*> projsToClean;
    WorkspaceRef ws = buildWorkspace(env);

    if (env.cmdLine.flag("full"))
    {
        for (const auto& proj : ws->projects)
        {
            projsToClean.push_back(proj.get());
        }
    }
    else
    {
        projsToClean.push_back(ws->projects.back().get());
    }

    for (const auto& proj: projsToClean)
    {
        for (const auto& path : fs::directory_iterator(proj->rootPath))
        {
            if (path.is_directory())
            {
                fs::path dir = path.path();
                if (dir.filename().string()[0] == '_')
                {
                    try
                    {
                        fs::remove_all(env.rootPath / dir);
                    }
                    catch (fs::filesystem_error& err)
                    {
                        error(env.cmdLine, stringFormat("File-system error: {0}", err.what()));
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}