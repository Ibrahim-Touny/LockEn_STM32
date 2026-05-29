/**
 * @file    storage.c
 * @brief   Flash memory credential storage - reads/writes authentication
 *          factors (RFID UID, fingerprint, password) to STM32F4 flash sector 5.
 */

#include "Abstractions/storage.h"
#include <string.h>

/* STM32F401RC: Sector 5 starts at 0x08020000 (128 KB).
 * Code must fit in sectors 0-4 (≤128 KB) for this to be safe. */
#define FLASH_SECTOR_5_ADDR  0x08020000UL

static const CredentialStore_t *const s_store =
    (const CredentialStore_t *)FLASH_SECTOR_5_ADDR;

/**
 * @brief Check if valid credentials are stored in flash.
 *
 * Verifies the magic number to ensure credentials have been initialized.
 *
 * @return 1 if valid credentials found, 0 otherwise
 */
uint8_t Storage_HasValid(void)
{
    return (s_store->magic == STORAGE_MAGIC);
}

/**
 * @brief Load credentials from flash memory.
 *
 * @param out Output structure to fill with credential data
 */
void Storage_Load(CredentialStore_t *out)
{
    memcpy(out, s_store, sizeof(CredentialStore_t));
}

/**
 * @brief Erase the credential storage sector from flash.
 *
 * @return HAL_OK on success, HAL_ERROR on failure
 */
HAL_StatusTypeDef Storage_Erase(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = FLASH_SECTOR_5,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t sector_error = 0;
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();
    return st;
}

/**
 * @brief Save credentials to flash memory.
 *
 * Erases the sector first, then writes the credential structure word-by-word.
 *
 * @param in Pointer to credential structure to save
 * @return HAL_OK on success, HAL_ERROR on failure
 */
HAL_StatusTypeDef Storage_Save(const CredentialStore_t *in)
{
    HAL_StatusTypeDef st = Storage_Erase();
    if (st != HAL_OK) return st;

    HAL_FLASH_Unlock();

    const uint32_t *words = (const uint32_t *)in;
    uint32_t addr = FLASH_SECTOR_5_ADDR;

    for (int i = 0; i < (int)(sizeof(CredentialStore_t) / 4); i++)
    {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, (uint64_t)words[i]);
        if (st != HAL_OK) break;
        addr += 4;
    }

    HAL_FLASH_Lock();
    return st;
}
