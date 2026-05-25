// r307.c
#include "r307.h"
#include "display.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static volatile uint8_t s_r307_ui_log_enabled = 0;

void R307_SetUiLogEnabled(uint8_t enable)
{
    s_r307_ui_log_enabled = enable ? 1u : 0u;
}

void R307_UiLog(const char *fmt, ...)
{
    if (!s_r307_ui_log_enabled) return;

    char buf[17];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Display_Line(1, buf);
}

// Helper: build command packet into out_buf, return packet length
// out_buf must be large enough (> 32 bytes recommended)
static uint16_t R307_BuildCommand(uint8_t *out_buf, uint8_t instruction,
                                  const uint8_t *params, uint16_t params_len)
{
    uint16_t idx = 0;

    // Header + address
    out_buf[idx++] = 0xEF;
    out_buf[idx++] = 0x01;
    out_buf[idx++] = 0xFF;
    out_buf[idx++] = 0xFF;
    out_buf[idx++] = 0xFF;
    out_buf[idx++] = 0xFF;

    // Packet identifier: command packet
    out_buf[idx++] = 0x01;

    // Length = Instruction(1) + params_len + checksum(2)
    uint16_t length_field = 1 + params_len + 2;
    out_buf[idx++] = (length_field >> 8) & 0xFF;
    out_buf[idx++] = length_field & 0xFF;

    // Instruction
    out_buf[idx++] = instruction;

    // Params
    if (params_len && params != NULL) {
        memcpy(&out_buf[idx], params, params_len);
        idx += params_len;
    }

    // Compute checksum: sum of bytes from packet identifier (index 6) to last data byte
    uint16_t sum = 0;
    for (uint16_t i = 6; i < idx; ++i) sum += out_buf[i];

    // Append checksum (2 bytes)
    out_buf[idx++] = (sum >> 8) & 0xFF;
    out_buf[idx++] = sum & 0xFF;

    return idx;
}

// Send command and receive response. response_max_len must be size of response buffer.
// This function will fill response[] and return HAL_OK on UART success (even if the R307 returns error codes).
static HAL_StatusTypeDef R307_SendCommand(uint8_t *cmd, uint16_t cmd_len,
                                   uint8_t *response, uint16_t response_max_len)
{
    HAL_StatusTypeDef status;

    // Transmit full command
    status = HAL_UART_Transmit(&R307_UART, cmd, cmd_len, 500);
    if (status != HAL_OK) {
    	DB_LOG("UART transmit failed");
        return status;
    }

    // Receive header (9 bytes): EF01 + Addr(4) + PID + LEN_H + LEN_L
    status = HAL_UART_Receive(&R307_UART, response, 9, 1000);
    if (status != HAL_OK) {
    	DB_LOG("UART receive failed (header)");
        return status;
    }

    // Get length field (this length already includes checksum)
    uint16_t payload_len = ((uint16_t)response[7] << 8) | response[8];
    uint16_t total_len = 9 + payload_len; // header + payload_len (payload includes checksum)

    if (total_len > response_max_len) {
    	DB_LOG("Response too large (%d bytes, buffer max %d)", total_len, response_max_len);
        // Drain remaining bytes if needed? For now return error
        return HAL_ERROR;
    }

    // Read rest of packet (payload_len bytes)
    if (payload_len > 0) {
        status = HAL_UART_Receive(&R307_UART, &response[9], payload_len, 1000);
        if (status != HAL_OK) {
        	DB_LOG("UART receive failed (payload)");
            return status;
        }
    }

    return HAL_OK;
}

// GenImg - Capture fingerprint image (no params / no toggles). The R307 returns codes:
// 0x00 success, other values indicate errors (e.g., no finger, movement, etc.)
static HAL_StatusTypeDef R307_CaptureFinger(void)
{
    uint8_t cmd[16];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x01, NULL, 0);

    uint8_t resp[32] = {0};
    DB_LOG("Capturing finger...");
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        if (resp[9] == 0x00) {
        	DB_LOG("Finger image captured");
            return HAL_OK;
        } else {
        	DB_LOG("Capture failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

// Img2Tz - Convert image to characteristic file (buffer 1 or 2)
static HAL_StatusTypeDef R307_Image2Tz(uint8_t buffer_id)
{
    if (buffer_id != 1 && buffer_id != 2) {
    	DB_LOG("Invalid buffer id %d (must be 1 or 2)", buffer_id);
        return HAL_ERROR;
    }
    uint8_t params[1] = { buffer_id };
    uint8_t cmd[16];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x02, params, 1);

    uint8_t resp[32] = {0};
    DB_LOG("Converting image to char file (buffer %d)...", buffer_id);
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        if (resp[9] == 0x00) {
        	DB_LOG("Image -> char (buffer %d) OK", buffer_id);
            return HAL_OK;
        } else {
        	DB_LOG("Image2Tz failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

// RegModel - Generate template from char files in buffer1 & buffer2 (merge)
static HAL_StatusTypeDef R307_GenerateTemplate(void)
{
    // instruction 0x05, no params
    uint8_t cmd[16];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x05, NULL, 0);

    uint8_t resp[32] = {0};
    DB_LOG("Generating template from buffer1 & buffer2...");
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        if (resp[9] == 0x00) {
            DB_LOG("Template generated (model created)");
            return HAL_OK;
        } else {
            DB_LOG("RegModel failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

// Store template (store model from buffer into flash page)
// buffer_id usually 1, page_id is 0..(max-1)
static HAL_StatusTypeDef R307_StoreTemplate(uint8_t buffer_id, uint16_t page_id)
{
    uint8_t params[3] = {
        buffer_id,
        (uint8_t)((page_id >> 8) & 0xFF),
        (uint8_t)(page_id & 0xFF)
    };
    uint8_t cmd[32];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x06, params, 3);

    uint8_t resp[32] = {0};
    DB_LOG("Storing template from buffer %d to page %d...", buffer_id, page_id);
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        if (resp[9] == 0x00) {
            DB_LOG("Template stored at ID=%d", page_id);
            return HAL_OK;
        } else {
            DB_LOG("Store failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

// Search: search buffer 1 over a range (start_page, page_num)
static HAL_StatusTypeDef R307_SearchDatabase(uint16_t *out_page_id, uint16_t *out_score,
                                      uint16_t start_page, uint16_t page_num)
{
    // params: bufferID(1), startPage(2), pageNum(2)
    uint8_t params[5] = {
        0x01, // buffer 1
        (uint8_t)((start_page >> 8) & 0xFF),
        (uint8_t)(start_page & 0xFF),
        (uint8_t)((page_num >> 8) & 0xFF),
        (uint8_t)(page_num & 0xFF)
    };
    uint8_t cmd[32];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x04, params, sizeof(params));

    uint8_t resp[32] = {0};
    DB_LOG("Searching database (start=%d, num=%d)...", start_page, page_num);
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        // resp[9] == confirmation; if 0x00, next bytes are pageid(2) score(2)
        if (resp[9] == 0x00) {
            if (out_page_id) *out_page_id = ((uint16_t)resp[10] << 8) | resp[11];
            if (out_score)   *out_score   = ((uint16_t)resp[12] << 8) | resp[13];
            DB_LOG("Match found: ID=%d Score=%d", (out_page_id?*out_page_id:0), (out_score?*out_score:0));
            return HAL_OK;
        } else if (resp[9] == 0x09) {
            DB_LOG("No matching fingerprint found (code=0x09)");
            return HAL_ERROR;
        } else {
            DB_LOG("Search failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

// Delete templates starting from page_id, count entries
static HAL_StatusTypeDef R307_DeleteTemplate(uint16_t page_id, uint16_t count)
{
    uint8_t params[4] = {
        (uint8_t)((page_id >> 8) & 0xFF),
        (uint8_t)(page_id & 0xFF),
        (uint8_t)((count >> 8) & 0xFF),
        (uint8_t)(count & 0xFF)
    };
    uint8_t cmd[32];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x0C, params, sizeof(params));

    uint8_t resp[32] = {0};
    DB_LOG("Deleting %d template(s) starting at ID=%d...", count, page_id);
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        if (resp[9] == 0x00) {
            DB_LOG("Delete OK");
            return HAL_OK;
        } else {
            DB_LOG("Delete failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

// Clear all templates (empty library) - instruction 0x0D (Empty)
static HAL_StatusTypeDef R307_ClearAllTemplates(void)
{
    uint8_t cmd[16];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x0D, NULL, 0);

    uint8_t resp[32] = {0};
    DB_LOG("Clearing all templates...");
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        if (resp[9] == 0x00) {
            DB_LOG("All templates cleared");
            return HAL_OK;
        } else {
            DB_LOG("Clear all failed (code=0x%02X)", resp[9]);
            return HAL_ERROR;
        }
    }
    return HAL_ERROR;
}

static HAL_StatusTypeDef WaitForFingerRemoval(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (1)
    {
        uint8_t resp[12];

        memset(resp, 0, sizeof(resp));

        // Send capture finger command, do not spam logs
        if (R307_SendCommand((uint8_t[]){
            0xEF,0x01,0xFF,0xFF,0xFF,0xFF,
            0x01,0x00,0x03,0x01,0x00,0x05
        }, 12, resp, sizeof(resp)) != HAL_OK)
        {
            osDelay(100);
            continue;
        }

        // resp[9] = confirmation code
        if (resp[9] == 0x02)  // 0x02 = no finger detected
        {
            FP_LOG("Finger removed");
            return HAL_OK;
        }

        // Check timeout
        if (HAL_GetTick() - start > timeout_ms)
        {
            FP_LOG("Timeout remove");
            return HAL_TIMEOUT;
        }

        osDelay(300);  // Wait before next check
    }
}

/**
 * @brief Wait until a finger is placed on the sensor.
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return HAL_StatusTypeDef HAL_OK if finger detected, HAL_TIMEOUT if timeout occurs.
 */
static HAL_StatusTypeDef WaitForFingerPlacement(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (1)
    {
        uint8_t resp[12];

        memset(resp, 0, sizeof(resp));

        // Send capture finger command
        if (R307_SendCommand((uint8_t[]){
            0xEF,0x01,0xFF,0xFF,0xFF,0xFF,
            0x01,0x00,0x03,0x01,0x00,0x05
        }, 12, resp, sizeof(resp)) != HAL_OK)
        {
            osDelay(100);
            continue;
        }

        // resp[9] = confirmation code
        if (resp[9] == 0x00)  // 0x00 = finger detected
        {
            FP_LOG("Finger detected");
            return HAL_OK;
        }

        // Check timeout
        if (HAL_GetTick() - start > timeout_ms)
        {
            FP_LOG("Timeout place");
            return HAL_TIMEOUT;
        }

        osDelay(300);  // Wait before next check
    }
}

static HAL_StatusTypeDef R307_CaptureWithTimeout(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (1)
    {
        if (R307_CaptureFinger() == HAL_OK) return HAL_OK;
        if ((HAL_GetTick() - start) > timeout_ms) return HAL_TIMEOUT;
        osDelay(200);
    }
}


/* ---------- High-level helper flows ---------- */

// Enroll: capture twice -> convert -> generate model -> store
HAL_StatusTypeDef R307_Enroll(uint16_t page_id)
{
    FP_LOG("Enroll FP");
    FP_LOG("Enroll ID=%d", page_id);

    // 1st capture
    FP_LOG("Place finger...");
    if (WaitForFingerPlacement(20000) != HAL_OK) {
    	return HAL_TIMEOUT;
    }

    FP_LOG("Capturing...");
    if (R307_CaptureWithTimeout(5000) != HAL_OK) {
        FP_LOG("Capture timeout");
        return HAL_TIMEOUT;
    }
    if (R307_Image2Tz(1) != HAL_OK) {
        FP_LOG("Conv 1 failed");
        return HAL_ERROR;
    }
    FP_LOG("1st img OK");

    // Wait until finger is removed (with timeout)
    FP_LOG("Remove finger...");

    if (WaitForFingerRemoval(10000) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    // 2nd capture
    FP_LOG("Place again...");
    if (WaitForFingerPlacement(20000) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    FP_LOG("Capturing 2nd...");
    if (R307_CaptureWithTimeout(5000) != HAL_OK) {
        FP_LOG("Capture timeout");
        return HAL_TIMEOUT;
    }

    if (R307_Image2Tz(2) != HAL_OK) {
        FP_LOG("Conv 2 failed");
        return HAL_ERROR;
    }
    FP_LOG("2nd img OK");

    FP_LOG("Remove finger...");

    if (WaitForFingerRemoval(10000) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    // Merge buffers into template
    if (R307_GenerateTemplate() != HAL_OK) {
        FP_LOG("Merge failed");
        return HAL_ERROR;
    }
    FP_LOG("Template merged");

    // Store model at page_id (store from buffer 1)
    if (R307_StoreTemplate(1, page_id) != HAL_OK) {
        FP_LOG("Store failed");
        return HAL_ERROR;
    }

    FP_LOG("Stored ID=%d", page_id);
    return HAL_OK;
}


// Verify: capture -> convert -> search over whole library (start 0, num 1000)
HAL_StatusTypeDef R307_Verify(uint16_t *out_page_id, uint16_t *out_score)
{
	HAL_StatusTypeDef status;

    FP_LOG("Verify FP");
    FP_LOG("Place finger...");
    if (WaitForFingerPlacement(20000) != HAL_OK) {
    	return HAL_TIMEOUT;
    }

    FP_LOG("Capturing...");
    if (R307_CaptureWithTimeout(5000) != HAL_OK) {
        FP_LOG("Capture timeout");
        return HAL_TIMEOUT;
    }
    if (R307_Image2Tz(1) != HAL_OK) return HAL_ERROR;

    // Search entire library; change range if you have different capacity
    uint16_t page_id = 0, score = 0;
    FP_LOG("Searching...");
    if (R307_SearchDatabase(&page_id, &score, 0x0000, 0x03E8) == HAL_OK) {
        FP_LOG("Match ID=%d", page_id);
        if (out_page_id) *out_page_id = page_id;
        if (out_score)   *out_score   = score;
        status = HAL_OK;
    }
    else {
    	status = HAL_ERROR;
    }

    FP_LOG("Remove finger...");

//       if (WaitForFingerRemoval(10000) != HAL_OK) {
//           status = HAL_TIMEOUT;
//       }
    return status;
}

// Verify Password (default password value is set here; change if your module uses different)
HAL_StatusTypeDef R307_VerifyPassword(uint32_t password)
{
    // password: 32-bit (most modules default 0x00000000 or 0x00000001)
    uint8_t params[4] = {
        (password >> 24) & 0xFF,
        (password >> 16) & 0xFF,
        (password >> 8)  & 0xFF,
        (password >> 0)  & 0xFF
    };
    uint8_t cmd[32];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x13, params, 4);

    uint8_t resp[32] = {0};
    DB_LOG("Verifying sensor password...");
    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        // resp[6] = packet identifier (0x07 = ACK), resp[9] = confirmation code
        if (resp[9] == 0x00) {
            FP_LOG("FP sensor OK!");
            return HAL_OK;
        }
        DB_LOG("Verify failed (confirmation=0x%02X)", resp[9]);
        return HAL_ERROR;
    }
    DB_LOG("UART timeout while waiting for verify response");
    return HAL_ERROR;
}

// Set a new password on the R307 module
HAL_StatusTypeDef R307_SetPassword(uint32_t new_password)
{
    // Convert 32-bit password to byte array (big-endian)
    uint8_t params[4] = {
        (new_password >> 24) & 0xFF,
        (new_password >> 16) & 0xFF,
        (new_password >> 8)  & 0xFF,
        (new_password >> 0)  & 0xFF
    };

    uint8_t cmd[32];
    uint16_t cmd_len = R307_BuildCommand(cmd, 0x12, params, 4); // 0x12 = set password

    uint8_t resp[32] = {0};
    DB_LOG("Setting new sensor password...");

    if (R307_SendCommand(cmd, cmd_len, resp, sizeof(resp)) == HAL_OK) {
        // resp[6] = packet identifier (0x07 = ACK)
        // resp[9] = confirmation code (0x00 = success)
        if (resp[6] == 0x07 && resp[9] == 0x00) {
            FP_LOG("Password set!");
            return HAL_OK;
        }
        DB_LOG("Set password failed (confirmation=0x%02X)", resp[9]);
        return HAL_ERROR;
    }

    DB_LOG("UART timeout while waiting for set password response");
    return HAL_ERROR;
}

HAL_StatusTypeDef R307_ClearAllFingerprints(void)
{
	return R307_ClearAllTemplates();
}

HAL_StatusTypeDef R307_DeleteFingerprints(uint16_t from_ID, uint16_t num_fp)
{
	return R307_DeleteTemplate(from_ID, num_fp);
}
