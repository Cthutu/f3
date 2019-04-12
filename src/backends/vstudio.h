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

private:
    // Returns <includeApiFolder?, includeTestFolder?>
    func whichFolders(const Project* proj) -> std::tuple<bool, bool>;

    func generateSln(const WorkspaceRef ws) -> bool;
    func generatePrjs(const WorkspaceRef ws) -> bool;
    func generatePrj(const ProjectRef proj) -> bool;
    func generateFilters(const ProjectRef proj) -> bool;

    func getProjectType(const ProjectRef proj) -> std::string;
    func getProjectExt(const ProjectRef proj) -> std::string;
    func getIncludePaths(const Project* proj) -> std::vector<std::string>;
    func getLibraries(const Project* proj) -> std::vector<std::string>;
    func getLibraryPaths(const Project* proj, BuildType buildType) -> std::vector<std::string>;

    func buildPchFiles(const Project* proj, BuildType buildType) -> bool;
    func buildDataFiles(const Project* proj, BuildType buildType) -> std::optional<std::vector<std::filesystem::path>>;

private:
    std::filesystem::path m_compiler;
    std::filesystem::path m_linker;
    std::filesystem::path m_lib;
    std::vector<std::filesystem::path> m_includePaths;
    std::vector<std::filesystem::path> m_libPaths;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

