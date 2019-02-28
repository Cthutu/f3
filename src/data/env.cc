//----------------------------------------------------------------------------------------------------------------------
// Builds and maintains the environment
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <data/env.h>
#include <utils/msg.h>

//----------------------------------------------------------------------------------------------------------------------
// Constructor

Env::Env(int argc, char** argv, fs::path&& path)
    : rootPath()
    , cmdLine(argc, argv)
    , buildType(BuildType::Debug)
{
    initPath(move(path));

    if (cmdLine.flag("release"))
    {
        buildType = BuildType::Release;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Construction based on previous environment

Env::Env(const Env& env, fs::path&& path)
    : rootPath()
    , cmdLine(env.cmdLine)
    , buildType(env.buildType)
{
    initPath(move(path));
}

//----------------------------------------------------------------------------------------------------------------------
// initPath

func Env::initPath(fs::path&& path) -> void
{
    fs::path cwd(move(path));
    while (cwd != cwd.parent_path())
    {
        fs::path iniPath = cwd / "forge.ini";
        if (fs::exists(iniPath))
        {
            // We have a valid project directory
            rootPath = cwd;
            break;
        }

        cwd = cwd.parent_path();
    }
}

//----------------------------------------------------------------------------------------------------------------------

func checkProject(const Env& env) -> bool
{
    if (env.rootPath.empty())
    {
        return error(env.cmdLine, "Not inside a forge project.");
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
