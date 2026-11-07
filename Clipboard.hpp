// Clipboard.hpp --- system clipboard helpers
#pragma once

#include "Typaste.hpp"

// Read the clipboard text (CF_UNICODETEXT / CF_TEXT)
BOOL Clip_GetText(tstring_t& str);

// Replace the clipboard text
BOOL Clip_SetText(const tstring_t& str);

// Get the selected text of an edit control, or its whole text when nothing is
// selected. bSelection receives TRUE when a selection was used.
BOOL Clip_GetEditSelectionOrAll(HWND hEdit, tstring_t& str, BOOL *pbSelection);
