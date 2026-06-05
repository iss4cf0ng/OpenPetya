// uefi.h

#ifndef UEFI_H
#define UEFI_H

#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "utils.h"

#define BOOT_DIR L"\\EFI\\Microsoft\\Boot\\";
#define BOOTMGFW_PATH L":\\EFI\\Microsoft\\Boot\\botmgfw.efi"

/// @brief Find first EFI System Partition (ESP)
/// @return 
std::wstring fnFindESP();

/// @brief Mount ESP
/// @param nIdxDrive 
/// @return 
std::wstring fnMountESP(int nIdxDrive);

/// @brief Unmount ESP
/// @param szLetter 
void fnUnmountESP(const std::wstring& szLetter);

/// @brief Install UEFL payload
/// @param szSrcEfiPath 
/// @param szPassword 
/// @param szDrivePath 
/// @param nTotalSectors 
/// @return 
bool fnbInstallUEFI(const std::wstring& szSrcEfiPath, const std::string& szPassword, const std::wstring& szDrivePath, UINT64 nTotalSectors);

/// @brief Restore the original UEFI
/// @return 
bool fnbRestoreUEFI();

#endif