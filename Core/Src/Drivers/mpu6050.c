/**
 * @file    mpu6050.c
 * @brief   MPU6050 6-axis motion sensor driver (accelerometer + gyroscope)
 *          for tamper detection and motion monitoring.
 */

#include "Drivers/mpu6050.h"

/**
 * @brief Scan I2C bus to find MPU6050 device address.
 *
 * Probes I2C addresses 1-127 to locate the sensor.
 *
 * @param hi2cx I2C handle (typically hi2c2)
 * @return 8-bit I2C address on success, -1 if not found
 */
int MPU6050_ScanDeviceID(I2C_HandleTypeDef *hi2cx){
	/* HAL expects the 7-bit address left-shifted by 1.
	 * MPU6050 is typically at 0x68 (AD0=0) => 0xD0, or 0x69 (AD0=1) => 0xD2.
	 */
	for (uint8_t addr7 = 1; addr7 < 0x7F; addr7++)
	{
		uint16_t addr8 = (uint16_t)(addr7 << 1);
		if (HAL_I2C_IsDeviceReady(hi2cx, addr8, 1, TIMEOUT) == HAL_OK)
		{
			return (int)addr8;
		}
	}
	return -1;
}


/**
 * @brief Read one or more bytes from an MPU6050 register.
 *
 * @param hi2cx I2C handle
 * @param registerAddress Register address
 * @param sizeofData Number of bytes to read
 * @param dataBuffer Output buffer
 * @return READ_SUCCESS or READ_FAIL
 */
static MPU6050ReadStatus MPU6050_ReadRegisterData(I2C_HandleTypeDef *hi2cx, uint16_t registerAddress, uint16_t sizeofData, uint8_t *dataBuffer){
	if(HAL_I2C_Mem_Read(hi2cx, MPU6050_DEVICE_ADDRESS, registerAddress, 1, dataBuffer, sizeofData, TIMEOUT)==HAL_OK){
		return READ_SUCCESS;
	}
	return READ_FAIL;
}

/**
 * @brief Write a value to an MPU6050 register.
 *
 * @param hi2cx I2C handle
 * @param registerAddress Register address
 * @param value Value to write
 * @return WRITE_SUCCESS or WRITE_FAIL
 */
static MPU6050WriteStatus MPU6050_WriteRegisterData(I2C_HandleTypeDef *hi2cx, uint16_t registerAddress, uint16_t value){
	uint8_t data[2]={0};
	data[0]=registerAddress;
	data[1]=value;

	if(HAL_I2C_Master_Transmit(hi2cx, MPU6050_DEVICE_ADDRESS, data, sizeof(data), TIMEOUT)==HAL_OK){
		return WRITE_SUCCESS;
	}
	return WRITE_FAIL;
}

/**
 * @brief Initialize the MPU6050 sensor with specified ranges.
 *
 * Verifies device ID, configures accelerometer and gyroscope full scales.
 *
 * @param hi2cx I2C handle
 * @param AFS_SEL Accelerometer full-scale range (0=2G, 1=4G, 2=8G, 3=16G)
 * @param FS_SEL Gyroscope full-scale range (0=250dps, 1=500dps, 2=1000dps, 3=2000dps)
 * @return INIT_SUCCESS or INIT_FAIL
 */
MPU6050InitStatus MPU6050_Init(I2C_HandleTypeDef *hi2cx, uint8_t AFS_SEL, uint8_t FS_SEL){
	uint8_t dataBuffer=0;
	if (MPU6050_ReadRegisterData(hi2cx, MPU6050_REG_WHO_AM_I, 1, &dataBuffer) != READ_SUCCESS)
	{
		return INIT_FAIL;
	}
	if(dataBuffer != 0x68)
	{
		return INIT_FAIL;
	}


	uint8_t tempReg = 0;

	PowerManagementRegister_t powerManagement={0};
	powerManagement.ClkSel = 0;
	powerManagement.Temp_Dis = 0;
	powerManagement.Reserved = 0;
	powerManagement.Cycle = 0;
	powerManagement.Sleep = 0;
	powerManagement.Device_Reset = 0;
	tempReg=*((uint8_t*)&powerManagement);
	MPU6050_WriteRegisterData(hi2cx, MPU6050_REG_PWR_MGMT_1, tempReg);
	///////////////////////////////////////////////////////////////////////////////////////
	AccelConfigRegister_t accelConfig = {0};
	accelConfig.Reserved = 0;
	accelConfig.AFS_Sel =AFS_SEL;
	accelConfig.ZA_ST = 0;
	accelConfig.YA_ST = 0;
	accelConfig.XA_ST = 0;
	tempReg=*((uint8_t*)&accelConfig);
	MPU6050_WriteRegisterData(hi2cx, MPU6050_REG_ACCEL_CONFIG, tempReg);
	////////////////////////////////////////////////////////////////////////////////////////
	GyroConfigRegister_t gyroConfig = {0};
	gyroConfig.Reserved = 0;
	gyroConfig.FS_Sel = FS_SEL;
	gyroConfig.ZG_ST = 0;
	gyroConfig.YG_ST = 0;
	gyroConfig.XG_ST = 0;
	tempReg=*((uint8_t*)&gyroConfig);
	MPU6050_WriteRegisterData(hi2cx, MPU6050_REG_GYRO_CONFIG, tempReg);
	////////////////////////////////////////////////////////////////////////////////////////

	return INIT_SUCCESS;
}


/**
 * @brief Read 3-axis acceleration values from the sensor.
 *
 * @param hi2cx I2C handle
 * @param accelData Output array [X, Y, Z] in raw sensor units
 */
void MPU6050_getAccelValue(I2C_HandleTypeDef *hi2cx, int16_t *accelData){
	uint8_t data[6]={0};
	MPU6050_ReadRegisterData(hi2cx, MPU6050_REG_ACCEL_XOUT_H, 6, data);
	accelData[0]=(int16_t)(data[0]<<8 | data[1]);
	accelData[1]=(int16_t)(data[2]<<8 | data[3]);
	accelData[2]=(int16_t)(data[4]<<8 | data[5]);
}



/**
 * @brief Read 3-axis gyroscope values from the sensor.
 *
 * @param hi2cx I2C handle
 * @param gyroData Output array [X, Y, Z] in raw sensor units
 */
void MPU6050_getGyroValue(I2C_HandleTypeDef *hi2cx, int16_t *gyroData){
	uint8_t data[6]={0};
	MPU6050_ReadRegisterData(hi2cx, MPU6050_REG_GYRO_XOUT_H, 6, data);
	gyroData[0]=(int16_t)(data[0]<<8 | data[1]);
	gyroData[1]=(int16_t)(data[2]<<8 | data[3]);
	gyroData[2]=(int16_t)(data[4]<<8 | data[5]);
}

/**
 * @brief Read temperature value from the sensor.
 *
 * @param hi2cx I2C handle
 * @param tempData Output raw temperature value
 * @return Temperature in Celsius as a float
 */
float MPU6050_getTempValue(I2C_HandleTypeDef *hi2cx, int16_t *tempData){
	uint8_t data[2]={0};
	float temperature=0;
	MPU6050_ReadRegisterData(hi2cx, MPU6050_REG_TEMP_OUT_H, 2, data);
	*tempData=(int16_t)(data[0]<<8 | data[1]);
	temperature=(float)*tempData/340.0 + 36.53;
	return temperature;
}


/**
 * @brief Convert raw acceleration values to physical units (g).
 *
 * @param accelData Raw acceleration array [X, Y, Z]
 * @param AFS_SEL Accelerometer range setting used at init
 * @param accelDataIng Output array [X, Y, Z] in grams (float)
 */
void MPU6050_getAccelIng(int16_t *accelData, uint8_t AFS_SEL, float *accelDataIng)
{
	if(AFS_SEL == 0x00){
		accelDataIng[0] = (float) accelData[0] / 16384.0;
		accelDataIng[1] = (float) accelData[1] / 16384.0;
		accelDataIng[2] = (float) accelData[2] / 16384.0;
	}
	else if(AFS_SEL == 0x01){
		accelDataIng[0] = (float) accelData[0] / 8192.0;
		accelDataIng[1] = (float) accelData[1] / 8192.0;
		accelDataIng[2] = (float) accelData[2] / 8192.0;
	}
	else if(AFS_SEL == 0x02){
		accelDataIng[0] = (float) accelData[0] / 4096.0;
		accelDataIng[1] = (float) accelData[1] / 4096.0;
		accelDataIng[2] = (float) accelData[2] / 4096.0;
	}
	else if(AFS_SEL == 0x03){
		accelDataIng[0] = (float) accelData[0] / 2048.0;
		accelDataIng[1] = (float) accelData[1] / 2048.0;
		accelDataIng[2] = (float) accelData[2] / 2048.0;
	}
}


/**
 * @brief Convert raw gyroscope values to physical units (degrees per second).
 *
 * @param gyroData Raw gyroscope array [X, Y, Z]
 * @param FS_SEL Gyroscope range setting used at init
 * @param gyroDataIns Output array [X, Y, Z] in dps (float)
 */
void MPU6050_getGyroIns(int16_t *gyroData, uint8_t FS_SEL, float *gyroDataIns)
{
	if(FS_SEL == 0x00){
		gyroDataIns[0] = (float) gyroData[0] / 131.0;
		gyroDataIns[1] = (float) gyroData[1] / 131.0;
		gyroDataIns[2] = (float) gyroData[2] / 131.0;
	}
	else if(FS_SEL == 0x01){
		gyroDataIns[0] = (float) gyroData[0] / 65.5;
		gyroDataIns[1] = (float) gyroData[1] / 65.5;
		gyroDataIns[2] = (float) gyroData[2] / 65.5;
	}
	else if(FS_SEL == 0x02){
		gyroDataIns[0] = (float) gyroData[0] / 32.8;
		gyroDataIns[1] = (float) gyroData[1] / 32.8;
		gyroDataIns[2] = (float) gyroData[2] / 32.8;
	}
	else if(FS_SEL == 0x03){
		gyroDataIns[0] = (float) gyroData[0] / 16.4;
		gyroDataIns[1] = (float) gyroData[1] / 16.4;
		gyroDataIns[2] = (float) gyroData[2] / 16.4;
	}
}