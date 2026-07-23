#include "../GameMonitor.h"
#include <iostream>

int wmain()
{
    struct Case { const wchar_t* left; const wchar_t* right; bool expected; };
    const Case cases[] = {
        {L"C:\\Games\\CS2\\cs2.exe", L"c:\\games\\cs2\\CS2.EXE", true},
        {L"C:/Games/CS2/cs2.exe", L"C:\\Games\\CS2\\cs2.exe", true},
        {L"C:\\Games\\one.exe", L"C:\\Games\\two.exe", false},
        {L"", L"", true},
        {L"C:\\Game\\app.exe", L"C:\\Game2\\app.exe", false}
    };

    for (const auto& test : cases)
    {
        const bool actual = GameMonitor::PathsEqual(test.left, test.right);
        if (actual != test.expected)
        {
            std::wcerr << L"PathsEqual failed for '" << test.left << L"' and '" << test.right << L"'\n";
            return 1;
        }
    }

    return 0;
}
