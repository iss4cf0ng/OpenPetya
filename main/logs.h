// logs.h

#ifndef LOGS_H
#define LOGS_H

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <windows.h>

enum LogLevel
{
    LEVEL_INFO,     // [*] Processing / General information (blue/cyan)
    LEVEL_GOOD,     // [+] Successed (green)
    LEVEL_ERROR,    // [-] Failed / Error (red)
    LEVEL_WARN      // [!] Warning (yellow)
};

void fnPrintLog(LogLevel level, const wchar_t* format, ...);
void fnPrintLog(LogLevel level, const char* format, ...);

#endif