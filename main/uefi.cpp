// uefi.cpp

#include "uefi.h"

std::wstring fnFindESP()
{
    for (wchar_t c = L'A'; c <= L'Z'; c++)
    {
        std::wstring szPath = std::wstring(1, c) + L":\\EFI\\Microsoft\\Boot\\botmgfw.efi";
        if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(szPath.c_str()))
            return std::wstring(1, c) + L":";
    }

    return L"";
}

std::wstring fnMountESP(int nIdxDrive)
{
    for (wchar_t c = L'Z'; c >= L'D'; c--)
    {

    }

    return L"";
}

void fnUnmountESP(const std::wstring& szLetter)
{


    return;
}

bool fnbInstallUEFI(const std::wstring& szSrcEfiPath, const std::wstring& szPassword, const std::wstring& szDrivePath, UINT64 nTotalSectors)
{


    return true;
}

bool fnbRestoreUEFI()
{


    return true;
}
