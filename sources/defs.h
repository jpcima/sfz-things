#pragma once

#ifdef _WIN64
#define SFORZANDO_DLL_DEFAULT_PATH                                      \
    "C:\\Program Files\\Plogue\\sforzando\\VST\\sforzando VST_x64.dll"
#else
#define SFORZANDO_DLL_DEFAULT_PATH                                      \
    "C:\\Program Files\\Plogue\\sforzando\\VST\\sforzando VST_x86.dll"
#endif
