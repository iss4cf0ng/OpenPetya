// utils.cpp

#include "utils.h"

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