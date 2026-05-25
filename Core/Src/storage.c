#include "storage.h"
#include <string.h>

/* STM32F401RC: Sector 5 starts at 0x08020000 (128 KB).
 * Code must fit in sectors 0-4 (≤128 KB) for this to be safe.       */
#define FLASH_SECTOR_5_ADDR  0x08020000UL

static const CredentialStore_t *const s_store =
    (const CredentialStore_t *)FLASH_SECTOR_5_ADDR;

uint8_t Storage_HasValid(void)
{
    return (s_store->magic == STORAGE_MAGIC);
}

void Storage_Load(CredentialStore_t *out)
{
    memcpy(out, s_store, sizeof(CredentialStore_t));
}

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
