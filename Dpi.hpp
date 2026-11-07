// Dpi.hpp --- Per-Monitor V2 helpers
#pragma once

#include "Typaste.hpp"

// Remember the base layout of a dialog (all values normalized to 96 DPI)
void Dpi_InitTracking(HWND hwnd);

// Re-apply the base layout for the new DPI
BOOL Dpi_HandleChanged(HWND hwnd, WPARAM wParam, LPARAM lParam);

// Create the bold title font of the dialog
void Dpi_SetupTitleFont(HWND hwnd);

// Forget the layout of a dialog that is being destroyed
void Dpi_Forget(HWND hwnd);

// Get the DPI of a window (Windows 10+; falls back to the device caps)
UINT Dpi_GetWindowDpi(HWND hwnd);
