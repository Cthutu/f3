//----------------------------------------------------------------------------------------------------------------------
// Win32 Registry access
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

//----------------------------------------------------------------------------------------------------------------------

enum class RegistryKey
{
    ClassesRoot,
    CurrentUser,
    LocalMachine,
    CurrentConfig,
    Users,
};

//----------------------------------------------------------------------------------------------------------------------

class RegKey
{
public:
    RegKey(RegistryKey key, std::string&& path, std::string&& name);

    func get() const -> const std::string& { return m_value; };

private:
    std::string m_value;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
