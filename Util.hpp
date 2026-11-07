// Util.hpp --- small helpers shared by the modules
#pragma once

#include "Typaste.hpp"

// Load a localized string from the resource of the current language
tstring_t LoadStr(UINT id);

// Format a message that takes one string argument ("%s")
tstring_t FormatStr(const tstring_t& fmt, LPCTSTR pszArg);

// Format a message that takes one integer argument ("%d")
tstring_t FormatInt(const tstring_t& fmt, int nValue);

// Format a message that takes two integer arguments ("%d/%d")
tstring_t FormatInt2(const tstring_t& fmt, int nValue1, int nValue2);
