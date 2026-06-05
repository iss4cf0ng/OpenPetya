// utils.h

#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <fstream>
#include <string>

#include "petya.h"

#define OS_Windows7 0x01
#define OS_Windows8 0x02
#define OS_Windows10 0x03

/// @brief Disk handling class
class clsDiskHandle
{
public:
    HANDLE m_hFile = INVALID_HANDLE_VALUE;

    clsDiskHandle();
    ~clsDiskHandle();

    /// @brief Open file
    /// @param szPath Destination path
    /// @param bWriteAccess Enable Read and Write
    /// @return 
    bool fnbOpen(const std::wstring& szPath, bool bWriteAccess);

    /// @brief Read specified sector
    /// @param nLBA 
    /// @param nCount 
    /// @param abBuffer 
    /// @return 
    bool fnbReadSectors(LONGLONG nLBA, DWORD nCount, std::vector<uint8_t>& abBuffer);

    /// @brief 
    /// @param nLBA 
    /// @param abBuffer 
    /// @return 
    bool fnbWriteSectors(LONGLONG nLBA, const std::vector<uint8_t>& abBuffer);
};

/// @brief 
/// @param szSrcPath 
/// @param szDstPath 
/// @return 
bool fnbCopyFile(const std::wstring& szSrcPath, const std::wstring& szDstPath);

/// @brief 
/// @param szPath 
/// @return 
bool fnbFileExists(const std::wstring& szPath);

/// @brief 
/// @param abBuffer 
/// @param nLength 
/// @param nOffset 
/// @return 
ULONG fnHexdump(const uint8_t* abBuffer, size_t nLength, size_t nOffset = 0);

/// @brief 
/// @param nMajor 
/// @param nMinor 
/// @return 
DWORD fnGetWindowsVersion(int nMajor, int nMinor);

#endif