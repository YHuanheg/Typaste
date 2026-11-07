// Config.cpp --- settings storage
//
// The preferred store is Typaste.ini next to the executable, so that the program
// can be used as a portable application. When the file does not exist (or the
// directory is read-only, e.g. Program Files), the registry is used as before.
#include "Typaste.hpp"
#include "App.hpp"
#include "Config.hpp"
#include <vector>

#define INI_SECTION L"Typaste"

BOOL Config_IsIniMode(void)
{
    const tstring_t strPath = Config_GetPath();
    return PathFileExists(strPath.c_str()) ? TRUE : FALSE;
}

tstring_t Config_GetPath(void)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("Typaste.ini"));
    return szPath;
}

BOOL Config_EnsureFile(void)
{
    const tstring_t strPath = Config_GetPath();
    if (PathFileExists(strPath.c_str()))
        return TRUE;

    // Create the file with the settings version; the caller writes the values
    TCHAR szVersion[32];
    wsprintf(szVersion, TEXT("%u"), (unsigned)SETTINGS_VERSION);
    if (!WritePrivateProfileStringW(INI_SECTION, L"SettingsVersion", szVersion, strPath.c_str()))
        return FALSE;

    return TRUE;
}

BOOL Config_Delete(void)
{
    BOOL bResult = FALSE;

    if (Config_IsIniMode())
    {
        const tstring_t strPath = Config_GetPath();
        bResult = DeleteFile(strPath.c_str()) != FALSE;
    }

    // Also remove the registry key so that a later non-portable run starts clean
    if (RegDeleteKey(HKEY_CURRENT_USER, TYP_REGKEY) == ERROR_SUCCESS)
        bResult = TRUE;

    return bResult;
}

BOOL Config_ReadInt(LPCTSTR pszName, DWORD *pValue)
{
    if (!pszName || !pValue)
        return FALSE;

    if (Config_IsIniMode())
    {
        const tstring_t strPath = Config_GetPath();
        const UINT nValue = GetPrivateProfileIntW(INI_SECTION, pszName, (UINT)-1, strPath.c_str());
        if (nValue == (UINT)-1)
            return FALSE;
        *pValue = (DWORD)nValue;
        return TRUE;
    }

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TYP_REGKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    DWORD dwData = 0, cbData = sizeof(dwData);
    const LONG error = RegQueryValueEx(hKey, pszName, NULL, NULL, (LPBYTE)&dwData, &cbData);
    RegCloseKey(hKey);
    if (error != ERROR_SUCCESS)
        return FALSE;

    *pValue = dwData;
    return TRUE;
}

BOOL Config_ReadString(LPCTSTR pszName, tstring_t& str)
{
    str.clear();
    if (!pszName)
        return FALSE;

    if (Config_IsIniMode())
    {
        const tstring_t strPath = Config_GetPath();
        TCHAR szText[MAX_PATH * 2] = TEXT("");
        GetPrivateProfileStringW(INI_SECTION, pszName, TEXT(""), szText, _countof(szText), strPath.c_str());
        if (!szText[0])
            return FALSE;
        str = szText;
        return TRUE;
    }

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TYP_REGKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    TCHAR szText[MAX_PATH * 2] = TEXT("");
    DWORD cbData = sizeof(szText) - sizeof(TCHAR);
    const LONG error = RegQueryValueEx(hKey, pszName, NULL, NULL, (LPBYTE)szText, &cbData);
    RegCloseKey(hKey);
    if (error != ERROR_SUCCESS)
        return FALSE;

    szText[_countof(szText) - 1] = 0;
    str = szText;
    return TRUE;
}

BOOL Config_WriteInt(LPCTSTR pszName, DWORD value)
{
    if (!pszName)
        return FALSE;

    if (Config_IsIniMode())
    {
        const tstring_t strPath = Config_GetPath();
        TCHAR szValue[32];
        wsprintf(szValue, TEXT("%u"), (unsigned)value);
        return WritePrivateProfileStringW(INI_SECTION, pszName, szValue, strPath.c_str()) != FALSE;
    }

    HKEY hAppKey = NULL;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, TYP_REGKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hAppKey, NULL) != ERROR_SUCCESS)
        return FALSE;

    const LONG error = RegSetValueEx(hAppKey, pszName, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
    RegCloseKey(hAppKey);
    return error == ERROR_SUCCESS;
}

BOOL Config_WriteString(LPCTSTR pszName, const tstring_t& str)
{
    if (!pszName)
        return FALSE;

    if (Config_IsIniMode())
    {
        const tstring_t strPath = Config_GetPath();
        return WritePrivateProfileStringW(INI_SECTION, pszName, str.c_str(), strPath.c_str()) != FALSE;
    }

    HKEY hAppKey = NULL;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, TYP_REGKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hAppKey, NULL) != ERROR_SUCCESS)
        return FALSE;

    const DWORD cbData = (DWORD)((str.size() + 1) * sizeof(TCHAR));
    const LONG error = RegSetValueEx(hAppKey, pszName, 0, REG_SZ, (const BYTE *)str.c_str(), cbData);
    RegCloseKey(hAppKey);
    return error == ERROR_SUCCESS;
}

// Trim the surrounding white space of a list item
static void TrimItem(tstring_t& str)
{
    size_t a = 0, b = str.size();
    while (a < b && (str[a] == TEXT(' ') || str[a] == TEXT('\t') ||
                     str[a] == TEXT('\r') || str[a] == TEXT('\n')))
        ++a;
    while (b > a && (str[b - 1] == TEXT(' ') || str[b - 1] == TEXT('\t') ||
                     str[b - 1] == TEXT('\r') || str[b - 1] == TEXT('\n')))
        --b;
    str = str.substr(a, b - a);
}

// The whitelist is stored as "a.exe;b.exe" in the INI file and as a
// REG_MULTI_SZ value in the registry.
BOOL Config_ReadWhitelist(std::vector<tstring_t>& list)
{
    list.clear();

    if (Config_IsIniMode())
    {
        tstring_t str;
        if (!Config_ReadString(TEXT("Whitelist"), str))
            return FALSE;

        size_t pos = 0;
        while (pos <= str.size())
        {
            const size_t next = str.find(TEXT(';'), pos);
            tstring_t item = str.substr(pos, next == std::wstring::npos ? std::wstring::npos : next - pos);
            TrimItem(item);
            if (!item.empty())
                list.push_back(item);
            if (next == std::wstring::npos)
                break;
            pos = next + 1;
        }
        return TRUE;
    }

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TYP_REGKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    DWORD dwType = 0, cbData = 0;
    LONG error = RegQueryValueEx(hKey, TEXT("Whitelist"), NULL, &dwType, NULL, &cbData);
    if (error != ERROR_SUCCESS || dwType != REG_MULTI_SZ || cbData < sizeof(TCHAR))
    {
        RegCloseKey(hKey);
        return FALSE;
    }

    std::vector<TCHAR> buf(cbData / sizeof(TCHAR) + 2, 0);
    error = RegQueryValueEx(hKey, TEXT("Whitelist"), NULL, NULL, (BYTE *)&buf[0], &cbData);
    RegCloseKey(hKey);
    if (error != ERROR_SUCCESS)
        return FALSE;

    for (size_t i = 0; i < buf.size(); )
    {
        const TCHAR *psz = &buf[i];
        if (!*psz)
            break;
        list.push_back(psz);
        i += lstrlen(psz) + 1;
    }
    return TRUE;
}

BOOL Config_WriteWhitelist(const std::vector<tstring_t>& list)
{
    if (Config_IsIniMode())
    {
        tstring_t str;
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (i)
                str += TEXT(';');
            str += list[i];
        }
        return Config_WriteString(TEXT("Whitelist"), str);
    }

    std::vector<TCHAR> buf;
    for (size_t i = 0; i < list.size(); ++i)
    {
        buf.insert(buf.end(), list[i].begin(), list[i].end());
        buf.push_back(0);
    }
    buf.push_back(0);

    HKEY hAppKey = NULL;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, TYP_REGKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hAppKey, NULL) != ERROR_SUCCESS)
        return FALSE;

    const DWORD cbData = (DWORD)(buf.size() * sizeof(TCHAR));
    const LONG error = RegSetValueEx(hAppKey, TEXT("Whitelist"), 0, REG_MULTI_SZ, (const BYTE *)&buf[0], cbData);
    RegCloseKey(hAppKey);
    return error == ERROR_SUCCESS;
}
