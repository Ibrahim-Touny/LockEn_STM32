#include "tamper_task.h"
#include "mpu6050.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"

#define MPU_ACCEL_THRESHOLD  1200U  /* ~0.29 g  (AFS=8G, 4096 LSB/g)       */
#define MPU_GYRO_THRESHOLD   2000U  /* ~61 dps  (FS=1000dps, 32.8 LSB/dps)  */
#define MPU_POLL_MS          200U

const osThreadAttr_t tamperTask_attr = {
    .name       = "TamperTask",
    .stack_size = 512,
    .priority   = (osPriority_t) osPriorityBelowNormal,
};

QueueHandle_t    g_tamperEvt = NULL;
volatile uint8_t g_mpu_ok    = 0;

extern I2C_HandleTypeDef hi2c2;

static uint16_t abs_i16(int16_t v)
{
    return (v < 0) ? (uint16_t)(-v) : (uint16_t)v;
}

void TamperTask(void *argument)
{
    g_tamperEvt = xQueueCreate(4, sizeof(uint8_t));

    /* Wait for AuthTask to finish MPU init and set g_mpu_ok */
    while (!g_mpu_ok) osDelay(50);

    /* Take baseline after MPU is ready */
    int16_t accel_prev[3] = {0};
    int16_t gyro_prev[3]  = {0};
    MPU6050_getAccelValue(&hi2c2, accel_prev);
    MPU6050_getGyroValue(&hi2c2, gyro_prev);

    for (;;)
    {
        osDelay(MPU_POLL_MS);

        if (!g_mpu_ok) continue;

        int16_t accel_now[3], gyro_now[3];
        MPU6050_getAccelValue(&hi2c2, accel_now);
        MPU6050_getGyroValue(&hi2c2, gyro_now);

        uint8_t moved = 0;
        for (int i = 0; i < 3; i++)
        {
            if (abs_i16((int16_t)(accel_now[i] - accel_prev[i])) > MPU_ACCEL_THRESHOLD) moved = 1;
            if (abs_i16((int16_t)(gyro_now[i]  - gyro_prev[i]))  > MPU_GYRO_THRESHOLD)  moved = 1;
            accel_prev[i] = accel_now[i];
            gyro_prev[i]  = gyro_now[i];
        }

        if (moved)
        {
            uint8_t evt = 1;
            xQueueSend(g_tamperEvt, &evt, 0);
        }
    }
}
