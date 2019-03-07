//----------------------------------------------------------------------------------------------------------------------
// Run command
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <data/env.h>
#include <data/workspace.h>
#include <utils/msg.h>
#include <utils/process.h>

func cmd_build(const Env& env) -> int;

//----------------------------------------------------------------------------------------------------------------------

func cmd_run(const Env& env) -> int
{
    int result = cmd_build(env);
    if (result) return result;

    bool release = env.buildType == BuildType::Release;

    Config cfg;
    cfg.readIni(env.cmdLine, env.rootPath / "forge.ini");

    auto typeString = cfg.get("info.type");
    if (typeString != "exe")
    {
        error(env.cmdLine, "Not an application project.  Cannot run!");
        return 1;
    }

    fs::path exePath = fs::path("_bin") / (release ? "release" : "debug") / (cfg.get("info.name", "out") + ".exe");
    fs::path exeFile = env.rootPath / exePath;
    if (fs::exists(exeFile))
    {
        msg(env.cmdLine, "Running", stringFormat("`{0}`", exePath.string()));
        Process p(exeFile.string(), vector<string>(env.cmdLine.secondaryParams()));
        int exitCode = p.get();
        msg(env.cmdLine, "Ended", stringFormat("Exit code: {0}", exitCode));
    }
    else
    {
        error(env.cmdLine, stringFormat("Unable to locate `{0}`", exeFile.string()));
        return 1;
    }

    return 0;
}