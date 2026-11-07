// Config.hpp --- settings storage (Typaste.ini next to the exe, or the registry)
#pragma once

#include "Typaste.hpp"
#include <vector>

// The full path of Typaste.ini (next to the executable)
tstring_t Config_GetPath(void);

// Is Typaste.ini in use? (it is preferred over the registry when it exists)
BOOL Config_IsIniMode(void);

// Create Typaste.ini next to the executable when possible
BOOL Config_EnsureFile(void);

// Remove the stored settings (INI file or the registry key)
BOOL Config_Delete(void);

BOOL Config_ReadInt(LPCTSTR pszName, DWORD *pValue);
BOOL Config_ReadString(LPCTSTR pszName, tstring_t& str);
BOOL Config_WriteInt(LPCTSTR pszName, DWORD value);
BOOL Config_WriteString(LPCTSTR pszName, const tstring_t& str);

BOOL Config_ReadWhitelist(std::vector<tstring_t>& list);
BOOL Config_WriteWhitelist(const std::vector<tstring_t>& list);
