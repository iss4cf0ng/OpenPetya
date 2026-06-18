// uefi.cpp

#include "uefi.h"
#include "utils.h"
#include "config.h"

std::wstring fnFindESP()
{
    for (wchar_t c = L'A'; c <= L'Z'; c++)
    {
        std::wstring szPath = std::wstring(1, c) + BOOTMGFW_PATH;
        if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(szPath.c_str()))
            return std::wstring(1, c) + L":";
    }

    return L"";
}

std::wstring fnMountESP(int nIdxDrive)
{
    // First, print what drives are available for diagnosis
    printf("[*] Scanning for free drive letters...\n");

    for (wchar_t c = L'Z'; c >= L'D'; c--)
    {
        std::wstring szLetter = std::wstring(1, c) + L":";
        UINT type = GetDriveTypeW((szLetter + L"\\").c_str());

        if (type != DRIVE_NO_ROOT_DIR)
            continue;

        printf("[*] Trying letter %ls...\n", szLetter.c_str());

        std::wstring szCmd = L"cmd.exe /c mountvol " + szLetter + L" /S";
        printf("[*] Command: %ls\n", szCmd.c_str());

        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::vector<wchar_t> buffer(szCmd.begin(), szCmd.end());
        buffer.push_back(0);

        if (CreateProcessW(nullptr, buffer.data(), nullptr, nullptr,
                FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 5000);
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            printf("[*] mountvol exit code: %lu\n", exit_code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        std::wstring szTest = szLetter + L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
        printf("[*] Checking: %ls\n", szTest.c_str());

        if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(szTest.c_str()))
        {
            printf("[+] ESP mounted at %ls\n", szLetter.c_str());
            return szLetter;
        }

        printf("[!] Not found at %ls, trying next...\n", szTest.c_str());

        // Unmount failed attempt before trying next letter
        std::wstring szUnmount = L"cmd.exe /c mountvol " + szLetter + L" /D";
        std::vector<wchar_t> ubuf(szUnmount.begin(), szUnmount.end());
        ubuf.push_back(0);
        STARTUPINFOW si2 = {}; si2.cb = sizeof(si2);
        PROCESS_INFORMATION pi2 = {};
        if (CreateProcessW(nullptr, ubuf.data(), nullptr, nullptr,FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si2, &pi2))
        {
            WaitForSingleObject(pi2.hProcess, 2000);
            CloseHandle(pi2.hProcess);
            CloseHandle(pi2.hThread);
        }
    }

    printf("[-] All letters exhausted, ESP not found.\n");
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

bool fnbInstallUEFI(const std::wstring& szSrcEfiPath, const std::string& szPassword, const std::wstring& szDrivePath, UINT64 nTotalSectors)
{
    printf("[*] UEFI installation\n");

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

    printf("[*] ESP at: %ls\n", szESP.c_str());

    std::wstring szBootDir = szESP + L"\\EFI\\Microsoft\\Boot\\";
    std::wstring szOriginalEFI = szBootDir + L"bootmgfw.efi";
    std::wstring szBackupEFI = szBootDir + L"bootmgfw_original.efi";
    std::wstring szTarget = szBootDir + L"bootmgfw.efi";

    printf("[*] Checking original bootmgfw.efi...\n");
    if (!fnbFileExists(szOriginalEFI))
    {
        fprintf(stderr, "[-] Error: %ls not found!\n", szOriginalEFI.c_str());
        if (bMount)
            fnUnmountESP(szESP);

        return false;
    }

    printf("[*] Found: %ls\n", szOriginalEFI.c_str());

    // backup original (skip if backup already exists)
    printf("[*] Backing up original bootmgfw.efi...\n");
    if (!fnbFileExists(szBackupEFI))
    {
        if (!fnbCopyFile(szOriginalEFI, szBackupEFI))
        {
            fprintf(stderr, "[-] ERROR: Backup failed!\n");
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }

        printf("[+] Back up to %ls\n", szBackupEFI.c_str());
    }
    else
    {
        printf("[!] Backup already exists, skipping.\n");
    }

    printf("[*] Installing custom UEFI bootloader...\n");
    if (!CopyFileW(szSrcEfiPath.c_str(), szTarget.c_str(), FALSE))
    {
        fprintf(stderr, "[-] ERROR: Install failed (error %lu)\n", GetLastError());
        
        // try with explicit overwrite flag
        SetFileAttributesW(szTarget.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!CopyFileW(szSrcEfiPath.c_str(), szTarget.c_str(), FALSE))
        {
            fprintf(stderr, "[-] Still failed with explicit overwrite flag :(\n");
            
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }
    }

    printf("[+] Installed: %ls\n", szTarget.c_str());

    printf("[*] Writing metadata sectors...\n");

    {
        clsDiskHandle disk;
        if (!disk.fnbOpen(szDrivePath, true))
        {
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }

        std::vector<uint8_t> state_sector(SECTOR_SIZE, 0);
        uint32_t magic = 0x424F4F54UL;
        memcpy(state_sector.data(), &magic, 4);
        state_sector[4] = 0x00;
        memcpy(state_sector.data() + 8, &nTotalSectors, 8);

        if (!disk.fnbWriteSectors(60, state_sector))
        {
            fprintf(stderr, "[-] ERROR: state sector write failed!\n");
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }

        // validate
        std::vector<uint8_t> abCheck;
        disk.fnbReadSectors(60, 1, abCheck);
        uint32_t rm = *(uint32_t *)abCheck.data();
        uint64_t rs = *(uint64_t *)(abCheck.data() + 8);

        printf("[*] State sector: magic=0x%08X sectors=%llu\n", rm, rs);
        
        if (rm != magic)
        {
            fprintf(stderr, "[-] ERROR: State sector validation is failed!\n");
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }
    }

    // password sector
    {
        clsDiskHandle disk;
        if (!disk.fnbOpen(szDrivePath, true))
        {
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }

        std::vector<uint8_t> pass_sector(SECTOR_SIZE, 0);
        uint32_t pass_magic = MAGIC_PASSWORD;
        memcpy(pass_sector.data(), &pass_magic, 4);
        pass_sector[4] = (uint8_t)szPassword.size();
        memcpy(pass_sector.data() + 5, szPassword.data(), szPassword.size());

        if (!disk.fnbWriteSectors(PASSWORD_SECTOR, pass_sector))
        {
            fprintf(stderr, "[-] ERORR: Password sector write failed.\n");
            if (bMount)
                fnUnmountESP(szESP);

            return false;
        }

        printf("[+] Password sector has been written.\n");

        // zero password from memory
        volatile char *p = (volatile char *)szPassword.data();
        for (size_t i = 0; i < szPassword.size(); i++)
            p[i] = 0;
    }

    // unmount if it is mounted
    printf("[*] Finalizing...\n");
    
    if (bMount)
        fnUnmountESP(szESP);

    printf("[+] ===== [UEFI Installation is completed!] =====\n");
    printf("[+] In next boot: Custom EFI application runs first.\n");

    return true;
}

bool fnbRestoreUEFI()
{
    printf("[*] UEFI restore\n");

    std::wstring szESP = fnFindESP();
    bool bMount = false;
    if (szESP.empty())
    {
        szESP = fnMountESP(0);
        bMount = true;
    }

    if (szESP.empty())
    {
        fprintf(stderr, "[-] ERROR: Cannot find ESP\n");
        return false;
    }

    std::wstring szBootDir = szESP + L"\\EFI\\Microsoft\\Boot\\";
    std::wstring szOriginalEfiPath = szBootDir + L"bootmgfw.efi";
    std::wstring szBackupPath = szBootDir + L"bootmgfw_original.efi";

    if (!fnbFileExists(szBackupPath))
    {
        fprintf(stderr, "[-] No backup found at %ls\n", szBackupPath.c_str());
        if (bMount)
            fnUnmountESP(szESP);

        return false;
    }

    SetFileAttributesW(szOriginalEfiPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!CopyFileW(szBackupPath.c_str(), szOriginalEfiPath.c_str(), FALSE))
    {
        fprintf(stderr, "[-] Restore failed (error %lu)\n", GetLastError());
        if (bMount)
            fnUnmountESP(szESP);

        return false;
    }

    printf("[+] Restore original bootmgfw.efi successfully!\n");
    if (bMount)
        fnUnmountESP(szESP);

    return true;
}
