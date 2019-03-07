//----------------------------------------------------------------------------------------------------------------------
// Interface for various back-ends
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <data/workspace.h>

//----------------------------------------------------------------------------------------------------------------------
// BuildState

enum class BuildState
{
    Success,
    Failed,
    NoWork,
};

//----------------------------------------------------------------------------------------------------------------------
// Back-end

class CmdLine;

class IBackend
{
public:
    virtual func available() const -> bool = 0;
    virtual func generateWorkspace(const WorkspaceRef workspace) -> bool = 0;
    virtual func launchIde(const WorkspaceRef workspace) -> void = 0;
    virtual func build(const WorkspaceRef ws) -> BuildState = 0;
    virtual func buildProject(const ProjectRef p) -> BuildState = 0;

protected:
    func scanDependencies(const ProjectRef proj, NodeRef node) -> void;
    func getIncludePaths(const ProjectRef proj, std::vector<std::filesystem::path>& paths) -> void;
    func getLibPaths(const ProjectRef proj, BuildType buildType, std::vector<std::filesystem::path>& paths) -> void;
};

//----------------------------------------------------------------------------------------------------------------------
// Back-end factory

func getBackend(const CmdLine& cmdLine) -> std::unique_ptr<IBackend>;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
