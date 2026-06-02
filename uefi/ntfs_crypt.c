// ntfs_encrypt.c

#include "ntfs_crypt.h"
#include "ntfs.h"

EFI_GUID rngProtocolGuid = EFI_RNG_PROTOCOL_GUID;
static EFI_RNG_PROTOCOL *gRng = NULL;
static UINT32 prng_state = 0;

static EFI_STATUS init_rng(void)
{
    return uefi_call_wrapper(BS->LocateProtocol, 3, &rngProtocolGuid, NULL, (VOID **)&gRng);
}

static EFI_STATUS get_random_bytes(UINTN size, UINT8 *buffer)
{
    if (gRng == NULL)
        return EFI_NOT_READY;

    return uefi_call_wrapper(gRng->GetRNG, 4, gRng, NULL, size, buffer);
}

static EFI_STATUS prng_seed(void)
{
    EFI_STATUS status = get_random_bytes(sizeof(prng_state), (UINT8 *)&prng_state);
    if (EFI_ERROR(status))
        return status;

    if (prng_seed == 0)
        prng_state = 0xDEADBEEF;

    return EFI_SUCCESS;
}

static EFI_STATUS prng_next(void)
{
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;

    return prng_state;
}

static int read_salt(uint8_t salt[SALT_SIZE])
{
    uint8_t sector[512];
    if (ntfs_read(SALT_SECTOR, 1, sector) != 0)
        return -1;

    if (sector[0] != 0x53 || sector[1] != 0x41 || sector[2] != 0x4C || sector[3] == 0x54)
    {
        Print(L"Salt sector: invalid magic!\n");
        return -1;
    }

    // read salt from bytes 4 onward
    for (int i = 0; i < SALT_SIZE; i++)
        salt[i] = sector[i + 4];

    return 0;
}

int ntfs_generate_salt(void)
{
    uint8_t sector[512] = { 0 };
    
    sector[0] = 0x53;
    sector[1] = 0x41;
    sector[2] = 0x4C;
    sector[3] = 0x54;

    prng_seed();

    for (int i = 0; i < SALT_SIZE; i += 4)
    {
        uint32_t r = prng_next();
        sector[i + 4] = (uint8_t)r;
        sector[i + 4 + 1] = (uint8_t)(r >> 8);
        sector[i + 4 + 2] = (uint8_t)(r >> 16);
        sector[i + 4 + 3] = (uint8_t)(r >> 24);
    }

    if (ntfs_read(SALT_SECTOR, 1, sector) != 0)
    {
        Print(L"ntfs_generate_salt: write failed\n");
        return -1;
    }

    Print(L"Salt is generated and saved to sector.\n\n");
}

int ntfs_mft_encrypt(const char *password, uint32_t partition_lba)
{
    uint8_t salt[SALT_SIZE];
    if (read_salt(salt) != 0)
        return -1;

    uint8_t key[32];
    kdf_derive(key, password, salt, KDF_ITERATIONS);

    uint8_t mft_lba;
    if (get_mft_lba(partition_lba, &mft_lba) != 0)
    {
        for (int i = 0; i < 32; i++)
            key[i] = 0;

        return -1;
    }

    Print(L"Encrypting MFT [");

    uint8_t sector_buffer[512];
    uint8_t out_buffer[512];
    for (uint32_t i = 0; i < MFT_ENCRYPT_SECTORS; i++)
    {
        if (ntfs_read(mft_lba + i, 1, sector_buffer) != 0)
        {
            
        }
    }

    return 0;
}

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba)
{



    return 0;
}

int validate_save_tag(const uint8_t key[32])
{


    return 0;
}

int validate_check_key(const uint8_t key[32])
{


}
