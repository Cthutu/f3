//----------------------------------------------------------------------------------------------------------------------
// Visual Studio 2017 backend implementation
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/vstudio.h>
#include <data/geninfo.h>
#include <data/workspace.h>
#include <fstream>
#include <iostream>
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
func generateSln(const WorkspaceRef& ws) -> bool
{
    ProjectRef mainProject = ws->projects[0];

    Env& env = mainProject->env;

    auto projPath = ws->rootPath/ "_make";
    if (!ensurePath(env.cmdLine, fs::path(projPath)))
    {
        error(env.cmdLine, stringFormat("Unable to create folder `{0}`.", projPath));
        return false;
    }

    fs::path slnPath = projPath / (mainProject->name + ".sln");
    msg(env.cmdLine, "Generating", stringFormat("Building solution: `{0}`.", slnPath.string()));

    optional<VSInfo> vi = getVsInfo();
    if (!vi)
    {
        error(env.cmdLine, "Unable to locate compiler.");
        return false;
    }

    TextFile f { fs::path(slnPath) };
    f
        << "Microsoft Visual Studio Solution File, Format Version 12.00"
        << "# Visual Studio 15"
        << stringFormat("VisualStudioVersion = {0}", vi->vsVersion)
        << "MinimumVisualStudioVersion = 10.0.40219.1"
        << stringFormat("Project(\"{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}\") = \"{0}\", \"{0}.vcxproj\", \"{1}\"",
            mainProject->name, mainProject->guid)
        << "EndProject"

        << "Global"

        << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution"
        << "\t\tDebug|x64 = Debug|x64"
        << "\t\tRelease|x64 = Release|x64"
        << "\tEndGlobalSection"

        << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution"
        << stringFormat("\t\t{0}.Debug|x64.ActiveCfg = Debug|x64", mainProject->guid)
        << stringFormat("\t\t{0}.Debug|x64.Build.0 = Debug|x64", mainProject->guid)
        << stringFormat("\t\t{0}.Release|x64.ActiveCfg = Release|x64", mainProject->guid)
        << stringFormat("\t\t{0}.Release|x64.Build.0 = Release|x64", mainProject->guid)
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
        error(mainProject->env.cmdLine, stringFormat("Cannot create solution file `{0}`", slnPath));
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
// generatePrj

func generatePrj(const ProjectRef proj) -> bool
{
    Env& env = proj->env;

    XmlNode rootNode;

    auto vs = *getVsInfo();
    vector<string> versions = split(vs.vsVersion, ".");
    auto projPath = proj->rootPath / "_make";

    string includeDirectories = fs::relative(env.rootPath / "src", projPath).string() + ";" + join(vs.includePaths, ";");
    string libDirectories = join(vs.libPaths, ";");

    XmlNode* includeGroup = nullptr;
    XmlNode* compileGroup = nullptr;
    
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
                .text("ConfigurationType", {}, "Application")   // #todo: change for libs and dlls
                .text("UseDebugLibraries", {}, "true")
                .text("PlatformToolset", {}, "v141")
                .text("CharacterSet", {}, "MultiByte")
            .end()
            .tag("PropertyGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='Release|x64'"}, {"Label", "Configuration"}})
                .text("ConfigurationType", {}, "Application")   // #todo: change for libs and dlls
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
                .text("TargetExt", {}, ".exe")      // #todo: Support libs and dlls
            .end()
            .tag("PropertyGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='Release|x64'"}})
                .text("LinkIncremental", {}, "false")
                .text("OutDir", {}, "..\\_bin\\release\\")
                .text("IntDir", {}, "..\\_obj\\release\\")
                .text("TargetName", {}, string(proj->name))
                .text("TargetExt", {}, ".exe")      // #todo: Support libs and dlls
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
                    .text("Subsystem", {}, proj->subsystemType == SubsystemType::Console ? "Console" : "Windows")
                    .text("GenerateDebugInformation", {}, "true")
                    .text("TreatLinkerWarningAsErrors", {}, "true")
                    .text("AdditionalOptions", {}, "/DEBUG:FULL %(AdditionalOptions)")
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
                    .text("Subsystem", {}, proj->subsystemType == SubsystemType::Console ? "Console" : "Windows")
                    .text("EnableCOMDATFolding", {}, "true")
                    .text("OptimizeReferences", {}, "true")
                    .text("GenerateDebugInformation", {}, "true")
                    .text("TreatLinkerWarningAsErrors", {}, "true")
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

//     function<void (const Node*)> genLinks = [includeGroup, compileGroup, &projPath, &genLinks](const Node* node) {
//         switch (node->type())
//         {
//         case Node::Type::Cpp:
//             {
//                 fs::path srcPath = fs::relative(node->path(), projPath);
//                 compileGroup->tag("ClCompile", {{"Include", srcPath.string()}}).end();
//             }
//             break;
// 
//         case Node::Type::Header:
//             {
//                 fs::path srcPath = fs::relative(node->path(), projPath);
//                 if (srcPath.extension() == ".h" || srcPath.extension() == ".hpp")
//                 includeGroup->tag("ClInclude", {{"Include", srcPath.string()}}).end();
//             }
//             break;
// 
//         case Node::Type::Folder:
//             [[fallthrough]];
// 
//         case Node::Type::Root:
//             for (const auto& subNodes : *node)
//             {
//                 genLinks(subNodes.get());
//             }
//             break;
//         }
//     };
//     genLinks(proj->env.rootNode.get());

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

func generateFilters(const ProjectRef& proj) -> bool
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

//     function<void(const unique_ptr<Node>&)> genFolders = [includesNode, compilesNode, foldersNode, &proj, &env, &projPath, &genFolders]
//     (const unique_ptr<Node>& node) 
//     {
//         switch (node->type())
//         {
//         case Node::Type::Cpp:
//             {
//                 auto path = fs::relative(node->path(), projPath);
//                 compilesNode->tag("ClCompile", { {"Include", path.string()} })
//                     .text("Filter", {}, fs::relative(node->path().parent_path(), env->rootPath).string())
//                     .end();
//             }
//             break;
// 
//         case Node::Type::Header:
//             {
//                 auto path = fs::relative(node->path(), projPath);
//                 includesNode->tag("ClInclude", { {"Include", path.string()} })
//                     .text("Filter", {}, fs::relative(node->path().parent_path(), env->rootPath).string())
//                     .end();
//             }
//             break;
// 
//         case Node::Type::Folder:
//             {
//                 fs::path folderPath = fs::relative(node->path(), env->rootPath);
//                 foldersNode->tag("Filter", { {"Include", string(folderPath.string())} })
//                     .text("UniqueIdentifier", {}, generateGuid())
//                     .end();
//             }
//             [[fallthrough]];
// 
//         case Node::Type::Root:
//             for (const auto& subNodes : *node)
//             {
//                 genFolders(subNodes);
//             }
//             break;
//         }
//     };
//     genFolders(env->rootNode);

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
    if (!generatePrj(workspace->projects[0])) return false;
    if (!generateFilters(workspace->projects[0])) return false;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// launchIde

func VStudioBackend::launchIde(const WorkspaceRef workspace) -> void
{
    ProjectRef mainProject = workspace->projects[0];

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

func VStudioBackend::build(const WorkspaceRef ws) -> BuildState
{
    ProjectRef proj = ws->projects[0];
    Env& env = proj->env;
    if (proj->appType == AppType::DynamicLibrary)
    {
        error(env.cmdLine, "DLL support is unimplemented.");
        return BuildState::Failed;
    }

    //
    // Build C++ files
    //

    vector<string> objs;
    fs::path buildTypeFolder = (proj->env.buildType == BuildType::Release) ? "release" : "debug";
    int numCompiledFiles = 0;

    // Recurse all through the nodes, applying the lambda for each one.
    // #todo: move this to a recursive function that goes through all dependencies
//     if (!env->rootNode->build([
//         this,
//         &env,
//         &proj,
//         &objs,
//         &buildTypeFolder,
//         &numCompiledFiles]
//         (Node* node) -> bool
//     {
//         // Builder
//         fs::path srcPath = node->path();
//         fs::path objPath = env->rootPath / "_obj" / buildTypeFolder / fs::relative(node->path(), env->rootPath);
//         objPath.replace_extension(".obj");
//         objs.push_back(objPath.string());
// 
//         bool build = false;
//         if (!fs::exists(objPath)) build = true;
//         else
//         {
//             auto ts = fs::last_write_time(srcPath);
//             auto to = fs::last_write_time(objPath);
// 
//             if (ts > to) build = true;
//             else
//             {
//                 // Check dependencies
//                 if (node->type() == Node::Type::Cpp)
//                 {
//                     CppNode* cppNode = (CppNode *)node;
//                     for (const auto& dep : *cppNode)
//                     {
//                         auto ts = fs::last_write_time(dep);
//                         if (ts > to)
//                         {
//                             build = true;
//                             break;
//                         }
//                     }
//                 }
//             }
//         }
// 
//         if (build)
//         {
//             if (!ensurePath(env->cmdLine, objPath.parent_path()))
//             {
//                 error(env->cmdLine, stringFormat("Unable to create folder `{0}`.", objPath.parent_path().string()));
//                 return false;
//             }
// 
//             string cmd = m_compiler.string();
//             Lines errorLines;
// 
//             // #todo: Add multiple include paths for dependencies.
//             vector<string> args = {
//                 "/nologo",
//                 "/EHsc",
//                 "/c",
//                 "/Zi",
//                 "/W3",
//                 "/WX",
//                 env->buildType == BuildType::Release ? "/MT" : "/MTd",
//                 "/std:c++17",
//                 "/Fd\"" + (env->rootPath / "_obj" / buildTypeFolder / "vc141.pdb").string() + "\"",
//                 "/Fo\"" + objPath.string() + "\"",
//                 "\"" + srcPath.string() + "\"",
//                 "/I\"" + (env->rootPath / "src").string() + "\""
//             };
// 
//             // DLLs and Libs have a "inc" folder for their public APIs.  Add this to the include paths
//             if (proj->appType != AppType::Exe)
//             {
//                 args.emplace_back(string("/I\"") + (env->rootPath / "inc").string() + "\"");
//             }
// 
//             // Add the compiler's standard include paths.
//             for (const auto& path : m_includePaths)
//             {
//                 args.emplace_back(string("/I\"") + path.string() + "\"");
//             }
// 
//             if (env->cmdLine.flag("v") || env->cmdLine.flag("verbose"))
//             {
//                 string line = cmd;
//                 for (const auto& arg : args)
//                 {
//                     line += " " + arg;
//                 }
//                 msg(env->cmdLine, "Running", line);
//             }
// 
//             msg(env->cmdLine, "Compiling", srcPath.string());
//             Process p(move(cmd), move(args), fs::current_path(),
//                 [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); },
//                 [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); });
// 
//             if (p.get())
//             {
//                 // Non-zero result means a failed compilation.
//                 error(env->cmdLine, stringFormat("Compilation of `{0}` failed.", srcPath.string()));
//                 errorLines.generate();
//                 for (const auto& line : errorLines)
//                 {
//                     cout << line << endl;
//                 }
//                 return false;
//             }
// 
//             ++numCompiledFiles;
//         }
// 
//         return true;
//     }))
//     {
//         return BuildState::Failed;
//     }

    //
    // Linking or library production
    // #todo: Support DLLs
    //
    Lines errorLines;
    fs::path binPath = env->rootPath / "_bin" / buildTypeFolder;
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
        if (!ensurePath(env->cmdLine, outPath.parent_path()))
        {
            error(env->cmdLine, stringFormat("Unable to create folder `{0}`.", outPath.string()));
            return BuildState::Failed;
        }
        bool release = (env->buildType == BuildType::Release);

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
                env->exeType == ExeType::Console ? "/SUBSYSTEM:CONSOLE" : "/SUBSYSTEM:WINDOWS",
                release ? "/OPT:REF" : "",
                release ? "/OPT:ICF" : "",
                "/MACHINE:X64"
            };

            // Add compiler's library paths.
            // #todo: Add dependency library paths.
            for (const auto& path : m_libPaths)
            {
                args.emplace_back(string("/LIBPATH:\"") + path.string() + "\"");
            }

            // Add compiled objects.
            for (const auto& obj : objs) 
            {
                args.emplace_back(obj); 
            }

            // Add libraries mentioned in forge.ini
            string libs = env->cfg.get("build.libs");
            vector<string> libsVector = split(libs, ";");
            for (const auto& lib : libsVector)
            {
                args.emplace_back(lib + ".lib");
            }

            if (env->cmdLine.flag("v") || env->cmdLine.flag("verbose"))
            {
                string line = cmd;
                for (const auto& arg : args)
                {
                    line += " " + arg;
                }
                msg(env->cmdLine, "Running", line);
            }

            msg(env->cmdLine, "Linking", outPath.string());
            Process p(move(cmd), move(args), fs::current_path(),
                [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); },
                [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); });

            if (p.get())
            {
                error(env->cmdLine, stringFormat("Linking of `{0}` failed.", outPath.string()));
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

            if (env->cmdLine.flag("v") || env->cmdLine.flag("verbose"))
            {
                string line = cmd;
                for (const auto& arg : args)
                {
                    line += " " + arg;
                }
                msg(env->cmdLine, "Running", line);
            }

            msg(env->cmdLine, "Archiving", outPath.string());
            Process p(move(cmd), move(args), fs::current_path(),
                [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); },
                [&errorLines](const char* buffer, size_t len) { errorLines.feed(buffer, len); });

            if (p.get())
            {
                error(env->cmdLine, stringFormat("Creation of `{0}` failed.", outPath.string()));
                errorLines.generate();
                for (const auto& line : errorLines)
                {
                    cout << line << endl;
                }
                return BuildState::Failed;
            }
        }
    }
    else
    {
        return BuildState::NoWork;
    }

    return BuildState::Success;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
