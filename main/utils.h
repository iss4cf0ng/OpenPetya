// utils.h

#ifndef UTILS_H
#define UTILS_H

#include <fstream>
#include <string>

bool fnbCopyFile(const std::wstring& szSrcPath, const std::wstring& szDstPath);
bool fnbFileExists(const std::wstring& szPath);

#endif