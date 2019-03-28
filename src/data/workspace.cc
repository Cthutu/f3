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

    auto maybeName = p->config.tryGet("info.name");
    if (maybeName)
    {
        p->name = *maybeName;
    }
    else return error(env.cmdLine, stringFormat("Project at `{0}` doesn't have a name (add info.name entry to forge.ini).", env.rootPath));

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

    //ws->includePaths = includePaths(ws, ws->projects.back());

    return ws;
}

//----------------------------------------------------------------------------------------------------------------------
// getProjectCompleteDeps
// Returns a set of all the dependencies for a particular project (including dependencies of its dependencies).

func getProjectCompleteDeps(const ProjectRef proj) -> set<Project *>
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

    scanProjects(proj.get());
    return projs;
}

//----------------------------------------------------------------------------------------------------------------------
// generateBuildOrder

// func generateBuildOrder(WorkspaceRef ws) -> void
// {
//     vector<const Project*> projs;
//     function<bool(vector<const Project*>&, const Project*)> getBuildOrder =
//         [&getBuildOrder](vector<const Project*>& paths, const Project* proj) -> bool
//     {
//         for (const auto& dep : proj->deps)
//         {
//             if (find(paths.begin(), paths.end(), dep.proj->rootPath) == paths.end())
//             {
//                 getBuildOrder(paths, dep.proj);
//             }
//         }
// 
//         if (find(paths.begin(), paths.end(), proj->rootPath) != paths.end())
//         {
//             return error(proj->env.cmdLine, stringFormat("Cyclic dependency on project at `{0}`.", proj->rootPath));
//         }
// 
//         paths.push_back(proj);
//         return true;
//     };
// 
//     if (!getBuildOrder(projPaths, proj.get())) return {};
// }
// 
// //----------------------------------------------------------------------------------------------------------------------
// // includePaths
// 
// func generatePaths(const WorkspaceRef ws, const ProjectRef proj) -> vector<fs::path>
// {
//     // Step 1 - Generate sorted list of project dependencies.  Here we detect cyclic dependencies.
//     vector<const Project*> projs;
//     function<bool (vector<const Project*>&, const Project*)> getBuildOrder =
//         [&getBuildOrder](vector<const Project*>& paths, const Project* proj) -> bool
//     {
//         for (const auto& dep : proj->deps)
//         {
//             if (find(paths.begin(), paths.end(), dep.proj->rootPath) == paths.end())
//             {
//                 getBuildOrder(paths, dep.proj);
//             }
//         }
// 
//         if (find(paths.begin(), paths.end(), proj->rootPath) != paths.end())
//         {
//             return error(proj->env.cmdLine, stringFormat("Cyclic dependency on project at `{0}`.", proj->rootPath));
//         }
// 
//         paths.push_back(proj);
//         return true;
//     };
// 
//     if (!getBuildOrder(projPaths, proj.get())) return {};
// 
//     // Step 2 - Generate the include paths
//     vector<fs::path> incPaths;
//     for (const auto& proj : projs)
//     {
//         switch (proj->appType)
//         {
//         case AppType::Exe:
//             incPaths.emplace_back(proj->rootPath / "src");
//         }
//     }
//     return projPaths;
// }

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
