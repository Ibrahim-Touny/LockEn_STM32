#pragma once
#include "cmsis_os.h"

extern const osThreadAttr_t fpTask_attr;
void FpTask(void *argument);
