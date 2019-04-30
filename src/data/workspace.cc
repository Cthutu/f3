//----------------------------------------------------------------------------------------------------------------------
// Workspace operations
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/backends.h>
#include <data/workspace.h>
#include <functional>
#include <set>
#include <utils/msg.h>
#include <utils/utils.h>

using namespace std;
namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// scanSrc

func scanSrc(unique_ptr<Node>& root, const fs::path& path, Node::Type folderType) -> void
{
    if ((path.filename().string()[0] != '.') && fs::exists(path) && fs::is_directory(path))
    {
        auto fnode = make_unique<Node>(folderType, fs::path(path));
        for (const auto& entry : fs::directory_iterator(fnode->fullPath))
        {
            const fs::path& path = entry.path();

            if (path.filename().string()[0] == '.') continue;

            if (fs::is_directory(path))
            {
                scanSrc(fnode, path, folderType);
            }
            else
            {
                if (folderType == Node::Type::DataFolder)
                {
                    // All files in here, regardless of extension are data files.  We ignore files that start with
                    // a period as they can be used as meta-files.
                    auto newNode = make_unique<Node>(Node::Type::DataFile, fs::path(path));
                    fnode->nodes.push_back(move(newNode));
                }
                else
                {
                    string ext = path.extension().string();
                    if (ext == ".cc" || ext == ".cpp" || ext == ".c")
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
            if (d.name == proj->name)
            {
                return error(env.cmdLine, "A project cannot depend on itself.  Check [dependencies] in the forge.ini file.");
            }
            fs::path projPath = fs::canonical(proj->rootPath / value);
            Env newEnv(env, move(projPath));
            if (!buildProject(ws, newEnv)) return false;
            d.proj = ws.projects.back().get();
            proj->deps.push_back(d);
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

    //
    // Get project name
    //
    auto maybeName = p->config.tryGet("info.name");
    if (maybeName)
    {
        p->name = *maybeName;
    }
    else return error(env.cmdLine, stringFormat("Project at `{0}` doesn't have a name (add info.name entry to forge.ini).", env.rootPath));

    //
    // Generate extra information about project
    //
    p->guid = generateGuid();

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

    //
    // Scan for defines
    //
#if OS_WIN32
    string section = "win32";
#else
#   error Write define scanning code for your platform
#endif
    p->defines["common"] = p->config.fetchSection(section);
    p->defines["debug"] = p->config.fetchSection(section + ".debug");
    p->defines["release"] = p->config.fetchSection(section + ".release");

    //
    // Scan for source code in project
    //
    scanSrc(p->rootNode, p->rootPath / "src", Node::Type::SourceFolder);
    scanSrc(p->rootNode, p->rootPath / "data", Node::Type::DataFolder);
    if (p->appType == AppType::Library || p->appType == AppType::DynamicLibrary)
    {
        scanSrc(p->rootNode, p->rootPath / "inc", Node::Type::ApiFolder);
        scanSrc(p->rootNode, p->rootPath / "test", Node::Type::TestFolder);
    }

    //
    // Get a list of dependencies
    //
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
// getProjectCompleteDeps
// Returns a set of all the dependencies for a particular project (including dependencies of its dependencies).

func getProjectCompleteDeps(const Project* proj) -> set<Project *>
{
    set<Project *> projs;
    function<void(const Project*)> scanProjects = [&projs, &scanProjects](const Project* proj) {
        for (const auto& dep : proj->deps)
        {
            if (projs.find(dep.proj) == projs.end())
            {
                // We don't have this dependency
                projs.insert(dep.proj);
                scanProjects(dep.proj);
            }
        }
    };

    scanProjects(proj);
    return projs;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
