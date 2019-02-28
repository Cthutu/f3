//----------------------------------------------------------------------------------------------------------------------
// Workspace data structure
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <data/config.h>
#include <data/env.h>
#include <filesystem>

//----------------------------------------------------------------------------------------------------------------------
// Node
// 
// Represents a source code element
//----------------------------------------------------------------------------------------------------------------------

struct Node
{
    enum class Type
    {
        SourceFile,
        HeaderFile,
        SourceFolder,
        TestFolder,
        ApiFolder,
    };

    Type                        type;           // Node type
    std::filesystem::path       fullPath;       // Full path of source file/folder
};

//----------------------------------------------------------------------------------------------------------------------
// Project
//
// Represents a single project
//----------------------------------------------------------------------------------------------------------------------

struct Project
{
    Env                         env;
    std::filesystem::path       rootPath;       // Path of project (intended or actual)
    std::string                 name;
    Config                      config;         // Project's forge.ini file.
    std::vector<Project*>       deps;           // Project indices for all the dependencies.

    Project(const Env& env, std::filesystem::path&& path)
        : env(env, std::filesystem::path(path))

    {

    }
};

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
};

//----------------------------------------------------------------------------------------------------------------------
// Workspace APIs
//----------------------------------------------------------------------------------------------------------------------

func buildWorkspace(const Env& env) -> std::unique_ptr<Workspace>;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
