#pragma once

#include <windows.h>

enum class CommandMenuAction
{
    None,
    Edit,
    Delete
};

class CommandMenu
{
public:
    static CommandMenuAction Show(HWND owner, POINT screenPoint);
};
