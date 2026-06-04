// uefi.cpp

#include "uefi.h"

std::wstring fnFindESP()
{
    for (wchar_t c = L'A'; c <= L'Z'; c++)
    {
        std::wstring szPath = std::wstring(1, c) + L":" + BOOTMGFW_PATH;
        if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(szPath.c_str()))
            return std::wstring(1, c) + L":";
    }

    return L"";
}

std::wstring fnMountESP(int nIdxDrive)
{
    for (wchar_t c = L'Z'; c >= L'D'; c--)
    {
        std::wstring szLetter = std::wstring(1, c);
        if (DRIVE_NO_ROOT_DIR == GetDriveTypeW((szLetter + L"\\").c_str()))
        {
            // try to mount ESP
            std::wstring szCmd = L"cmd.exe /c mountvol " + szLetter + L":" + L" /S";

            STARTUPINFOW si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            std::vector<wchar_t> buffer(szCmd.begin(), szCmd.end());
            buffer.push_back(0);

            if (CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
            {
                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }

            // check if it worked
            std::wstring szTest = szLetter + BOOTMGFW_PATH;
            if (INVALID_FILE_ATTRIBUTES != GetFileAttributes(szTest.c_str()))
            {
                printf("ESP is mounted at %ls\n", szLetter.c_str());
                return szLetter;
            }
        }
    }

    return L"";
}

void fnUnmountESP(const std::wstring& szLetter)
{
    std::wstring szCmd = L"cmd.exe /c mountvol " + szLetter + L" /D";
    std::vector<wchar_t> buffer(szCmd.begin(), szCmd.end());
    buffer.push_back(0);

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    printf("ESP is unmount from %ls\n", szLetter.c_str());

    return;
}

bool fnbInstallUEFI(const std::wstring& szSrcEfiPath, const std::wstring& szPassword, const std::wstring& szDrivePath, UINT64 nTotalSectors)
{
    printf("UEFI installation\n");

    // find/mount ESP
    std::wstring szESP = fnFindESP();
    bool bMount = false;
    
    if (szESP.empty())
    {
        printf("ESP not mounted, mounting temporarily...\n");
        szESP = fnMountESP(0);
        bMount = true;
    }

    if (szESP.empty())
    {
        fprintf(stderr, "ERROR: Cannot find or mount ESP!\n");
        fprintf(stderr, "Please try to run this program with adminstrative privilege.\n");
        return false;
    }

    printf("ESP at: %ls\n", szESP.c_str());



    return true;
}

bool fnbRestoreUEFI()
{


    return true;
}
