//----------------------------------------------------------------------------------------------------------------------
// New command
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <data/env.h>
#include <data/geninfo.h>
#include <utils/utils.h>
#include <utils/msg.h>
#include <utils/process.h>

//----------------------------------------------------------------------------------------------------------------------

func cmd_new(const Env& env) -> int
{
    //
    // Validate parameters
    //

    if (env.cmdLine.numParams() != 1)
    {
        error(env.cmdLine, "Invalid parameters for `new` command.");
        return 1;
    }

    if (!validateFileName(env.cmdLine.param(0)))
    {
        error(env.cmdLine, stringFormat("`{0}` is an invalid name for a project.", env.cmdLine.param(0)));
        return 1;
    }

    //
    // Process environment and apply generation info
    //

    GenInfo info(env.cmdLine);
    if (!info.apply(env.cmdLine))
    {
        try
        {
            remove_all(info.projPath);
        }
        catch (...)
        {
        }
        return 1;
    }

    //
    // Initialise Git repository
    //

    using namespace std::filesystem;

    TextFile gitignore(info.projPath / ".gitignore");
    gitignore << "_*/";
    if (!gitignore.write())
    {
        error(env.cmdLine, "Cannot create .gitignore file.");
        remove_all(info.projPath);
        return 1;
    }

    Process p1("git", { "init" }, path(info.projPath));
    if (p1.get())
    {
        error(env.cmdLine, "Unable to initialise the git repository.  Have you installed git?");
        remove_all(info.projPath);
        return 1;
    }
    Process p2("git", { "add", "." }, path(info.projPath));
    if (p2.get())
    {
        error(env.cmdLine, "Unable to add files to git repository.");
        remove_all(info.projPath);
        return 1;
    }
    Process p3("git", { "commit", "-m", "Initial commit." }, path(info.projPath));
    if (p3.get())
    {
        error(env.cmdLine, "Unable to commit initial files to repository.");
        remove_all(info.projPath);
        return 1;
    }

    //
    // Finalise with a message
    //

    string resultMsg;
    switch (info.appType)
    {
    case AppType::Exe:
        resultMsg = stringFormat("binary (application) `{0}` project.", info.projName);
        break;
    case AppType::Library:
        resultMsg = stringFormat("library `{0}` project.", info.projName);
        break;
    case AppType::DynamicLibrary:
        resultMsg = stringFormat("dynamic library `{0}` project.", info.projName);
        break;
    }

    msg(env.cmdLine, "Created", resultMsg);
    return 0;
}