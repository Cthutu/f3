//----------------------------------------------------------------------------------------------------------------------
// Backend factory
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <backends/backends.h>
#include <fstream>
#include <functional>
#include <utils/cmdline.h>
#include <utils/msg.h>
#include <utils/utils.h>

#if OS_WIN32
#   include <backends/vstudio.h>
#endif

using namespace std;

//----------------------------------------------------------------------------------------------------------------------
// getBackEnd

func getBackend(const CmdLine& cmdLine) -> unique_ptr<IBackend>
{
#if OS_WIN32

    auto vs = make_unique<VStudioBackend>();
    if (vs->available()) return vs;

#endif

    error(cmdLine, "Unable to find supported IDE or compiler.");
    return {};
}

//----------------------------------------------------------------------------------------------------------------------
// Include paths

func IBackend::getIncludePaths(const Project* proj, vector<fs::path>& paths) -> void
{
    // #todo: Handle dependent projects
    paths.emplace_back(proj->rootPath / "src");
    if (proj->appType == AppType::Library || proj->appType == AppType::DynamicLibrary)
    {
        paths.emplace_back(proj->rootPath / "inc");
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Library paths

func IBackend::getLibPaths(const Project* proj, BuildType buildType, vector<fs::path>& paths) -> void
{
    // #todo: Handle dependent projects
    if (proj->appType != AppType::Exe)
    {
        paths.emplace_back(proj->rootPath / "_bin" / (buildType == BuildType::Debug ? "debug" : "release"));
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Backend utility methods

func IBackend::scanDependencies(const Project* proj, const unique_ptr<Node>& node) -> void
{
    vector<fs::path> includePaths;
    getIncludePaths(proj, includePaths);

    function<void(const fs::path&)> scanFiles = [&includePaths, &node, &scanFiles]
    (const fs::path& path) -> void
    {
        ifstream f(path);
        if (f)
        {
            string line;
            while (getline(f, line))
            {
                trim(line);
                if (line.substr(0, 8) == "#include")
                {
                    string includePath = extractSubStr(line, '"', '"');
                    if (includePath.empty())
                    {
                        includePath = extractSubStr(line, '<', '>');
                    }
                    if (!includePath.empty())
                    {
                        for (const auto& p : includePaths)
                        {
                            fs::path checkPath = p / includePath;
                            if (node->deps.find(checkPath) == node->deps.end() && fs::exists(checkPath))
                            {
                                // Found a dependency that's original.
                                node->deps.insert(checkPath);
                                scanFiles(checkPath);
                            }
                        }
                    }
                }

            }
        }
    };

    scanFiles(node->fullPath);
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

