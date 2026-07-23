#pragma once
#include <Windows.h>

namespace ArcVibrance
{
bool RegisterProfileEditorClass(HINSTANCE instance);
LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
}
