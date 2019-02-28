//----------------------------------------------------------------------------------------------------------------------
// Command line processing
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <set>

//----------------------------------------------------------------------------------------------------------------------

class CmdLine
{
public:
    CmdLine(int argc, char** argv);

    func command() const -> const std::string&;
    func exePath() const -> const std::string&;
    func numParams() const -> uint;
    func param(uint i) const -> const std::string&;
    func flag(std::string name) const -> bool;
    func secondaryParams() const -> const std::vector<std::string>&;

private:
    std::string m_exePath;
    std::string m_command;
    std::vector<std::string> m_params;
    std::vector<std::string> m_secondaryParams;
    std::set<std::string> m_flags;
};

