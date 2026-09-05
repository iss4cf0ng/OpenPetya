// OpenPetya.cpp

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <termios.h>

const size_t SECTOR_SIZE = 512;
const int BACKUP_MBR_SECTOR = 63;
const int STAGE2_START_SECTOR = 34;
const uint32_t MBR_BOOT_CODE_SIZE = 446;



int main()
{

}