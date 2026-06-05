// utils.cpp

#include "utils.h"

clsDiskHandle::clsDiskHandle() = default;

clsDiskHandle::~clsDiskHandle()
{
    if (INVALID_HANDLE_VALUE != m_hFile)
        CloseHandle(m_hFile);
}

bool clsDiskHandle::fnbOpen(const std::wstring& szPath, bool bWriteAccess)
{
    DWORD dwAccess = bWriteAccess ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
    m_hFile = CreateFileW(
        szPath.c_str(),
        dwAccess,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_WRITE_THROUGH,
        nullptr
    );

    if (INVALID_HANDLE_VALUE == m_hFile)
    {
        DWORD hError = GetLastError();
        std::wcerr << L"Failed to open " << szPath << L" (error " << hError << L")\n";

        if (ERROR_ACCESS_DENIED == hError)
            std::cerr << " Please Run as Administrator\n";

        return false;
    }

    return true;
}

bool clsDiskHandle::fnbWriteSectors(LONGLONG nLBA, const std::vector<uint8_t>& abBuffer)
{
    LARGE_INTEGER nOffset;
    nOffset.QuadPart = nLBA * SECTOR_SIZE;

    if (!SetFilePointerEx(m_hFile, nOffset, nullptr, FILE_BEGIN))
    {
        std::wcerr << "Seek failed (error " << GetLastError() << ")\n";
        return false;
    }

    DWORD written = 0;
    if (!WriteFile(m_hFile, abBuffer.data(), (DWORD)abBuffer.size(), &written, nullptr) || written != abBuffer.size())
    {
        std::wcerr << "Write failed (error " << GetLastError() << ")\n";
        return false;
    }

    FlushFileBuffers(m_hFile);
    return true;
}

bool clsDiskHandle::fnbReadSectors(LONGLONG nLBA, DWORD nCount, std::vector<uint8_t>& abBuffer)
    {
        LARGE_INTEGER nOffset;
        nOffset.QuadPart = nLBA * SECTOR_SIZE;

        if (!SetFilePointerEx(m_hFile, nOffset, nullptr, FILE_BEGIN))
        {
            std::cerr << "Seek failed (error: " << GetLastError() << ")\n";
            return false;
        }

        abBuffer.resize((size_t)(nCount * SECTOR_SIZE));
        DWORD nRead = 0;
        if (!ReadFile(m_hFile, abBuffer.data(), (DWORD)abBuffer.size(), &nRead, nullptr) || nRead != abBuffer.size())
        {
            std::wcerr << "Read failed (error: " << GetLastError() << ")\n";
            return false;
        }

        return true;
    }

bool fnbCopyFile(const std::wstring& szSrcPath, const std::wstring& szDstPath)
{
    if (!CopyFileW(szSrcPath.c_str(), szDstPath.c_str(), FALSE))
    {
        fprintf(stderr, "CopyFile failed, error = %lu\n", GetLastError());
        return false;
    }

    return true;
}

bool fnbFileExists(const std::wstring& szPath)
{
    return INVALID_FILE_ATTRIBUTES != GetFileAttributesW(szPath.c_str());
}

ULONG fnHexdump(const uint8_t* abBuffer, size_t nLength, size_t nOffset = 0)
{
    ULONG nResult = 0;
    for (ULONG i = 0; i < nLength; i += 16)
    {
        printf("%08X |", i);

        nResult += 16;
        for (ULONG j = 0; j < 16; j++)
        {
            if (i + j < nLength)
            {
                nResult += printf(" %02X", abBuffer[i + j]);
            }
            else
            {
                nResult += printf(" 00");
            }
        }

        nResult += printf(" | ");
        for (ULONG j = 0; j < 16; j++)
        {
            if (i + j < nLength)
            {
                UCHAR k = abBuffer[i + j];
                UCHAR c = k < 32 || k > 127 ? '.' : k;
                nResult += printf("%c", c);
            }
            else
            {
                nResult += printf(" ");
            }
        }

        nResult += printf("\n");
    }

    return nResult;
}

DWORD fnGetWindowsVersion(int nMajor, int nMinor)
{
    if (nMajor == 10)
    {
        return OS_Windows10
    }
    else if (nMajor == 6)
    {
        if (nMinor == 3 || nMinor == 2)
        {
            return OS_Windows8;
        }
        else if (nMinor == 1)
        {
            return OS_Windows7;
        }
    }

    return -1;
}
