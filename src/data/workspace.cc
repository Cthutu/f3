//----------------------------------------------------------------------------------------------------------------------
// Workspace operations
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>
#include <data/workspace.h>
#include <utils/msg.h>
#include <utils/utils.h>

using namespace std;
namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// buildProjects

func buildProject(Workspace& ws, const Env& env) -> bool
{
    assert(checkProject(env));

    auto p = make_unique<Project>(env, fs::path(env.rootPath));
    p->rootPath = env.rootPath;
    p->config.readIni(env.cmdLine, env.rootPath / "forge.ini");

    auto maybeName = p->config.tryGet("info.name");
    if (maybeName)
    {
        p->name = *maybeName;
    }
    else return error(env.cmdLine, stringFormat("Project at `{0}` doesn't have a name (add info.name entry to forge.ini).", env.rootPath));

    p->guid = generateGuid();

    // Figure out application type
    auto maybeAppType = p->config.tryGet("info.type");
    if (maybeAppType)
    {
        if (maybeAppType == "lib")
        {
            p->appType = AppType::Library;
        }
        else if (maybeAppType == "dll")
        {
            p->appType = AppType::DynamicLibrary;
        }
        else if (maybeAppType == "exe")
        {
            p->appType = AppType::Exe;
        }
        else return error(env.cmdLine, stringFormat("Unknown application type.  Please check info.type entry in forge.ini for project `{0}`.", p->name));
    }

    // Figure out subsystem type
    p->subsystemType = SubsystemType::NotRequired;
    if (p->appType == AppType::Exe)
    {
        auto maybeSSType = p->config.tryGet("info.subsystem");
        if (maybeSSType)
        {
            if (maybeSSType == "console")
            {
                p->subsystemType = SubsystemType::Console;
            }
            else if (maybeSSType == "windows")
            {
                p->subsystemType = SubsystemType::Windows;
            }
            else
            {
                return error(env.cmdLine, stringFormat("Unknown subsystem type: `{0}`", *maybeSSType));
            }
        }
        else
        {
            p->subsystemType = SubsystemType::Console;
        }
    }

    ws.projects.push_back(move(p));
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// buildWorkspace

func buildWorkspace(const Env& env) -> unique_ptr<Workspace>
{
    if (!checkProject(env))
    {
        error(env.cmdLine, "Unable to find forge project.");
        return false;
    }

    auto ws = make_unique<Workspace>();
    ws->rootPath = env.rootPath;
    ws->guid = generateGuid();

    if (!buildProject(*ws, env))
    {
        return {};
    }

    return ws;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
