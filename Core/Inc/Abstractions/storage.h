/*
 * ============================================================================
 * storage.h
 * ============================================================================
 * Flash storage for saved credentials (RFID card UID and password)
 * Provides functions to read, write, and erase stored authentication data
 * ============================================================================
 */

#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
 * Magic value to indicate valid stored credentials
 * If stored magic == STORAGE_MAGIC, the credentials are valid
 */
#define STORAGE_MAGIC  0xA55AA55AUL

/*
 * Credential storage structure (exactly 16 bytes)
 * Written to STM32F4 flash as 4 consecutive 32-bit words
 *
 * Layout (16 bytes total):
 *   Bytes 0-3:   magic value (0xA55AA55A if valid)
 *   Bytes 4-8:   RFID card UID (5 bytes)
 *   Bytes 9-13:  4-digit password + null terminator (5 bytes)
 *   Bytes 14-15: padding for alignment
 */
typedef struct {
    uint32_t magic;       /* 0xA55AA55A = valid credentials stored */
    uint8_t  uid[5];      /* RFID card UID (5 bytes)               */
    char     password[5]; /* 4-digit password + null terminator    */
    uint8_t  _pad[2];     /* padding to reach 16 bytes             */
} CredentialStore_t;

/*
 * Check if valid credentials are currently stored in flash
 * Returns: 1 if valid credentials exist, 0 otherwise
 */
uint8_t           Storage_HasValid(void);

/*
 * Load stored credentials from flash
 * out: pointer to CredentialStore_t to fill with loaded data
 * Note: does not check validity; use Storage_HasValid() first
 */
void              Storage_Load(CredentialStore_t *out);

/*
 * Save credentials to flash
 * in: pointer to CredentialStore_t to write
 * Returns: HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef Storage_Save(const CredentialStore_t *in);

/*
 * Erase stored credentials from flash
 * Clears the magic value to invalidate any stored data
 * Returns: HAL_OK if successful, HAL_ERROR otherwise
 */
HAL_StatusTypeDef Storage_Erase(void);
