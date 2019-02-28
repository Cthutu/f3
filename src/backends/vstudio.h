//----------------------------------------------------------------------------------------------------------------------
// Visual Studio 2017 Backend
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <backends/backends.h>
#include <filesystem>

//----------------------------------------------------------------------------------------------------------------------
// VStudioBackend

class VStudioBackend : public IBackend
{
public:
    VStudioBackend();

    func available() const -> bool override;
    func generateWorkspace(const WorkspaceRef workspace) -> bool override;
    func launchIde(const WorkspaceRef workspace) -> void override;
    func build(const WorkspaceRef workspace) -> BuildState override;
    func buildProject(const ProjectRef project) -> BuildState override;

private:
    std::filesystem::path m_compiler;
    std::filesystem::path m_linker;
    std::filesystem::path m_lib;
    std::vector<std::filesystem::path> m_includePaths;
    std::vector<std::filesystem::path> m_libPaths;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

