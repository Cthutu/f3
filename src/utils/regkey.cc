//----------------------------------------------------------------------------------------------------------------------
// Registry Key implementation
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#if OS_WIN32

#include <utils/regkey.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------

RegKey::RegKey(RegistryKey key, std::string&& path, std::string&& name)
    : m_value()
{
    HKEY k;

    switch (key)
    {
    case RegistryKey::ClassesRoot: k = HKEY_CLASSES_ROOT; break;
    case RegistryKey::CurrentUser: k = HKEY_CURRENT_USER; break;
    case RegistryKey::LocalMachine: k = HKEY_LOCAL_MACHINE; break;
    case RegistryKey::CurrentConfig: k = HKEY_CURRENT_CONFIG; break;
    case RegistryKey::Users: k = HKEY_USERS; break;
    }

    HKEY hkey;
    if (RegOpenKeyA(k, NULL, &hkey) == ERROR_SUCCESS)
    {
        char* buffer = 0;
        DWORD size = 0;

        if (RegGetValueA(k, path.c_str(), name.c_str(), RRF_RT_ANY, NULL, 0, &size) == ERROR_SUCCESS)
        {
            buffer = new char[size + 1];
            RegGetValueA(k, path.c_str(), name.c_str(), RRF_RT_ANY, NULL, buffer, &size);
            m_value = buffer;
            delete[] buffer;
        }

        RegCloseKey(hkey);
    }
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // OS_WIN32
