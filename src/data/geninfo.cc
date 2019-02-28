//----------------------------------------------------------------------------------------------------------------------
// Construct and apply GenInfos
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <data/geninfo.h>
#include <fstream>
#include <utils/msg.h>
#include <utils/utils.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// TextFile
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

TextFile::TextFile(fs::path&& path)
    : m_path(move(path))
{

}

//----------------------------------------------------------------------------------------------------------------------

func TextFile::operator << (string&& line) -> TextFile&
{
    m_lines.emplace_back(move(line));
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

func TextFile::write() const -> bool
{
    ofstream f(m_path);
    if (f)
    {
        for (const auto& line : m_lines)
        {
            f << line << endl;
        }
    }

    return bool(f);
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// GenInfo
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

GenInfo::GenInfo(const CmdLine& cmdLine)
{
    assert(cmdLine.command() == "new");
    assert(cmdLine.numParams() == 1);

    projName = cmdLine.param(0);
    assert(validateFileName(projName));

    //
    // Work out application and sub-system (if necessary) type
    //

    appType = AppType::Exe;
    subsystemType = SubsystemType::NotRequired;
    string typeString = "exe";

    if (cmdLine.flag("lib"))
    {
        appType = AppType::Library;
        typeString = "lib";
    }
    else if (cmdLine.flag("dll"))
    {
        appType = AppType::DynamicLibrary;
        typeString = "dll";
    }
    else
    {
        subsystemType = SubsystemType::Console;
        if (cmdLine.flag("windows")) subsystemType = SubsystemType::Windows;
    }

    //
    // Generate the paths
    //

    projPath = fs::current_path() / projName;
    auto incPath = projPath / "inc";
    auto srcPath = projPath / "src";
    auto testPath = projPath / "test";

    //
    // Generate the files
    //

    // Forge.ini
    textFiles.emplace_back(projPath / "forge.ini");
    textFiles.back() << "[info]";
    textFiles.back() << stringFormat("name = {0}", projName);
    textFiles.back() << stringFormat("type = {0}", typeString);
    if (subsystemType == SubsystemType::Windows)
    {
        textFiles.back() << "system = windows";
    }
    textFiles.back() << "";
    textFiles.back() << "[build]";
    textFiles.back() << "# Uncomment this to add libraries to link with.";
    textFiles.back() << "# libs = ";
    textFiles.back() << "";
    textFiles.back() << "[dependencies]";

    // SRC folder
    if (appType == AppType::Exe)
    {
        textFiles.emplace_back(srcPath / "main.cc");
        textFiles.back() << "#include <iostream>";
        textFiles.back() << "";
        textFiles.back() << "auto main(int argc, char** argv) -> int";
        textFiles.back() << "{";
        textFiles.back() << "    std::cout << \"Hello, World!\" << std::endl;";
        textFiles.back() << "}";
        textFiles.back() << "";
    }
    else
    {
        textFiles.emplace_back(incPath / projName / (projName + ".h"));
        textFiles.back() << "#pragma once";
        textFiles.back() << "";
        textFiles.back() << "auto hello() -> void;";
        textFiles.back() << "";
        
        textFiles.emplace_back(srcPath / "hello.cc");
        textFiles.back() << stringFormat("#include <{0}/{0}.h>", projName);
        textFiles.back() << "#include <iostream>";
        textFiles.back() << "";
        textFiles.back() << "auto hello() -> void";
        textFiles.back() << "{";
        textFiles.back() << "    std::cout << \"Hello, World!\" << std::endl;";
        textFiles.back() << "}";
        textFiles.back() << "";

        textFiles.emplace_back(testPath / "test_main.cc");
        textFiles.back() << "// Only include this line in one compilation unit.";
        textFiles.back() << "#define CATCH_CONFIG_MAIN";
        textFiles.back() << "";
        textFiles.back() << "#include <catch.h>";
        textFiles.back() << stringFormat("#include <{0}/{0}.h>", projName);
        textFiles.back() << "";
        textFiles.back() << "TEST_CASE(\"Greet\", \"[Greet]\")";
        textFiles.back() << "{";
        textFiles.back() << "    hello();";
        textFiles.back() << "}";
        textFiles.back() << "";
    }
}

//----------------------------------------------------------------------------------------------------------------------

func GenInfo::apply(const CmdLine& cmdLine) const -> bool
{
    if (fs::exists(projPath))
    {
        error(cmdLine, "The path for this project already exists.");
        return false;
    }

    for (const auto& textFile : textFiles)
    {
        if (!ensurePath(cmdLine, textFile.getPath().parent_path()) || !textFile.write())
        {
            error(cmdLine, stringFormat("Cannot write `{0}`.", textFile.getPath().string()));
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
