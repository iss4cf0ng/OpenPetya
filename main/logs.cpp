// logs.cpp

#include "logs.h"

void fnPrintLog(LogLevel level, const wchar_t* format, ...)
{
    FILE* stream = (level == LEVEL_ERROR || level == LEVEL_WARN ? stderr : stdout);
    HANDLE hConsole = GetStdHandle(stream == stderr? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD originalAttrs = consoleInfo.wAttributes;

    switch (level)
    {
        case LEVEL_INFO:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_BLUE);
            fputws(L"[*]", stream);
            break;
        case LEVEL_GOOD:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_GREEN);
            fputws(L"[+]", stream);
            break;
        case LEVEL_ERROR:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_RED);
            fputws(L"[-]", stream);
            break;
        case LEVEL_WARN:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
            fputws(L"[!]", stream);
            break;
    }

    SetConsoleTextAttribute(hConsole, originalAttrs);
    fputws(L" ", stream);

    va_list args;
    va_start(args, format);
    vfwprintf(stream, format, args);
    va_end(args);

    //fputws(L"\n", stream);
}

void fnPrintLog(LogLevel level, const char* format, ...)
{
    FILE* stream = (level == LEVEL_ERROR || level == LEVEL_WARN) ? stderr : stdout;
    HANDLE hConsole = GetStdHandle(stream == stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD originalAttrs = consoleInfo.wAttributes;

    switch (level) {
        case LEVEL_INFO:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_BLUE);
            fputs("[*]", stream);
            break;
        case LEVEL_GOOD:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_GREEN);
            fputs("[+]", stream);
            break;
        case LEVEL_ERROR:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_RED);
            fputs("[-]", stream);
            break;
        case LEVEL_WARN:
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
            fputs("[!]", stream);
            break;
    }

    SetConsoleTextAttribute(hConsole, originalAttrs);
    fputs(" ", stream);

    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    //fputs("\n", stream);
}