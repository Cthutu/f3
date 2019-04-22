//----------------------------------------------------------------------------------------------------------------------
// Workspace data structure
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <data/config.h>
#include <data/env.h>
#include <data/geninfo.h>
#include <filesystem>
#include <set>

//----------------------------------------------------------------------------------------------------------------------
// Node
// 
// Represents a source code element
//----------------------------------------------------------------------------------------------------------------------

struct Node
{
    enum class Type
    {
        Root,
        SourceFile,
        HeaderFile,
        SourceFolder,
        TestFolder,
        ApiFolder,
        PchFile,
        DataFolder,
        DataFile,
    };

    Type                                type;           // Node type
    std::filesystem::path               fullPath;       // Full path of source file/folder
    std::vector<std::unique_ptr<Node>>  nodes;          // Child nodes
    std::set<std::filesystem::path>     deps;           // Dependencies

    Node(Type type, std::filesystem::path&& fullPath)
        : type(type)
        , fullPath(move(fullPath))
    {}
};

//----------------------------------------------------------------------------------------------------------------------
// Project
//
// Represents a single project
//----------------------------------------------------------------------------------------------------------------------

using Paths = std::vector<std::filesystem::path>;

struct Project
{
    Env                         env;
    std::filesystem::path       rootPath;       // Path of project (intended or actual)
    std::string                 name;
    Config                      config;         // Project's forge.ini file.
    std::string                 guid;
    std::unique_ptr<Node>       rootNode;
    AppType                     appType;
    SubsystemType               ssType;

    struct Dep
    {
        string name;
        string version;
        Project* proj;
    };
    std::vector<Dep>            deps;           // Project indices for all the dependencies.

    Project(const Env& env, std::filesystem::path&& path)
        : env(env, std::filesystem::path(path))
    {

    }
};

using ProjectRef = std::unique_ptr<Project>&;

//----------------------------------------------------------------------------------------------------------------------
// Workspace
//
// Contains all projects that need to be included to build the root project.  Also contains the original environment
// that is used to construct the projects
//----------------------------------------------------------------------------------------------------------------------

struct Workspace
{
    std::filesystem::path                   rootPath;       // Root path of initial project
    std::vector<std::unique_ptr<Project>>   projects;
    std::string                             guid;
    Paths                                   buildOrder;
};

using WorkspaceRef = std::unique_ptr<Workspace>&;

//----------------------------------------------------------------------------------------------------------------------
// Workspace APIs
//----------------------------------------------------------------------------------------------------------------------

func buildWorkspace(const Env& env) -> std::unique_ptr<Workspace>;
func getProjectCompleteDeps(const Project* proj) -> std::set<Project *>;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
