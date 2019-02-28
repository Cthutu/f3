//----------------------------------------------------------------------------------------------------------------------
// GenInfo structure
//
// This contains information required to generate the initial files in a project.  The information is compiled from
// the command line
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <utils/cmdline.h>

namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// AppType

enum class AppType
{
    Exe,
    Library,
    DynamicLibrary,
};

//----------------------------------------------------------------------------------------------------------------------
// Subsystem type

enum class SubsystemType
{
    NotRequired,
    Console,
    Windows,
};

//----------------------------------------------------------------------------------------------------------------------
// TextFile

class TextFile
{
public:
    TextFile(fs::path&& path);

    func operator << (std::string&& line) -> TextFile&;

    func write() const -> bool;
    func getPath() const -> const fs::path& { return m_path; }

private:
    fs::path m_path;
    std::vector<std::string> m_lines;
};

//----------------------------------------------------------------------------------------------------------------------
// GenInfo struct

struct GenInfo
{
    std::string             projName;
    fs::path                projPath;
    AppType                 appType;
    SubsystemType           subsystemType;
    std::vector<TextFile>   textFiles;

    GenInfo(const CmdLine& cmdLine);

    func apply(const CmdLine& cmdLine) const -> bool;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
