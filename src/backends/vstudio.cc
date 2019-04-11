//----------------------------------------------------------------------------------------------------------------------
// Visual Studio 2017 backend implementation
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/vstudio.h>
#include <data/geninfo.h>
#include <data/workspace.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <utils/lines.h>
#include <utils/process.h>
#include <utils/regkey.h>
#include <utils/msg.h>
#include <utils/utils.h>
#include <utils/xml.h>

#if !OS_WIN32
#   error Do not use this file for non-Windows builds
#endif

#include <Windows.h>

using namespace std;
namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// Visual Studio info

struct VSInfo
{
    fs::path            vsWherePath;    // Location of vswhere.exe
    fs::path            installPath;    // Base path of Visual Studio
    string              version;        // Version of compiler, e.g. "14.14.26428"
    string              vsVersion;      // Version of visual studio, e.g. "15.7.27703.2047"
    fs::path            toolsPath;      // Path to find cl.exe and link.exe
    vector<fs::path>    includePaths;   // Array of include paths to compile with.
    vector<fs::path>    libPaths;       // Array of library paths to find libraries to link against.
};

//----------------------------------------------------------------------------------------------------------------------
// getVsInfo

func getVsInfo() -> optional<VSInfo>
{
    VSInfo vi;

    // Step 1: Locate vswhere.
    char* buffer = new char[32767];
    if (!GetEnvironmentVariableA("ProgramFiles(x86)", buffer, 32767))
    {
        return {};
    }
    fs::path programFilesPath = buffer;
    delete[] buffer;
    vi.vsWherePath = programFilesPath / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";

    // Step 2: Run vswhere to locate the folder of Visual Studio C++ compiler
    Lines vsWhereLines;
    Process vsWhere(vi.vsWherePath.string(),
        {
            "-latest",
            "-products", "*",
            "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property", "installationPath"
        },
        fs::current_path(),
        [&vsWhereLines](const char* buffer, size_t len) {
        vsWhereLines.feed(buffer, len);
    });
    int vsWhereResult = vsWhere.get();
    vsWhereLines.generate();
    vi.installPath = vsWhereLines[0];

    // Step 3: Extract the version number
    ifstream versionTxt;
    versionTxt.open(vi.installPath / "VC" / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt");
    if (versionTxt.is_open())
    {
        getline(versionTxt, vi.version);
        versionTxt.close();
    }
    else
    {
        return {};
    }

    vi.toolsPath = vi.installPath / "VC" / "Tools" / "MSVC" / vi.version / "bin" / "HostX64" / "x64";

    // Step 4: Discover Visual Studio version.
    vsWhereLines.clear();
    Process vsWhere2(vi.vsWherePath.string(),
        {
            "-latest",
            "-property", "installationVersion",
        },
        fs::current_path(),
        [&vsWhereLines](const char* buffer, size_t len) {
        vsWhereLines.feed(buffer, len);
    });
    vsWhere2.get();
    vsWhereLines.generate();
    vi.vsVersion = vsWhereLines[0];

    //
    // Include paths
    //

    // #todo: Extract 14.0 from the vsVersion string.
    vi.includePaths.emplace_back(vi.installPath / "VC" / "Tools" / "MSVC" / vi.version / "include");

    auto installationFolder =
        RegKey(RegistryKey::LocalMachine, "SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0",
            "InstallationFolder").get();
    auto sdkVersion =
        RegKey(RegistryKey::LocalMachine, "SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0",
            "ProductVersion").get();
    fs::path includePath = fs::path(installationFolder) / "Include" / (sdkVersion + ".0");
    fs::path libPath = fs::path(installationFolder) / "Lib" / (sdkVersion + ".0");

    vi.includePaths.emplace_back(includePath / "ucrt");
    vi.includePaths.emplace_back(includePath / "um");
    vi.includePaths.emplace_back(includePath / "shared");

    //
    // Library paths
    //

    vi.libPaths.emplace_back(vi.installPath / "VC" / "Tools" / "MSVC" / vi.version / "lib" / "x64");
    vi.libPaths.emplace_back(libPath / "ucrt" / "x64");
    vi.libPaths.emplace_back(libPath / "um" / "x64");

    return vi;
}

//----------------------------------------------------------------------------------------------------------------------
//
func VStudioBackend::generateSln(const WorkspaceRef ws) -> bool
{
    auto projPath = ws->rootPath/ "_make";
    if (!ensurePath(ws->projects.back()->env.cmdLine, fs::path(projPath)))
    {
        error(ws->projects.back()->env.cmdLine, stringFormat("Unable to create folder `{0}`.", projPath));
        return false;
    }

    fs::path slnPath = projPath / (ws->projects.back()->name + ".sln");
    msg(ws->projects.back()->env.cmdLine, "Generating", stringFormat("Building solution: `{0}`.", slnPath.string()));

    optional<VSInfo> vi = getVsInfo();
    if (!vi)
    {
        error(ws->projects.back()->env.cmdLine, "Unable to locate compiler.");
        return false;
    }

    TextFile f { fs::path(slnPath) };
    f
        << "Microsoft Visual Studio Solution File, Format Version 12.00"
        << "# Visual Studio 15"
        << stringFormat("VisualStudioVersion = {0}", vi->vsVersion)
        << "MinimumVisualStudioVersion = 10.0.40219.1";

    auto writeProj = [&f, &ws](const ProjectRef proj) {
        fs::path relativePath = fs::relative(proj->rootPath / "_make", ws->rootPath / "_make") / stringFormat("{0}.vcxproj", proj->name);

        f << stringFormat("Project(\"{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}\") = \"{0}\", \"{1}\", \"{2}\"",
            proj->name, relativePath.string(), proj->guid);

        if (!proj->deps.empty())
        {
            f << "\tProjectSection(ProjectDependencies) = postProject";
            for (const auto& dep : proj->deps)
            {
                f << stringFormat("\t\t{0} = {0}", dep.proj->guid);
            }
            f << "\tEndProjectSection";
        }

        f << "EndProject";
    };

    writeProj(ws->projects.back());
    for (auto& proj : ws->projects)
    {
        if (proj == ws->projects.back()) continue;
        writeProj(proj);
    }

    f
        << "Global"

        << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution"
        << "\t\tDebug|x64 = Debug|x64"
        << "\t\tRelease|x64 = Release|x64"
        << "\tEndGlobalSection"

        << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution";

    for (auto& proj : ws->projects)
    {
        f
            << stringFormat("\t\t{0}.Debug|x64.ActiveCfg = Debug|x64", proj->guid)
            << stringFormat("\t\t{0}.Debug|x64.Build.0 = Debug|x64", proj->guid)
            << stringFormat("\t\t{0}.Release|x64.ActiveCfg = Release|x64", proj->guid)
            << stringFormat("\t\t{0}.Release|x64.Build.0 = Release|x64", proj->guid);
    }

    f
        << "\tEndGlobalSection"

        << "\tGlobalSection(SolutionProperties) = preSolution"
        << "\t\tHideSolutionNode = FALSE"
        << "\tEndGlobalSection"

        << "\tGlobalSection(ExtensibilityGlobals) = postSolution"
        << stringFormat("\t\tSolutionGuid = {0}", ws->guid)
        << "\tEndGlobalSection"

        << "EndGlobal";

    if (f.write())
    {
        return true;
    }
    else
    {
        error(ws->projects.back()->env.cmdLine, stringFormat("Cannot create solution file `{0}`", slnPath));
        try
        {
            fs::remove_all(projPath);
        }
        catch (...)
        {

        }
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// generatePrjs

func VStudioBackend::generatePrjs(const WorkspaceRef ws) -> bool
{
    for (auto& proj : ws->projects)
    {
        if (!generatePrj(proj)) return false;
        if (!generateFilters(proj)) return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// getProjectType
// Returns the string used in a project file for representing the application type.

func VStudioBackend::getProjectType(const ProjectRef proj) -> string
{
    switch (proj->appType)
    {
    case AppType::Exe:
        return "Application";

    case AppType::Library:
        return "StaticLibrary";

    case AppType::DynamicLibrary:
        return "DynamicLibrary";
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------
// getProjectExt
// Returns the filename extension for the project's binary.

func VStudioBackend::getProjectExt(const ProjectRef proj) -> string
{
    switch (proj->appType)
    {
    case AppType::Exe:
        return ".exe";

    case AppType::Library:
        return ".lib";

    case AppType::DynamicLibrary:
        return ".dll";
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------
// getIncludePaths

func VStudioBackend::getIncludePaths(const Project* proj) -> vector<string>
{
    auto projPath = proj->rootPath / "_make";

    vector<fs::path> incPaths;

    //
    // Add dependency paths
    //

    set<Project *> deps = getProjectCompleteDeps(proj);
    for (const Project* proj : deps)
    {
        incPaths.emplace_back(fs::canonical(proj->rootPath / "inc"));
        //incPaths.emplace_back(fs::relative(proj->rootPath / "inc", projPath));
    }

    //
    // Add paths
    //
    incPaths.emplace_back(fs::canonical(proj->rootPath / "src"));
    //incPaths.emplace_back(fs::relative(proj->rootPath / "src", projPath));

    if (proj->appType == AppType::Library || proj->appType == AppType::DynamicLibrary)
    {
        incPaths.emplace_back(fs::canonical(proj->rootPath / "inc"));
        //incPaths.emplace_back(fs::relative(proj->rootPath / "inc", projPath));
    }

    //
    // Convert to single semi-colon delimited string
    //

    vector<string> strings;
    transform(incPaths.begin(), incPaths.end(), back_inserter(strings), 
        [](const fs::path& path) -> string { return path.string(); });
    return strings;
}

//----------------------------------------------------------------------------------------------------------------------
// getLibraries

func VStudioBackend::getLibraries(const Project* proj) -> vector<string>
{
    auto projPath = proj->rootPath / "_make";

    vector<string> libs;

    set<Project*> deps = getProjectCompleteDeps(proj);
    for (const Project* proj : deps)
    {
        libs.emplace_back(proj->name + ".lib");
    }

    return libs;
}

//----------------------------------------------------------------------------------------------------------------------
// getLibraryPaths

func VStudioBackend::getLibraryPaths(const Project* proj, BuildType buildType) -> vector<string>
{
    auto projPath = proj->rootPath / "_make";
    string buildString = buildType == BuildType::Debug ? "debug" : "release";
    
    vector<fs::path> libPaths;

    set<Project*> deps = getProjectCompleteDeps(proj);
    for (const Project* proj : deps)
    {
        libPaths.emplace_back(fs::relative(proj->rootPath / "_bin" / buildString, projPath));
    }

    vector<string> libPathStrings;
    transform(libPaths.begin(), libPaths.end(), back_inserter(libPathStrings),
        [](const fs::path& path) -> string { return path.string(); });

    return libPathStrings;
}

//----------------------------------------------------------------------------------------------------------------------
// generatePrj

func VStudioBackend::generatePrj(const ProjectRef proj) -> bool
{
    Env& env = proj->env;

    XmlNode rootNode;

    auto vs = *getVsInfo();
    vector<string> versions = split(vs.vsVersion, ".");
    auto projPath = proj->rootPath / "_make";
    if (!ensurePath(env.cmdLine, fs::path(projPath))) return false;

    string includeDirectories = join(getIncludePaths(proj.get()), ";") + ";" + join(vs.includePaths, ";");

    XmlNode* includeGroup = nullptr;
    XmlNode* compileGroup = nullptr;

    buildDataFiles(proj.get(), env.buildType);
    
    rootNode
        .tag("Project", { { "DefaultTargets", "Build" }, { "ToolsVersion", "15.0" }, 
                          { "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003" } })
            .tag("ItemGroup", { { "Label", "ProjectConfigurations" } })
                .tag("ProjectConfiguration", { { "Include", "Debug|x64" } })
                    .text("Configuration", {}, "Debug")
                    .text("Platform", {}, "x64")
                .end()
                .tag("ProjectConfiguration", { { "Include", "Release|x64" } })
                    .text("Configuration", {}, "Release")
                    .text("Platform", {}, "x64")
                .end()
            .end()
            .tag("PropertyGroup", { {"Label", "Globals"} })
                .text("VCProjectVersion", {}, versions[0] + ".0")
                .text("ProjectGuid", {}, string(proj->guid))
                .text("Keyword", {}, "Win32Proj")
                .text("RootNamespace", {}, string(proj->name))
                .text("WindowsTargetPlatformVersion", {}, "10.0.17134.0")
            .end()
            .tag("Import", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"}})
            .end()
            .tag("PropertyGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='Debug|x64'"}, {"Label", "Configuration"}})
                .text("ConfigurationType", {}, getProjectType(proj))
                .text("UseDebugLibraries", {}, "true")
                .text("PlatformToolset", {}, "v141")
                .text("CharacterSet", {}, "MultiByte")
            .end()
            .tag("PropertyGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='Release|x64'"}, {"Label", "Configuration"}})
                .text("ConfigurationType", {}, getProjectType(proj))
                .text("UseDebugLibraries", {}, "false")
                .text("PlatformToolset", {}, "v141")
                .text("WholeProgramOptimization", {}, "true")
                .text("CharacterSet", {}, "MultiByte")
            .end()
            .tag("Import", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.props"}})
            .end()
            .tag("ImportGroup", {{"Label", "ExtensionSettings"}})
            .end()
            .tag("ImportGroup", {{"Label", "Shared"}})
            .end()
            .tag("ImportGroup", {{"Label", "PropertySheets"}, {"Condition", "'$(Configuration)|$(Platform)'=='Debug|x64'"}})
                .tag("Import", {{"Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props"}, 
                                {"Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')"},
                                {"Label", "LocalAppDataPlatform"}})
                .end()
            .end()
            .tag("ImportGroup", {{"Label", "PropertySheets"}, {"Condition", "'$(Configuration)|$(Platform)'=='Release|x64'"}})
                .tag("Import", {{"Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props"}, 
                                {"Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')"},
                                {"Label", "LocalAppDataPlatform"}})
                .end()
            .end()
            .tag("PropertyGroup", {{"Label", "UserMacros"}})
            .end()
            .tag("PropertyGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='Debug|x64'"}})
                .text("LinkIncremental", {}, "true")
                .text("OutDir", {}, "..\\_bin\\debug\\")
                .text("IntDir", {}, "..\\_obj\\debug\\")
                .text("TargetName", {}, string(proj->name))
                .text("TargetExt", {}, getProjectExt(proj))
            .end()
            .tag("PropertyGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='Release|x64'"}})
                .text("LinkIncremental", {}, "false")
                .text("OutDir", {}, "..\\_bin\\release\\")
                .text("IntDir", {}, "..\\_obj\\release\\")
                .text("TargetName", {}, string(proj->name))
                .text("TargetExt", {}, getProjectExt(proj))
            .end()
            .tag("ItemDefinitionGroup", { {"Condition", "'$(Configuration)|$(Platform)'=='Debug|x64'"}})
                .tag("ClCompile", {})
                    .text("PrecompiledHeader", {}, "NotUsing")
                    .text("WarningLevel", {}, "Level3")
                    .text("TreatWarningAsError", {}, "true")
                    .text("PreprocessorDefinitions", {}, "_CRT_SECURE_NO_WARNINGS;_DEBUG;WIN32;%(PreprocessorDefinitions)")      // #todo: Support non-console apps, libs, and dlls
                    .text("AdditionalIncludeDirectories", {}, string(includeDirectories))
                    .text("Optimization", {}, "Disabled")
                    .text("RuntimeLibrary", {}, "MultiThreadedDebug")
                    .text("RuntimeTypeInfo", {}, "false")
                    .text("AdditionalOptions", {}, "/std:c++17 %(AdditionalOptions)")
                .end()
                .tag("Link", {})
                    .text("Subsystem", {}, proj->ssType == SubsystemType::Console ? "Console" : "Windows")
                    .text("GenerateDebugInformation", {}, "true")
                    .text("TreatLinkerWarningAsErrors", {}, "true")
                    .text("AdditionalOptions", {}, "/DEBUG:FULL %(AdditionalOptions)")
                    .text("AdditionalDependencies", {}, join(getLibraries(proj.get()), ";") + ";%(AdditionalDependencies)")
                    .text("AdditionalLibraryDirectories", {}, join(getLibraryPaths(proj.get(), BuildType::Debug), ";") + ";%(AdditionalLibraryDirectories)")
                .end()
            .end()
            .tag("ItemDefinitionGroup", { {"Condition", "'$(Configuration)|$(Platform)'=='Release|x64'"}})
                .tag("ClCompile", {})
                    .text("PrecompiledHeader", {}, "NotUsing")
                    .text("WarningLevel", {}, "Level3")
                    .text("TreatWarningAsError", {}, "true")
                    .text("PreprocessorDefinitions", {}, "_CRT_SECURE_NO_WARNINGS;NDEBUG;WIN32;%(PreprocessorDefinitions)")      // #todo: Support non-console apps, libs, and dlls
                    .text("AdditionalIncludeDirectories", {}, string(includeDirectories))
                    .text("Optimization", {}, "Full")
                    .text("FunctionLevelLinking", {}, "true")
                    .text("IntrinsicFunctions", {}, "true")
                    .text("MinimumRebuild", {}, "false")
                    .text("RuntimeLibrary", {}, "MultiThreaded")
                    .text("RuntimeTypeInfo", {}, "false")
                    .text("AdditionalOptions", {}, "/std:c++17 %(AdditionalOptions)")
                .end()
                .tag("Link", {})
                    .text("Subsystem", {}, proj->ssType == SubsystemType::Console ? "Console" : "Windows")
                    .text("EnableCOMDATFolding", {}, "true")
                    .text("OptimizeReferences", {}, "true")
                    .text("GenerateDebugInformation", {}, "true")
                    .text("TreatLinkerWarningAsErrors", {}, "true")
                    .text("AdditionalDependencies", {}, join(getLibraries(proj.get()), ";") + ";%(AdditionalDependencies)")
                    .text("AdditionalLibraryDirectories", {}, join(getLibraryPaths(proj.get(), BuildType::Release), ";") + ";%(AdditionalLibraryDirectories)")
                .end()
            .end()
            .tag("ItemGroup", {}, &includeGroup)
            .end()
            .tag("ItemGroup", {}, &compileGroup)
            .end()
            .tag("ItemGroup", {})
                .tag("None", {{"Include", "..\\forge.ini"}})
                .end()
            .end()
            .tag("Import", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets"}})
            .end()
            .tag("ImportGroup", {{"Label", "ExtensionTargets"}})
            .end()
        .end();

    auto [includeApiFolder, includeTestFolder] = whichFolders(proj.get());

    function<void (const unique_ptr<Node>&)> genLinks = 
        [
            includeGroup, 
            compileGroup, 
            &projPath, 
            &genLinks,
            includeTestFolder, 
            includeApiFolder,
            &env,
            &proj
        ]
    (const unique_ptr<Node>& node) {
        switch (node->type)
        {
        case Node::Type::SourceFile:
            {
                fs::path srcPath = fs::relative(node->fullPath, projPath);
                compileGroup->tag("ClCompile", {{"Include", srcPath.string()}}).end();
            }
            break;

        case Node::Type::HeaderFile:
            {
                fs::path srcPath = fs::relative(node->fullPath, projPath);
                if (srcPath.extension() == ".h" || srcPath.extension() == ".hpp")
                includeGroup->tag("ClInclude", {{"Include", srcPath.string()}}).end();
            }
            break;

        case Node::Type::DataFile:
            {
                fs::path relPath = fs::relative(node->fullPath, proj->rootPath);
                fs::path buildTypeFolder = env.buildType == BuildType::Debug ? "debug" : "release";
                fs::path dataPath = fs::relative(proj->rootPath / "_obj" / buildTypeFolder / relPath, projPath);
                dataPath.replace_extension(dataPath.extension().string() + ".cc");
                compileGroup->tag("ClCompile", { {"Include", dataPath.string()} }).end();
            }
            break;

        case Node::Type::ApiFolder:
        case Node::Type::TestFolder:
        case Node::Type::SourceFolder:
        case Node::Type::DataFolder:
        case Node::Type::Root:
            if (node->type == Node::Type::ApiFolder && !includeApiFolder) break;
            if (node->type == Node::Type::TestFolder && !includeTestFolder) break;
            for (auto& subNode : node->nodes)
            {
                genLinks(subNode);
            }
            break;
        }
    };
    genLinks(proj->rootNode);

    fs::path prjPath = projPath / (proj->name + ".vcxproj");
    msg(env.cmdLine, "Generating", stringFormat("Building project: `{0}`.", prjPath.string()));

    ofstream f;
    f.open(prjPath, ios::trunc);
    if (f.is_open())
    {
        f << rootNode.generate();
        f.close();
    }
    else
    {
        error(env.cmdLine, stringFormat("Unable to create file `{0}`.", prjPath.string()));
        return false;
    }
                
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// generateFilters

func VStudioBackend::generateFilters(const ProjectRef proj) -> bool
{
    Env& env = proj->env;

    XmlNode rootNode;
    XmlNode* foldersNode = nullptr;
    XmlNode* includesNode = nullptr;
    XmlNode* compilesNode = nullptr;

    rootNode
        .tag("Project", {{"ToolsVersion", "4.0"}, {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}})
            .tag("ItemGroup", {}, &foldersNode)
            .end()
            .tag("ItemGroup", {}, &includesNode)
            .end()
            .tag("ItemGroup", {}, &compilesNode)
            .end()
        .end();

    //
    // Process the folders, headers and source files
    //

    auto projPath = env.rootPath / "_make";

    auto[includeApiFolder, includeTestFolder] = whichFolders(proj.get());

    function<void(const unique_ptr<Node>&)> genFolders = 
        [
            includesNode, 
            compilesNode, 
            foldersNode, 
            &proj, 
            &env, 
            &projPath, 
            &genFolders, 
            includeTestFolder, 
            includeApiFolder
        ]
    (const unique_ptr<Node>& node)
    {
        switch (node->type)
        {
        case Node::Type::SourceFile:
            {
                auto path = fs::relative(node->fullPath, projPath);
                compilesNode->tag("ClCompile", { {"Include", path.string()} })
                    .text("Filter", {}, fs::relative(node->fullPath.parent_path(), env.rootPath).string())
                    .end();
            }
            break;

        case Node::Type::HeaderFile:
            {
                auto path = fs::relative(node->fullPath, projPath);
                includesNode->tag("ClInclude", { {"Include", path.string()} })
                    .text("Filter", {}, fs::relative(node->fullPath.parent_path(), env.rootPath).string())
                    .end();
            }
            break;

        case Node::Type::DataFile:
            {
                fs::path relPath = fs::relative(node->fullPath, proj->rootPath);
                fs::path buildTypeFolder = env.buildType == BuildType::Debug ? "debug" : "release";
                fs::path dataPath = fs::relative(proj->rootPath / "_obj" / buildTypeFolder / relPath, projPath);
                dataPath.replace_extension(dataPath.extension().string() + ".cc");
                includesNode->tag("ClCompile", { {"Include", dataPath.string()} })
                    .text("Filter", {}, fs::relative(node->fullPath.parent_path(), env.rootPath).string())
                    .end();
            }
            break;

        case Node::Type::ApiFolder:
        case Node::Type::TestFolder:
        case Node::Type::SourceFolder:
        case Node::Type::DataFolder:
            if (node->type == Node::Type::ApiFolder && !includeApiFolder) break;
            if (node->type == Node::Type::TestFolder && !includeTestFolder) break;
            {
                fs::path folderPath = fs::relative(node->fullPath, env.rootPath);
                foldersNode->tag("Filter", { {"Include", string(folderPath.string())} })
                    .text("UniqueIdentifier", {}, generateGuid())
                    .end();
            }
            [[fallthrough]];

        case Node::Type::Root:
            for (auto& subNode : node->nodes)
            {
                genFolders(subNode);
            }
            break;
        }
    };
    genFolders(proj->rootNode);

    fs::path filtersPath = projPath / (proj->name + ".vcxproj.filters");
    msg(env.cmdLine, "Generating", stringFormat("Building filters: `{0}`.", filtersPath.string()));

    ofstream f;
    f.open(filtersPath, ios::trunc);
    if (f.is_open())
    {
        f << rootNode.generate();
        f.close();
    }
    else
    {
        error(env.cmdLine, stringFormat("Unable to create file `{0}`.", filtersPath.string()));
        return false;
    }
                
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Constructor

VStudioBackend::VStudioBackend()
{
    auto vi = getVsInfo();
    if (vi)
    {
        fs::path compilerPath = vi->toolsPath / "cl.exe";
        fs::path linkerPath = vi->toolsPath / "link.exe";
        fs::path libPath = vi->toolsPath / "lib.exe";

        if (fs::exists(compilerPath) &&
            fs::exists(linkerPath) &&
            fs::exists(libPath))
        {
            m_compiler = compilerPath;
            m_linker = linkerPath;
            m_lib = libPath;
            m_includePaths = vi->includePaths;
            m_libPaths = vi->libPaths;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// whichFolders

func VStudioBackend::whichFolders(const Project* proj) -> tuple<bool, bool>
{
    return {
        (proj->appType == AppType::Library || proj->appType == AppType::DynamicLibrary),
        false
    };
}

//----------------------------------------------------------------------------------------------------------------------
// available

func VStudioBackend::available() const -> bool
{
    auto vs = getVsInfo();
    return bool(vs);
}

//----------------------------------------------------------------------------------------------------------------------
// generateWorkspace

func VStudioBackend::generateWorkspace(const WorkspaceRef workspace) -> bool
{
    if (!generateSln(workspace)) return false;
    if (!generatePrjs(workspace)) return false;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// launchIde

func VStudioBackend::launchIde(const WorkspaceRef workspace) -> void
{
    ProjectRef mainProject = workspace->projects.back();

    auto vs = getVsInfo();
    if (vs)
    {
        auto projPath = workspace->rootPath / "_make";
        Process ide((vs->installPath / "Common7" / "IDE" / "devenv.exe").string(),
            {
                (projPath / (mainProject->name + ".sln")).string()
            });
    }
}

//----------------------------------------------------------------------------------------------------------------------

func VStudioBackend::buildDataFiles(const Project* proj, BuildType buildType) -> optional<vector<fs::path>>
{
    vector<fs::path> paths;
    fs::path buildTypeFolder = buildType == BuildType::Debug ? "debug" : "release";
    function<bool(const unique_ptr<Node>&)> buildData =
        [
            &buildData, 
            proj, 
            &buildTypeFolder,
            &paths
        ]
    (const unique_ptr<Node>& node)
    {
        switch (node->type)
        {
        case Node::Type::ApiFolder:
        case Node::Type::TestFolder:
        case Node::Type::SourceFolder:
        case Node::Type::DataFolder:
        case Node::Type::Root:
            for (auto& subNode : node->nodes)
            {
                if (!buildData(subNode)) return false;
            }
            break;

        case Node::Type::DataFile:
            {
                fs::path relPath = fs::relative(node->fullPath, proj->rootPath);
                fs::path srcPath = node->fullPath;

                fs::path dataPath = proj->rootPath / "_obj" / buildTypeFolder / relPath;
                dataPath.replace_extension(dataPath.extension().string() + ".cc");
                if (!ensurePath(proj->env.cmdLine, fs::path(dataPath.parent_path()))) return false;

                if (!fs::exists(dataPath) || (fs::last_write_time(srcPath) > fs::last_write_time(dataPath)))
                {
                    // We need to generate the C++ files from the file pointed to by srcPath.
                            // We need to generate the C++ file from the file pointed to by srcPath.
                    string name = symbolise(relPath.string());
                    msg(proj->env.cmdLine, "Data", stringFormat("Generating data ({0}).", name));

                    TextFile f{ fs::path(dataPath) };
                    f
                        << "// Data file generated by Forge."
                        << "//"
                        << (string("// Source: ") + relPath.string())
                        << ""
                        << "#include <cstdint>"
                        << ""
                        << stringFormat("extern const uint8_t {0}[];", name)
                        << stringFormat("extern const uint64_t size_{0};", name)
                        << ""
                        << stringFormat("const uint64_t size_{0} = {1};", name, fs::file_size(srcPath))
                        << stringFormat("const uint8_t {0}[] = ", name)
                        << "{";

                    ifstream dataFile{ srcPath, ios::binary | ios::in };
                    if (dataFile.is_open())
                    {
                        istreambuf_iterator<char> dataStream(dataFile), endDataStream;
                        vector<char> data(dataStream, endDataStream);
                        dataFile.close();

                        for (size_t i = 0; i < data.size(); ++i)
                        {
                            string rowStr = "    ";
                            for (size_t row = i; row < min(data.size(), 16); ++row, ++i)
                            {
                                rowStr += string("0x") + byteHexStr((u8)data[i]) + ", ";
                            }
                            f << move(rowStr);
                        }
                    }
                    else
                    {
                        return error(proj->env.cmdLine, stringFormat("Unable to read data file `{0}`.", dataPath.string()));
                    }

                    f << "};";

                    // Generate the C++ symbol name from the path

                    if (!f.write())
                    {
                        return error(proj->env.cmdLine, stringFormat("Unable to generate data file `{0}`.", dataPath.string()));
                    }
                    paths.push_back(dataPath);
                }
            }
            break;

        default:
            return true;
        }

        return true;
    };

    if (!buildData(proj->rootNode)) return {};

    return paths;
}

//----------------------------------------------------------------------------------------------------------------------

func VStudioBackend::buildPchFiles(const Project* proj, BuildType buildType) -> void
{

}

//----------------------------------------------------------------------------------------------------------------------

func VStudioBackend::build(const WorkspaceRef workspace) -> BuildState
{
    ProjectRef proj = workspace->projects.back();
    if (proj->appType == AppType::DynamicLibrary)
    {
        error(proj->env.cmdLine, "DLL support is unimplemented.");
        return BuildState::Failed;
    }

    fs::path buildTypeFolder = (proj->env.buildType == BuildType::Release) ? "release" : "debug";
    int numCompiledFiles = 0;

    //
    // Step 1 - Determine build order
    //
    vector<const Project*> projects;
    function<void(const Project *)> gatherDeps = [&gatherDeps, &projects](const Project* proj)
    {
        if (find(projects.begin(), projects.end(), proj) == projects.end())
        {
            for (const auto& dep : proj->deps)
            {
                if (find(projects.begin(), projects.end(), dep.proj) == projects.end())
                {
                    gatherDeps(dep.proj);
                }
            }

            projects.push_back(proj);
        }
    };
    gatherDeps(proj.get());

    //
    // Step 2 - Build each project
    //

    for (const auto& proj : projects)
    {
        auto[includeApiFolder, includeTestFolder] = whichFolders(proj);
        bool usePch = false;
        optional<string> pchFile;
        msg(proj->env.cmdLine, "Building", stringFormat("Building project `{0}`...", proj->name));
        vector<string> objs;

        auto dataFiles = buildDataFiles(proj, proj->env.buildType);
        if (!dataFiles) return BuildState::Failed;

        function<bool(const unique_ptr<Node>&)> buildNodes =
            [this, &buildNodes, &numCompiledFiles, &proj, &buildTypeFolder, &usePch, &pchFile,
            &includeApiFolder, &includeTestFolder, &objs]
        (const unique_ptr<Node>& node) -> bool
        {
            switch(node->type)
            {
            case Node::Type::ApiFolder:
            case Node::Type::TestFolder:
            case Node::Type::SourceFolder:
            case Node::Type::DataFolder:
            case Node::Type::Root:
                if (node->type == Node::Type::ApiFolder && !includeApiFolder) return true;
                if (node->type == Node::Type::TestFolder && !includeTestFolder) return true;

                for (auto& subNode : node->nodes)
                {
                    if (!buildNodes(subNode)) return false;
                }
                break;

            case Node::Type::HeaderFile:
                break;

            case Node::Type::SourceFile:
            case Node::Type::PchFile:
            case Node::Type::DataFile:
                {
                    fs::path relPath = fs::relative(node->fullPath, proj->rootPath);
                    fs::path srcPath = node->fullPath;
                    fs::path objPath = proj->rootPath / "_obj" / buildTypeFolder / relPath;

                    if (node->type == Node::Type::DataFile)
                    {
                        srcPath = objPath;
                        srcPath.replace_extension(srcPath.extension().string() + ".cc");
                        objPath.replace_extension(objPath.extension().string() + ".obj");
                    }
                    else
                    {
                        objPath.replace_extension(".obj");
                    }

                    objs.push_back(objPath.string());

                    bool build = false;
                    if (!fs::exists(objPath)) build = true;
                    else
                    {
                        auto ts = fs::last_write_time(srcPath);
                        auto to = fs::last_write_time(objPath);

                        if (ts > to) build = true;
                        else
                        {
                            // Check dependencies

                            // #todo: Move this to workspace building?
                            scanDependencies(proj, node);

                            for (auto& srcDep : node->deps)
                            {
                                auto ts = fs::last_write_time(srcDep);
                                if (ts > to)
                                {
                                    build = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (build)
                    {
                        if (!ensurePath(proj->env.cmdLine, objPath.parent_path()))
                        {
                            return error(proj->env.cmdLine, stringFormat("Unable to create folder `{0}`.", objPath.parent_path().string()));
                        }

                        string cmd = m_compiler.string();
                        Lines errorLines;

                        vector<string> args = {
                            "/nologo",
                            "/EHsc",
                            "/c",
                            "/Zi",
                            "/W3",
                            "/WX",
                            proj->env.buildType == BuildType::Release ? "/MT" : "/MTd",
                            "/std:c++17",
                            "/Fd\"" + (proj->env.rootPath / "_obj" / buildTypeFolder / "vc141.pdb").string() + "\"",
                            "/Fo\"" + objPath.string() + "\"",
                            "\"" + srcPath.string() + "\"",
                            //"/I\"" + (env.rootPath / "src").string() + "\""
                        };

                        vector<string> incPaths = getIncludePaths(proj);
                        for (const auto& path : incPaths)
                        {
                            args.emplace_back(string("/I\"") + path + "\"");
                        }

                        // Check for pre-compiled header
                        if (usePch)
                        {
                            string flag = node->type == Node::Type::PchFile ? "/Yc" : "/Yu";
                            args.emplace_back(flag + *pchFile);
                            auto pchPath = proj->rootPath / "_obj" / buildTypeFolder / (proj->name + ".pch");
                            args.emplace_back(string("/Fp") + pchPath.string());
                        }

                        // Add the compiler's standard include paths.
                        for (const auto& path : m_includePaths)
                        {
                            args.emplace_back(string("/I\"") + path.string() + "\"");
                        }

                        // Check for verbosity.
                        if (proj->env.cmdLine.flag("v") || proj->env.cmdLine.flag("verbose"))
                        {
                            string line = cmd;
                            for (const auto& arg : args)
                            {
                                line += " " + arg;
                            }
                            msg(proj->env.cmdLine, "Running", line);
                        }

                        msg(proj->env.cmdLine, "Compiling", srcPath.string());
                        Process p(move(cmd), move(args), fs::current_path(),
                            [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); },
                            [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); });

                        if (p.get())
                        {
                            // Non-zero result means a failed compilation.
                            error(proj->env.cmdLine, stringFormat("Compilation of `{0}` failed.", srcPath.string()));
                            errorLines.generate();
                            for (const auto& line : errorLines)
                            {
                                cout << line << endl;
                            }
                            return false;
                        }

                        ++numCompiledFiles;
                    } // if (build)
                }
                break;
            } // switch

            return true;
        };

        //
        // Pre-compiled header
        //

        pchFile = proj->config.tryGet("build.pch");
        if (pchFile)
        {
            usePch = true;

            // Figure out if we need to rebuild the pch.cc file.
            fs::path pchPath = proj->env.rootPath / "_obj" / buildTypeFolder / "pch.cc";
            bool createPch = false;

            // If the file doesn't exist, it's obvious that we need to rebuild it.
            if (!fs::exists(pchPath))
            {
                createPch = true;
            }
            else
            {
                createPch = fs::last_write_time(proj->rootPath / "forge.ini") > fs::last_write_time(pchPath);
            }

            if (createPch)
            {
                if (!ensurePath(proj->env.cmdLine, proj->rootPath / "_obj" / buildTypeFolder)) return BuildState::Failed;
                TextFile pchTextFile{ fs::path(pchPath) };
                pchTextFile << (string("#include <") + *pchFile + ">\n");
                pchTextFile.write();
            }

            auto node = make_unique<Node>(Node::Type::PchFile, move(pchPath));
            if (!buildNodes(node))
            {
                return BuildState::Failed;
            }
        }

        //
        // Build all nodes
        //

        const unique_ptr<Node>& rootNode = proj->rootNode;
        if (!buildNodes(rootNode))
        {
            return BuildState::Failed;
        }

        //
        // Linking or library production
        // #todo: Support DLLs
        //
        Lines errorLines;
        fs::path binPath = proj->rootPath / "_bin" / buildTypeFolder;
        string ext;

        switch (proj->appType)
        {
        case AppType::Exe:              ext = ".exe"; break;
        case AppType::Library:          ext = ".lib"; break;
        case AppType::DynamicLibrary:   ext = ".dll"; break;
        default: assert(0);
        }

        fs::path outPath = binPath / (proj->name + ext);
        fs::path pdbPath = binPath / (proj->name + ".pdb");

        if (!fs::exists(outPath) || (numCompiledFiles > 0))
        {
            if (!ensurePath(proj->env.cmdLine, outPath.parent_path()))
            {
                error(proj->env.cmdLine, stringFormat("Unable to create folder `{0}`.", outPath.string()));
                return BuildState::Failed;
            }
            bool release = (proj->env.buildType == BuildType::Release);

            if (proj->appType == AppType::Exe ||
                proj->appType == AppType::DynamicLibrary)
            {
                // An executable requires link.exe
                auto cmd = m_linker.string();
                vector<string> args =
                {
                    "/nologo",
                    string("/OUT:\"") + outPath.string() + "\"",
                    "/WX",
                    release ? "/DEBUG:NONE" : "/DEBUG:FULL",
                    string("/PDB:\"") + pdbPath.string() + "\"",
                    proj->ssType == SubsystemType::Console ? "/SUBSYSTEM:CONSOLE" : "/SUBSYSTEM:WINDOWS",
                    release ? "/OPT:REF" : "",
                    release ? "/OPT:ICF" : "",
                    "/MACHINE:X64"
                };

                // Add compiler's library paths.
                // #todo: Add dependency library paths.
                for (const auto& path : getLibraryPaths(proj, proj->env.buildType))
                {
                    args.emplace_back(string("/LIBPATH:\"") + path + "\"");
                }
                for (const auto& path : m_libPaths)
                {
                    args.emplace_back(string("/LIBPATH:\"") + path.string() + "\"");
                }

                for (const auto& path : getLibraries(proj))
                {
                    args.emplace_back(string("\"") + path + "\"");
                }

                // Add compiled objects.
                for (const auto& obj : objs) 
                {
                    args.emplace_back(string("\"") + obj + "\""); 
                }

                // Add libraries mentioned in forge.ini
                string libs = proj->config.get("build.libs");
                vector<string> libsVector = split(libs, ";");
                for (const auto& lib : libsVector)
                {
                    args.emplace_back(lib + ".lib");
                }

                if (proj->env.cmdLine.flag("v") || proj->env.cmdLine.flag("verbose"))
                {
                    string line = cmd;
                    for (const auto& arg : args)
                    {
                        line += " " + arg;
                    }
                    msg(proj->env.cmdLine, "Running", line);
                }

                msg(proj->env.cmdLine, "Linking", outPath.string());
                Process p(move(cmd), move(args), fs::current_path(),
                    [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); },
                    [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); });

                if (p.get())
                {
                    error(proj->env.cmdLine, stringFormat("Linking of `{0}` failed.", outPath.string()));
                    errorLines.generate();
                    for (const auto& line : errorLines)
                    {
                        cout << line << endl;
                    }
                    return BuildState::Failed;
                }
            }
            else
            {
                // Generating a library with lib.exe
                auto cmd = m_lib.string();
                vector<string> args =
                {
                    "/NOLOGO",
                    "/WX",
                    string("/OUT:\"") + outPath.string() + "\"",
                };

                // Add compiled objects.
                for (const auto& obj : objs)
                {
                    args.emplace_back(obj);
                }

                if (proj->env.cmdLine.flag("v") || proj->env.cmdLine.flag("verbose"))
                {
                    string line = cmd;
                    for (const auto& arg : args)
                    {
                        line += " " + arg;
                    }
                    msg(proj->env.cmdLine, "Running", line);
                }

                msg(proj->env.cmdLine, "Archiving", outPath.string());
                Process p(move(cmd), move(args), fs::current_path(),
                    [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); },
                    [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); });

                if (p.get())
                {
                    error(proj->env.cmdLine, stringFormat("Creation of `{0}` failed.", outPath.string()));
                    errorLines.generate();
                    for (const auto& line : errorLines)
                    {
                        cout << line << endl;
                    }
                    return BuildState::Failed;
                }
            }
        }

    } // for each project

    return BuildState::Success;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
