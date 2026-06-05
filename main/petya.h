// petya.h

#ifndef PETYA_H
#define PETYA_H

#include <windows.h>
#include <winioctl.h>
#include <string>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <vector>

#define SECTOR_SIZE 512
#define MBR_BOOT_CODE_SIZE 446
#define MBR_FULL_SIZE 512
#define STAGE2_START_SECTOR 1

#define MAGIC_STATE 0x424F4F54UL
#define MAGIC_PASSWORD 0x50415353UL

#define PASSWORD_SECTOR 59
#define VALIDATION_SECTOR 60
#define BACKUP_MBR_SECTOR 63

#endif