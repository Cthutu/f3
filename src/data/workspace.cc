//----------------------------------------------------------------------------------------------------------------------
// Workspace operations
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/backends.h>
#include <data/workspace.h>
#include <utils/msg.h>
#include <utils/utils.h>

using namespace std;
namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// scanSrc

func scanSrc(unique_ptr<Node>& root, const fs::path& path, Node::Type folderType) -> void
{
    if (fs::exists(path) && fs::is_directory(path))
    {
        auto fnode = make_unique<Node>(folderType, fs::path(path));
        for (const auto& entry : fs::directory_iterator(fnode->fullPath))
        {
            const fs::path& path = entry.path();
            if (fs::is_directory(path))
            {
                scanSrc(fnode, path, folderType);
            }
            else
            {
                string ext = path.extension().string();
                if (ext == ".cc" || ext == ".cpp")
                {
                    auto ccnode = make_unique<Node>(Node::Type::SourceFile, fs::path(path));
                    fnode->nodes.push_back(move(ccnode));
                }
                else if (ext == ".h" || ext == ".hpp")
                {
                    auto hnode = make_unique<Node>(Node::Type::HeaderFile, fs::path(path));
                    fnode->nodes.push_back(move(hnode));
                }
                else
                {
                    // Ignore all other file types.
                }
            }
        }

        root->nodes.push_back(move(fnode));
    }
}

//----------------------------------------------------------------------------------------------------------------------
// processDeps

func buildProject(Workspace& ws, const Env& env) -> bool;

func processDeps(Workspace& ws, const Env& env, ProjectRef& proj) -> bool
{
    auto kvs = proj->config.fetchSection("dependencies");
    for (const auto&[key, value] : kvs)
    {
        Project::Dep d;
        auto elems = split(key, ":");
        if (elems.size() != 2)
        {
            return error(env.cmdLine, stringFormat("Invalid dependency declaration: `{0}`.", key));
        }

        if (elems[0] == "local")
        {
            // Local library
            d.name = elems[1];
            fs::path projPath = fs::canonical(proj->rootPath / value);
            Env newEnv(env, move(projPath));
            if (!buildProject(ws, newEnv)) return false;
            d.proj = ws.projects.back().get();
        }
        else
        {
            return error(env.cmdLine, stringFormat("Invalid dependency type: `{0}`.", elems[0]));
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// buildProjects

func buildProject(Workspace& ws, const Env& env) -> bool
{
    if (!checkProject(env))
    {
        return error(env.cmdLine, stringFormat("Invalid project path at `{0}`.", env.rootPath));
    }

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

    p->rootNode = make_unique<Node>(Node::Type::Root, fs::path(p->rootPath));

    string appTypeStr = p->config.get("info.type");
    if (appTypeStr == "lib")
    {
        p->appType = AppType::Library;
        p->ssType = SubsystemType::NotRequired;
    }
    else if (appTypeStr == "dll")
    {
        p->appType = AppType::DynamicLibrary;
        p->ssType = SubsystemType::NotRequired;
    }
    else if (appTypeStr == "exe")
    {
        p->appType = AppType::Exe;
        string ssTypeStr = p->config.get("info.subsystem");
        if (ssTypeStr == "windows")
        {
            p->ssType = SubsystemType::Windows;
        }
        else if (ssTypeStr == "console" || ssTypeStr.empty())
        {
            p->ssType = SubsystemType::Console;
        }
        else
        {
            return error(env.cmdLine, "Invalid subsystem type (info.subsystem).");
        }
    }
    else
    {
        return error(env.cmdLine, "Invalid application type (info.type).");
    }

    scanSrc(p->rootNode, p->rootPath / "src", Node::Type::SourceFolder);
    if (p->appType == AppType::Library || p->appType == AppType::DynamicLibrary)
    {
        scanSrc(p->rootNode, p->rootPath / "inc", Node::Type::ApiFolder);
        scanSrc(p->rootNode, p->rootPath / "test", Node::Type::TestFolder);
    }

    if (!processDeps(ws, env, p)) return false;

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
