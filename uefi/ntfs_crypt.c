// ntfs_crypt.c

#include "ntfs_crypt.h"
#include "ntfs.h"

EFI_GUID rngProtocolGuid = EFI_RNG_PROTOCOL_GUID;
static EFI_RNG_PROTOCOL *gRng = NULL;
static UINT32 prng_state = 0;

static const uint8_t KNOWN_PLAIN[32] = {
    'B','O','O','T','L','O','A','D',
    'E','R','V','E','R','I','F','Y',
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE
};

static void compute_tag(const uint8_t key[32], uint8_t tag[TAG_SIZE])
{
    uint8_t nonce[8] = {
        0xAB, 0xCD, 0xEF, 0x12,
        0x34, 0x56, 0x78, 0x9A,
    };

    Salsa20_Ctx ctx;
    salsa20_init(&ctx, key, nonce, 0);
    salsa20_encrypt(&ctx, KNOWN_PLAIN, tag, TAG_SIZE);
}

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
            Print(L"\nRead error!\n");
            for (int j = 0; j < 32; j++)
                key[j] = 0;

            return -1;
        }

        uint8_t nonce[8] = { 0 };
        nonce[0] = (uint8_t)i;
        nonce[1] = (uint8_t)(i >> 8);
        nonce[2] = (uint8_t)(i >> 16);
        nonce[3] = (uint8_t)(i >> 24);

        Salsa20_Ctx ctx;
        salsa20_init(&ctx, key, nonce, 0);
        salsa20_encrypt(&ctx, sector_buffer, out_buffer, 512);

        if (ntfs_write(mft_lba, 1, out_buffer) != 0)
        {
            Print(L"\nWrite error!\n");
            for (int j = 0; j < 32; j++)
                key[j] = 0;

            return -1;
        }

        if (i % 16 == 0)
            Print(L"#");
    }

    Print(L"]\n");

    validate_save_tag(key);

    for (int i = 0; i < 32; i++)
        key[i] = 0;

    return 0;
}

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba)
{
    uint8_t salt[SALT_SIZE];
    if (read_salt(salt) != 0)
        return -1;

    Print(L"Deriving key...\n");
    uint8_t key[32];
    kdf_derive(key, password, salt, KDF_ITERATIONS);

    Print(L"Validating key...\n");
    if (!validate_check_key(key))
    {
        Print(L"Validation failed: wrong password.\n");
        Print(L"MFT untouched.\n");

        for (int i = 0; i < 32; i++)
            key[i] = 0;

        return -1;
    }

    Print(L"Password is valdiated.\n");

    uint32_t mft_lba;
    if (get_mft_lba(partition_lba, &mft_lba) != 0)
    {
        for (int i = 0; i < 32; i++)
            key[i] = 0;

        return -1;
    }

    Print(L"Decrypting MFT [");

    static uint8_t sector_buffer[512];
    static uint8_t out_buffer[512];

    for (uint32_t i = 0; i < MFT_ENCRYPT_SECTORS; i++)
    {
        if (ntfs_read(mft_lba + i, 1, sector_buffer) != 0)
        {
            Print(L"\nRead error!\n");
            
            for (int i = 0; i < 32; i++)
                key[i] = 0;

            return -1;
        }

        uint8_t nonce[8] = { 0 };
        nonce[0] = (uint8_t)i;
        nonce[1] = (uint8_t)(i >> 8);
        nonce[2] = (uint8_t)(i >> 16);
        nonce[3] = (uint8_t)(i >> 24);

        Salsa20_Ctx ctx;
        salsa20_init(&ctx, key, nonce, 0);
        salsa20_decrypt(&ctx, sector_buffer, out_buffer, 512);

        if (ntfs_write(mft_lba + i, 1, out_buffer) != 0)
        {
            Print(L"\nWrite error!\n");
            for (int i = 0; i < 32; i++)
                key[i] = 0;

            return -1;
        }

        if (i % 16 == 0)
            Print(L"#");
    }

    Print(L"]\n");

    for (int i = 0; i < 32; i++)
        key[i] = 0;

    return 0;
}

int validate_save_tag(const uint8_t key[32])
{
    static uint8_t sector[512] = { 0 };

    sector[0] = VALIDATE_MAGIC_0;
    sector[1] = VALIDATE_MAGIC_1;
    sector[2] = VALIDATE_MAGIC_2;
    sector[3] = VALIDATE_MAGIC_3;

    compute_tag(key, sector + 4);

    if (ntfs_write(VALIDATION_SECTOR, 1, sector) != 0)
    {
        Print(L"validate_save_tag: write failed!\n");
        return -1;
    }

    Print(L"Validation tag is saved to sector %d\n", VALIDATION_SECTOR);

    return 0;
}

int validate_check_key(const uint8_t key[32])
{
    static uint8_t sector[512] = { 0 };
    if (ntfs_write(VALIDATION_SECTOR, 1, sector) != 0)
    {
        Print(L"validate_check_key: read failed!\n");
        return 0;
    }

    if (sector[0] != VALIDATE_MAGIC_0 || sector[1] != VALIDATE_MAGIC_1 || sector[2] != VALIDATE_MAGIC_2 || sector[3] != VALIDATE_MAGIC_3)
    {
        Print(L"Validation sector is not initialized!\n");
        return 0;
    }

    static uint8_t expected_tag[TAG_SIZE] = { 0 };
    compute_tag(key, expected_tag);

    uint8_t diff = 0;
    for (int i = 0; i < TAG_SIZE; i++)
        diff |= expected_tag[i] ^ sector[i+4];

    return diff == 0;
}
