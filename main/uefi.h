// uefi.h

#ifndef UEFI_H
#define UEFI_H

#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

std::wstring fnFindESP();
std::wstring fnMountESP(int nIdxDrive);
void fnUnmountESP(const std::wstring& szLetter);
bool fnbInstallUEFI(const std::wstring& szSrcEfiPath, const std::wstring& szPassword, const std::wstring& szDrivePath, UINT64 nTotalSectors);
bool fnbRestoreUEFI();

#endif