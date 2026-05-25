#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define STORAGE_MAGIC  0xA55AA55AUL

/* 16 bytes total — written as 4 × 32-bit words to STM32F4 flash */
typedef struct {
    uint32_t magic;       /* 0xA55AA55A = valid credentials stored */
    uint8_t  uid[5];      /* RFID card UID                         */
    char     password[5]; /* 4-digit password + null terminator    */
    uint8_t  _pad[2];     /* padding to 16 bytes                   */
} CredentialStore_t;

uint8_t           Storage_HasValid(void);
void              Storage_Load(CredentialStore_t *out);
HAL_StatusTypeDef Storage_Save(const CredentialStore_t *in);
HAL_StatusTypeDef Storage_Erase(void);
