//----------------------------------------------------------------------------------------------------------------------
// Clean command
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <data/env.h>
#include <filesystem>
#include <utils/msg.h>

namespace fs = std::filesystem;
using namespace std;

//----------------------------------------------------------------------------------------------------------------------

func cmd_clean(const Env& env) -> int
{
    if (!checkProject(env)) return 1;

    for (const auto& path : fs::directory_iterator(env.rootPath))
    {
        if (path.is_directory())
        {
            fs::path dir = path.path();
            if (dir.filename().string()[0] == '_')
            {
                try
                {
                    fs::remove_all(env.rootPath / dir);
                }
                catch (fs::filesystem_error& err)
                {
                    error(env.cmdLine, stringFormat("File-system error: {0}", err.what()));
                    return 1;
                }
            }
        }
    }

    return 0;
}