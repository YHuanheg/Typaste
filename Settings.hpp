// Settings.hpp --- the application settings
#pragma once

#include "Typaste.hpp"

// Fill g_settings with the default values
void Settings_LoadDefaults(void);

// Load the settings (Typaste.ini first, then the registry)
BOOL Settings_Load(void);

// Save the settings
BOOL Settings_Save(void);

// Remove the stored settings
BOOL Settings_Delete(void);

// Show the settings dialog
void Settings_Show(HWND hwndParent);

// Should the current foreground window be handled by Typaste?
BOOL Settings_IsForegroundWhitelisted(void);
