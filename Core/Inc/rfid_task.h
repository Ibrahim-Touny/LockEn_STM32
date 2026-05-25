#pragma once
#include "cmsis_os.h"

extern const osThreadAttr_t rfidTask_attr;
void RfidTask(void *argument);
