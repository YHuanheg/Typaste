// Util.cpp --- small helpers shared by the modules
#include "Typaste.hpp"
#include "Lang.hpp"

tstring_t LoadStr(UINT id)
{
    std::wstring str = Lang_LoadString(id);
#ifdef UNICODE
    return str;
#else
    std::string ansi;
    ansi.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i)
        ansi.push_back((char)str[i]);
    return ansi;
#endif
}

tstring_t FormatStr(const tstring_t& fmt, LPCTSTR pszArg)
{
    TCHAR szText[1024] = TEXT("");
    if (fmt.empty())
        return szText;
    wsprintf(szText, fmt.c_str(), pszArg);
    return szText;
}

tstring_t FormatInt(const tstring_t& fmt, int nValue)
{
    TCHAR szText[512] = TEXT("");
    if (fmt.empty())
        return szText;
    wsprintf(szText, fmt.c_str(), nValue);
    return szText;
}
