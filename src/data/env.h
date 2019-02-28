//----------------------------------------------------------------------------------------------------------------------
// Forge environment
// Represents the entire state of a project and its dependent projects
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <utils/cmdline.h>

namespace fs = ::std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// BuildType

enum class BuildType
{
    Debug,
    Release
};

//----------------------------------------------------------------------------------------------------------------------
// Env
// Root environment

struct Env
{
    fs::path    rootPath;       // Path of main project
    CmdLine     cmdLine;        // Command line configuration
    BuildType   buildType;      // Debug or release?

    Env(int argc, char** argv, fs::path&& path);
    Env(const Env& env, fs::path&& path);

private:
    func initPath(fs::path&& path) -> void;
};

//----------------------------------------------------------------------------------------------------------------------
// Some utilities on the environment

// Check to see if there's a valid project.
func checkProject(const Env& env) -> bool;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
